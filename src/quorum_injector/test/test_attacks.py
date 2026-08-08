from px4_msgs.msg import SensorGps
from quorum_injector.attacks import ONSET_S, freeze_gps, get_elapsed_s, is_active


def make_fix(latitude_deg: float, north_speed_mps: float) -> SensorGps:
    msg = SensorGps()
    msg.latitude_deg = latitude_deg
    msg.vel_n_m_s = north_speed_mps
    msg.vel_m_s = north_speed_mps
    return msg


def test_elapsed_s():
    assert get_elapsed_s(21_500_000, 1_500_000) == 20.0


def test_onset_is_inclusive():
    assert not is_active(ONSET_S - 0.1)
    assert is_active(ONSET_S)


def test_passthrough_before_onset():
    msg = make_fix(41.5, 4.8)

    assert freeze_gps(msg, make_fix(41.0, 4.8), 1.0) is msg


def test_freeze_holds_the_fix():
    held = make_fix(41.0, 4.8)
    frozen = freeze_gps(make_fix(41.5, 4.8), held, ONSET_S)

    assert frozen.latitude_deg == held.latitude_deg
    assert frozen.vel_n_m_s == 0.0
    assert frozen.vel_m_s == 0.0
