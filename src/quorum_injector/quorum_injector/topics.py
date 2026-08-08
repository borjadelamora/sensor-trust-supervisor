from dataclasses import dataclass

from px4_msgs.msg import SensorCombined, SensorGps, SensorOpticalFlow, VehicleOpticalFlowVel

OUT_PREFIX = "/fmu/out/"
TARGET_PREFIX = "/quorum/"


def twin_name(source: str) -> str:
    assert source.startswith(OUT_PREFIX), f"Topic '{source}' lacks required prefix '{OUT_PREFIX}'"
    return TARGET_PREFIX + source.removeprefix(OUT_PREFIX)


@dataclass(frozen=True)
class MirroredTopic:
    msg_type: type
    source: str

    @property
    def twin(self) -> str:
        return twin_name(self.source)


def relayed_topics() -> tuple[MirroredTopic, ...]:
    return (
        MirroredTopic(SensorGps, OUT_PREFIX + "vehicle_gps_position"),
        MirroredTopic(SensorCombined, OUT_PREFIX + "sensor_combined"),
        MirroredTopic(SensorOpticalFlow, OUT_PREFIX + "sensor_optical_flow"),
        MirroredTopic(VehicleOpticalFlowVel, OUT_PREFIX + "vehicle_optical_flow_vel"),
    )
