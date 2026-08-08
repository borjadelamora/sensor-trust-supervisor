import copy

from px4_msgs.msg import SensorGps

# mid-square, once the climb is done. Hardcoded until scenario files exist.
ONSET_S = 20.0


def get_elapsed_s(timestamp_us: int, start_us: int) -> float:
    return (timestamp_us - start_us) / 1e6


def is_active(elapsed_s: float) -> bool:
    return elapsed_s >= ONSET_S


def freeze_gps(msg: SensorGps, held: SensorGps, elapsed_s: float) -> SensorGps:
    if not is_active(elapsed_s):
        return msg

    frozen_msg = copy.deepcopy(msg)

    frozen_msg.latitude_deg = held.latitude_deg
    frozen_msg.longitude_deg = held.longitude_deg
    frozen_msg.altitude_msl_m = held.altitude_msl_m
    frozen_msg.altitude_ellipsoid_m = held.altitude_ellipsoid_m

    frozen_msg.vel_m_s = 0.0
    frozen_msg.vel_n_m_s = 0.0
    frozen_msg.vel_e_m_s = 0.0
    frozen_msg.vel_d_m_s = 0.0

    return frozen_msg
