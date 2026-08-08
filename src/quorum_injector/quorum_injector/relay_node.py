from functools import partial
from typing import Any

import rclpy
from rclpy import qos
from rclpy.node import Node
from rclpy.publisher import Publisher

from quorum_injector import topics


class RelayNode(Node):
    def __init__(self) -> None:
        super().__init__("relay_node")

        for topic in topics.relayed_topics():
            self._make_pair(topic)

    def _make_pair(self, topic: topics.MirroredTopic) -> None:
        publisher = self.create_publisher(topic.msg_type, topic.twin, qos.qos_profile_sensor_data)

        callback = partial(self._relay, publisher=publisher)

        self.create_subscription(
            topic.msg_type, topic.source, callback, qos.qos_profile_sensor_data
        )

    def _relay(self, msg: Any, publisher: Publisher) -> None:
        publisher.publish(msg)


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
