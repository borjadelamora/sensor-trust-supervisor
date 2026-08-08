from functools import partial
from typing import Any

import rclpy
from px4_msgs.msg import SensorGps
from rclpy import qos
from rclpy.node import Node
from rclpy.publisher import Publisher

from quorum_injector import attacks, topics


class RelayNode(Node):
    def __init__(self) -> None:
        super().__init__("relay_node")

        self._start_us: int | None = None
        self._held_fix: SensorGps | None = None
        self._has_onset_log = False

        for topic in topics.relayed_topics():
            self._make_pair(topic)

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
        if self._start_us is None:
            self._start_us = msg.timestamp
            self._held_fix = msg

        elapsed_s = attacks.get_elapsed_s(msg.timestamp, self._start_us)

        if not attacks.is_active(elapsed_s):
            self._held_fix = msg
        elif not self._has_onset_log:
            self.get_logger().warning(f"holding the gps fix from {attacks.ONSET_S:.0f} s")
            self._has_onset_log = True

        publisher.publish(attacks.freeze_gps(msg, self._held_fix, elapsed_s))


def main(args: list[str] | None = None) -> None:
    rclpy.init(args=args)
    relay = RelayNode()

    try:
        rclpy.spin(relay)
    except KeyboardInterrupt:
        pass
    finally:
        relay.destroy_node()
        rclpy.shutdown()
