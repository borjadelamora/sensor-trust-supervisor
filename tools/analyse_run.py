import argparse
import csv
import itertools
import math
import re
import statistics
import sys
from pathlib import Path
from typing import Any

import rosbag2_py
from rclpy.serialization import deserialize_message
from rosidl_runtime_py.utilities import get_message

CHANNELS = {
    "gps": ("/fmu/out/vehicle_gps_position", "/quorum/vehicle_gps_position"),
    "flow_vel": ("/fmu/out/vehicle_optical_flow_vel", "/quorum/vehicle_optical_flow_vel"),
    "flow_raw": ("/fmu/out/sensor_optical_flow", "/quorum/sensor_optical_flow"),
    "range": ("/fmu/out/distance_sensor", "/quorum/distance_sensor"),
    "camera": ("/camera/flow_image", "/quorum/flow_image"),
}

FIELDS = [
    "run",
    "scenario",
    "channel",
    "clock",
    "matched",
    "corrupted",
    "first_corrupted_s",
    "last_corrupted_s",
    "metric",
    "value",
    "max_gap_s",
]

EARTH_RADIUS_M = 6_371_000.0

# The relay counts onsets from the first px4 stamp it saw, and says so before it corrupts anything
RELAY_ORIGIN = re.compile(r"onsets count from px4 stamp ([\d.]+) s")

# Below this a ratio is dividing by sensor noise rather than by a reading
NEGLIGIBLE = 1e-6
UNCHANGED = 1.0


def read_messages(bag: Path) -> dict[str, dict[int, Any]]:
    reader = rosbag2_py.SequentialReader()
    reader.open(
        rosbag2_py.StorageOptions(uri=str(bag), storage_id="sqlite3"),
        rosbag2_py.ConverterOptions("", ""),
    )
    types = {topic.name: topic.type for topic in reader.get_all_topics_and_types()}
    wanted = {name for pair in CHANNELS.values() for name in pair}
    messages: dict[str, dict[int, Any]] = {name: {} for name in wanted}

    # A stamp older than one already seen predates the clock sync, as it does in the relay
    latest: dict[str, int] = dict.fromkeys(wanted, 0)

    while reader.has_next():
        topic, data, _ = reader.read_next()
        if topic in wanted:
            msg = deserialize_message(data, get_message(types[topic]))
            stamp = stamp_us(msg)
            if stamp >= latest[topic]:
                latest[topic] = stamp
                messages[topic][stamp] = msg

    return messages


def stamp_us(msg: Any) -> int:
    if hasattr(msg, "timestamp"):
        return msg.timestamp

    return msg.header.stamp.sec * 1_000_000 + msg.header.stamp.nanosec // 1000


def differs(channel: str, raw: Any, twin: Any) -> bool:
    if channel == "gps":
        return (raw.latitude_deg, raw.longitude_deg, raw.vel_n_m_s) != (
            twin.latitude_deg,
            twin.longitude_deg,
            twin.vel_n_m_s,
        )
    if channel == "flow_vel":
        return list(raw.vel_ne) != list(twin.vel_ne)
    if channel == "flow_raw":
        return (raw.distance_m, list(raw.pixel_flow), raw.quality) != (
            twin.distance_m,
            list(twin.pixel_flow),
            twin.quality,
        )
    if channel == "range":
        return raw.current_distance != twin.current_distance

    return bytes(raw.data) != bytes(twin.data)


def measure(channel: str, pairs: list[tuple[Any, Any]]) -> tuple[str, float]:
    if not pairs:
        return ("", 0.0)

    # An episode can end well before the flight does, so the last pair is not the worst one
    if channel == "gps":
        return ("peak_separation_m", round(max(separation_m(r, t) for r, t in pairs), 4))

    if channel == "flow_vel":
        return (
            "vel_ratio",
            furthest_ratio(
                [
                    float(t.vel_ne[0]) / float(r.vel_ne[0])
                    for r, t in pairs
                    if abs(r.vel_ne[0]) > NEGLIGIBLE
                ]
            ),
        )

    # This airframe leaves distance_m out of the raw message, so the lens is all that moves here
    if channel == "flow_raw":
        return (
            "pixel_flow_ratio",
            furthest_ratio(
                [
                    float(t.pixel_flow[0]) / float(r.pixel_flow[0])
                    for r, t in pairs
                    if abs(r.pixel_flow[0]) > NEGLIGIBLE
                ]
            ),
        )

    if channel == "range":
        return (
            "range_ratio",
            furthest_ratio(
                [
                    float(t.current_distance) / float(r.current_distance)
                    for r, t in pairs
                    if abs(r.current_distance) > NEGLIGIBLE
                ]
            ),
        )

    # A flood ramps and then holds, and one drives the level down where the other drives it up
    levels = [(mean_level(t), mean_level(r)) for r, t in pairs]
    twin_level, _ = max(levels, key=lambda pair: abs(pair[0] - pair[1]))

    return ("flooded_level", round(twin_level, 2))


def mean_level(msg: Any) -> float:
    return statistics.mean(bytes(msg.data))


# The magnitude an attack reached, which a median over an unevenly sampled window does not give
def furthest_ratio(values: list[float]) -> float:
    if not values:
        return 0.0

    return round(max(values, key=lambda value: abs(value - UNCHANGED)), 4)


def separation_m(raw: Any, twin: Any) -> float:
    north_m = math.radians(twin.latitude_deg - raw.latitude_deg) * EARTH_RADIUS_M
    east_m = (
        math.radians(twin.longitude_deg - raw.longitude_deg)
        * EARTH_RADIUS_M
        * math.cos(math.radians(raw.latitude_deg))
    )

    return math.hypot(north_m, east_m)


def relay_origin_s(bag: Path) -> float:
    log = bag / "relay.log"

    if not log.exists():
        raise SystemExit(f"{bag}: no relay.log, so nothing says where the px4 clock started")

    origin = RELAY_ORIGIN.search(log.read_text())
    if origin is None:
        raise SystemExit(f"{bag}: relay.log never recorded the px4 clock origin")

    return float(origin.group(1))


def analyse(bag: Path) -> list[dict[str, Any]]:
    manifest = dict(
        line.split(": ", 1)
        for line in (bag / "manifest.txt").read_text().splitlines()
        if ": " in line
    )
    messages = read_messages(bag)
    px4_stamps = sorted(messages[CHANNELS["gps"][0]])
    if not px4_stamps:
        raise SystemExit(f"{bag}: no relayed fixes")

    max_gap_s = round(max(b - a for a, b in itertools.pairwise(px4_stamps)) / 1e6, 3)
    origin_s = relay_origin_s(bag)

    rows = []
    for channel, (raw_topic, twin_topic) in CHANNELS.items():
        raw, twin = messages[raw_topic], messages[twin_topic]
        matched = sorted(raw.keys() & twin.keys())
        if not matched:
            continue

        # Gazebo's image clock is neither the relay's epoch nor its rate, so frames count alone
        clock = "gz" if channel == "camera" else "px4"
        start_s = matched[0] / 1e6 if clock == "gz" else origin_s
        corrupted = [(raw[k], twin[k]) for k in matched if differs(channel, raw[k], twin[k])]
        elapsed = [stamp_us(t) / 1e6 - start_s for _, t in corrupted]
        metric, value = measure(channel, corrupted)

        rows.append(
            {
                "run": manifest["run"],
                "scenario": manifest["scenario"],
                "channel": channel,
                "clock": clock,
                "matched": len(matched),
                "corrupted": len(corrupted),
                "first_corrupted_s": round(elapsed[0], 3) if elapsed else "",
                "last_corrupted_s": round(elapsed[-1], 3) if elapsed else "",
                "metric": metric,
                "value": value,
                "max_gap_s": max_gap_s,
            }
        )

    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare each relayed twin against its source")
    parser.add_argument("bags", nargs="+", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    rows = [row for bag in args.bags for row in analyse(bag)]

    with args.out.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)

    for row in rows:
        print(
            f"{row['scenario']:<40} {row['channel']:<9} "
            f"{row['corrupted']:>5}/{row['matched']:<5} "
            f"{row['first_corrupted_s']!s:>7} {row['metric']} {row['value']}"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
