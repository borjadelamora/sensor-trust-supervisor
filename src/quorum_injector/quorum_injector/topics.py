from dataclasses import dataclass

from px4_msgs.msg import (
    DistanceSensor,
    SensorCombined,
    SensorGps,
    SensorOpticalFlow,
    VehicleAttitude,
    VehicleOpticalFlowVel,
)
from sensor_msgs.msg import Image

OUT_PREFIX = "/fmu/out/"
TARGET_PREFIX = "/quorum/"


def twin_name(source: str) -> str:
    assert not source.startswith(TARGET_PREFIX), (
        f"Topic '{source}' already under prefix '{TARGET_PREFIX}'"
    )
    return TARGET_PREFIX + source.rsplit("/", 1)[-1]


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
        MirroredTopic(VehicleAttitude, OUT_PREFIX + "vehicle_attitude"),
        MirroredTopic(DistanceSensor, OUT_PREFIX + "distance_sensor"),
        MirroredTopic(Image, "/camera/flow_image"),
    )
