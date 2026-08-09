from dataclasses import dataclass, replace
from functools import partial
from pathlib import Path
from typing import Any

import rclpy
from numpy.random import default_rng
from px4_msgs.msg import SensorGps
from rclpy import qos
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.publisher import Publisher

from quorum_injector import attacks, topics
from quorum_injector.scenario import ScenarioError, load_scenario


@dataclass(frozen=True)
class GpsStream:
    start_us: int
    held_fix: SensorGps


class RelayNode(Node):
    def __init__(self) -> None:
        super().__init__("relay_node")

        self._stream: GpsStream | None = None
        self._onset_logged = False
        scenario_file = self.declare_parameter("scenario_file", "").value
        seed = self.declare_parameter("seed", -1).value

        self._plan: attacks.AttackPlan = self._plan_attack(scenario_file, seed)

        for topic in topics.relayed_topics():
            self._make_pair(topic)

    def _plan_attack(self, scenario_file: str, seed: int) -> attacks.AttackPlan:
        if not scenario_file:
            self.get_logger().info("no scenario file, relaying without an attack")
            return attacks.NO_ATTACK

        scenario = load_scenario(Path(scenario_file), None if seed < 0 else seed)
        plan = attacks.plan_attack(scenario, default_rng(scenario.seed))

        self.get_logger().info(
            f"{scenario.scenario_id}: {scenario.mode.value} at {plan.onset_s:.2f} s, "
            f"seed {scenario.seed}"
        )
        return plan

    def _make_pair(self, topic: topics.MirroredTopic) -> None:
        publisher = self.create_publisher(topic.msg_type, topic.twin, qos.qos_profile_sensor_data)

        if topic.msg_type is SensorGps:
            callback = partial(self._relay_gps, publisher=publisher)
        else:
            callback = partial(self._relay, publisher=publisher)

        self.create_subscription(
            topic.msg_type, topic.source, callback, qos.qos_profile_sensor_data
        )

    def _relay(self, msg: Any, publisher: Publisher) -> None:
        publisher.publish(msg)

    def _relay_gps(self, msg: SensorGps, publisher: Publisher) -> None:
        if self._stream is None:
            self._stream = GpsStream(start_us=msg.timestamp, held_fix=msg)
            publisher.publish(msg)
            return

        elapsed_s = attacks.get_elapsed_s(msg.timestamp, self._stream.start_us)
        relayed = attacks.apply_to_gps(msg, self._stream.held_fix, self._plan, elapsed_s)

        if not relayed.corrupted:
            self._stream = replace(self._stream, held_fix=msg)
        elif not self._onset_logged:
            self.get_logger().warning(f"corrupting the gps fix from {self._plan.onset_s:.2f} s")
            self._onset_logged = True

        publisher.publish(relayed.fix)


def main(args: list[str] | None = None) -> int:
    rclpy.init(args=args)
    relay = None

    try:
        relay = RelayNode()
        rclpy.spin(relay)
    except ScenarioError as err:
        rclpy.logging.get_logger("relay_node").fatal(str(err))
        return 1
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        if relay is not None:
            relay.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()

    return 0
