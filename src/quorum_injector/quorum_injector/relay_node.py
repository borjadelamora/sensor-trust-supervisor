from collections.abc import Callable
from functools import partial
from pathlib import Path
from typing import Any

import rclpy
from numpy.random import default_rng
from px4_msgs.msg import SensorGps, VehicleOpticalFlowVel
from rclpy import qos
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.publisher import Publisher
from sensor_msgs.msg import Image

from quorum_injector import camera, clock, flow, gps, plan, topics
from quorum_injector.scenario import CHANNELS, NoAttack, ScenarioError, load_scenario

Handler = Callable[[Any, Publisher, float], None]


class RelayNode(Node):
    def __init__(self) -> None:
        super().__init__("relay_node")

        self._px4_clock = clock.Clock()
        self._held_fix: SensorGps | None = None
        self._announced: set[str] = set()
        self._handlers: dict[type, Handler] = {
            SensorGps: self._relay_gps,
            VehicleOpticalFlowVel: self._relay_flow,
            Image: self._relay_image,
        }
        scenario_file = self.declare_parameter("scenario_file", "").value
        seed = self.declare_parameter("seed", -1).value

        self._plan: plan.AttackPlan = self._plan_attack(scenario_file, seed)

        for topic in topics.relayed_topics():
            self._make_pair(topic)

    def _plan_attack(self, scenario_file: str, seed: int) -> plan.AttackPlan:
        if not scenario_file:
            self.get_logger().info("no scenario file, relaying without an attack")
            return plan.NO_ATTACK

        scenario = load_scenario(Path(scenario_file), None if seed < 0 else seed)
        attack_plan = plan.plan_attack(scenario, default_rng(scenario.seed))

        for channel in CHANNELS:
            channel_plan = getattr(attack_plan, channel)
            if isinstance(channel_plan.attack, NoAttack):
                continue

            self.get_logger().info(
                f"{scenario.scenario_id} [{channel}]: {channel_plan.attack} "
                f"at {channel_plan.onset_s:.2f} s, seed {scenario.seed}"
            )

        return attack_plan

    def _make_pair(self, topic: topics.MirroredTopic) -> None:
        publisher = self.create_publisher(topic.msg_type, topic.twin, qos.qos_profile_sensor_data)

        handler = self._handlers.get(topic.msg_type, self._relay)
        callback = partial(self._on_message, handler=handler, publisher=publisher)

        self.create_subscription(
            topic.msg_type, topic.source, callback, qos.qos_profile_sensor_data
        )

    def _on_message(self, msg: Any, handler: Handler, publisher: Publisher) -> None:
        # Frames carry no px4 stamp, so the last one times them.
        if not isinstance(msg, Image):
            self._mark_time(msg.timestamp)

        elapsed_s = self._px4_clock.elapsed_s
        if elapsed_s is None:
            publisher.publish(msg)
            return

        handler(msg, publisher, elapsed_s)

    def _mark_time(self, stamp_us: int) -> None:
        was_running = self._px4_clock.start_s is not None

        self._px4_clock.mark(stamp_us)

        if not was_running and self._px4_clock.start_s is not None:
            self.get_logger().info(f"onsets count from px4 stamp {self._px4_clock.start_s:.3f} s")

    def _relay(self, msg: Any, publisher: Publisher, elapsed_s: float) -> None:
        publisher.publish(msg)

    def _relay_gps(self, msg: SensorGps, publisher: Publisher, elapsed_s: float) -> None:
        if self._held_fix is None:
            self._held_fix = msg
            publisher.publish(msg)
            return

        relayed = gps.apply(msg, self._held_fix, self._plan.gps, elapsed_s)

        if not relayed.corrupted:
            self._held_fix = msg
        else:
            self._announce("gps", self._plan.gps.onset_s)

        publisher.publish(relayed.msg)

    def _relay_flow(
        self, msg: VehicleOpticalFlowVel, publisher: Publisher, elapsed_s: float
    ) -> None:
        relayed = flow.apply(msg, self._plan.flow, elapsed_s)

        if relayed.corrupted:
            self._announce("flow", self._plan.flow.onset_s)

        publisher.publish(relayed.msg)

    def _relay_image(self, msg: Image, publisher: Publisher, elapsed_s: float) -> None:
        relayed = camera.apply(msg, self._plan.camera, elapsed_s)

        if relayed.corrupted:
            self._announce("camera", self._plan.camera.onset_s)

        publisher.publish(relayed.msg)

    def _announce(self, channel: str, onset_s: float) -> None:
        if channel in self._announced:
            return

        self.get_logger().warning(f"corrupting the {channel} stream from {onset_s:.2f} s")
        self._announced.add(channel)


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
