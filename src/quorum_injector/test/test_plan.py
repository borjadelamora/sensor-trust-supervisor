from numpy.random import default_rng
from quorum_injector.plan import is_active, is_within, plan_attack
from quorum_injector.scenario import (
    BENIGN,
    CameraBlackout,
    ChannelSpec,
    GpsFreeze,
    NoAttack,
    Scenario,
)

ONSET_S = 20.0
FREEZE = ChannelSpec(attack=GpsFreeze(), onset_s=ONSET_S, onset_jitter_s=2.0)
BLACKOUT = ChannelSpec(
    attack=CameraBlackout(intensity=0.0, duration_s=10.0), onset_s=26.0, onset_jitter_s=2.0
)


def make_scenario(seed: int = 7, **channels: ChannelSpec) -> Scenario:
    return Scenario(scenario_id="ATK-001", seed=seed, **channels)


def test_the_onset_is_inclusive():
    assert not is_active(ONSET_S - 0.1, ONSET_S)
    assert is_active(ONSET_S, ONSET_S)


# The window is half-open, so a blackout of 10 s corrupts 10 s of frames and not one more.
def test_the_window_excludes_its_end():
    assert not is_within(ONSET_S - 0.1, ONSET_S, 10.0)
    assert is_within(ONSET_S, ONSET_S, 10.0)
    assert not is_within(ONSET_S + 10.0, ONSET_S, 10.0)


def test_the_same_seed_plans_the_same_onset():
    scenario = make_scenario(gps=FREEZE)

    planned = plan_attack(scenario, default_rng(scenario.seed))

    assert planned == plan_attack(scenario, default_rng(scenario.seed))
    assert planned != plan_attack(scenario, default_rng(scenario.seed + 1))


# Every channel draws, attacked or not, so adding one cannot re-seed the others. Twenty runs of
# ATK-002 have to keep their onsets when a camera attack joins the file.
def test_adding_a_camera_attack_leaves_the_gps_onset_alone():
    gps_only = make_scenario(gps=FREEZE)
    dual = make_scenario(gps=FREEZE, camera=BLACKOUT)

    single_plan = plan_attack(gps_only, default_rng(gps_only.seed))
    dual_plan = plan_attack(dual, default_rng(dual.seed))

    assert dual_plan.gps == single_plan.gps
    assert dual_plan.camera.onset_s != BLACKOUT.onset_s


def test_an_unattacked_channel_plans_no_attack():
    scenario = make_scenario(gps=FREEZE)

    planned = plan_attack(scenario, default_rng(scenario.seed))

    assert isinstance(planned.flow.attack, NoAttack)
    assert planned.flow.onset_s == BENIGN.onset_s
