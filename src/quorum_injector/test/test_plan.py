from numpy.random import default_rng
from quorum_injector.plan import ChannelPlan, PlannedEpisode, plan_attack
from quorum_injector.profile import Step
from quorum_injector.scenario import (
    BENIGN,
    CameraBlackout,
    Episode,
    GpsFreeze,
    NoAttack,
    Scenario,
)

ONSET_S = 20.0
DURATION_S = 10.0
FREEZE = (Episode(GpsFreeze(), onset_s=ONSET_S, onset_jitter_s=2.0, duration_s=None),)
BLACKOUT = (
    Episode(
        CameraBlackout(intensity=1.0, profile=Step()),
        onset_s=26.0,
        onset_jitter_s=2.0,
        duration_s=DURATION_S,
    ),
)

OPEN_ENDED = ChannelPlan((PlannedEpisode(GpsFreeze(), ONSET_S, None),))
WINDOWED = ChannelPlan((PlannedEpisode(GpsFreeze(), ONSET_S, DURATION_S),))


def make_scenario(seed: int = 7, **channels: tuple[Episode, ...]) -> Scenario:
    return Scenario(scenario_id="ATK-001", seed=seed, **channels)


def test_the_onset_is_inclusive():
    assert OPEN_ENDED.active(ONSET_S - 0.1) is None
    assert OPEN_ENDED.active(ONSET_S) is not None


# The window is half-open, so an episode of 10 s corrupts 10 s of messages and not one more.
def test_the_window_excludes_its_end():
    assert WINDOWED.active(ONSET_S) is not None
    assert WINDOWED.active(ONSET_S + DURATION_S) is None
    assert OPEN_ENDED.active(ONSET_S + 10_000.0) is not None


# The gap between two episodes is what a recovery to trusted needs, so it has to be quiet.
def test_the_gap_between_episodes_is_clean():
    plan = ChannelPlan(WINDOWED.episodes + (PlannedEpisode(GpsFreeze(), 40.0, DURATION_S),))

    assert plan.active(ONSET_S).onset_s == ONSET_S
    assert plan.active(35.0) is None
    assert plan.active(41.0).onset_s == 40.0


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
    assert dual_plan.camera.episodes[0].onset_s != BLACKOUT[0].onset_s


def test_an_unattacked_channel_plans_no_attack():
    scenario = make_scenario(gps=FREEZE)

    planned = plan_attack(scenario, default_rng(scenario.seed))

    assert isinstance(planned.flow.episodes[0].attack, NoAttack)
    assert planned.flow.episodes[0].onset_s == BENIGN[0].onset_s
