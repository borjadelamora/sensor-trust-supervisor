from numpy.random import default_rng
from px4_msgs.msg import SensorGps
from quorum_injector.attacks import AttackPlan, apply_to_gps, get_elapsed_s, is_active, plan_attack
from quorum_injector.scenario import AttackMode, Scenario

ONSET_S = 20.0


def make_fix(latitude_deg: float, north_speed_mps: float) -> SensorGps:
    msg = SensorGps()
    msg.latitude_deg = latitude_deg
    msg.vel_n_m_s = north_speed_mps
    msg.vel_m_s = north_speed_mps
    return msg


def test_elapsed_s():
    assert get_elapsed_s(21_500_000, 1_500_000) == 20.0


def test_onset_is_inclusive():
    assert not is_active(ONSET_S - 0.1, ONSET_S)
    assert is_active(ONSET_S, ONSET_S)


def test_the_same_seed_plans_the_same_onset():
    scenario = Scenario(
        scenario_id="ATK-001",
        mode=AttackMode.GPS_FREEZE,
        onset_s=ONSET_S,
        onset_jitter_s=2.0,
        seed=7,
    )

    planned = plan_attack(scenario, default_rng(scenario.seed))

    assert planned == plan_attack(scenario, default_rng(scenario.seed))
    assert planned != plan_attack(scenario, default_rng(scenario.seed + 1))


def test_passthrough_before_onset():
    plan = AttackPlan(mode=AttackMode.GPS_FREEZE, onset_s=ONSET_S)
    msg = make_fix(41.5, 4.8)

    relayed = apply_to_gps(msg, make_fix(41.0, 4.8), plan, 1.0)

    assert not relayed.corrupted
    assert relayed.fix is msg


def test_freeze_holds_the_fix():
    plan = AttackPlan(mode=AttackMode.GPS_FREEZE, onset_s=ONSET_S)
    held = make_fix(41.0, 4.8)

    relayed = apply_to_gps(make_fix(41.5, 4.8), held, plan, ONSET_S)

    assert relayed.corrupted
    assert relayed.fix.latitude_deg == held.latitude_deg
    assert relayed.fix.vel_n_m_s == 0.0
    assert relayed.fix.vel_m_s == 0.0
