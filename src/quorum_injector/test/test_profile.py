import pytest
from quorum_injector.profile import Linear, Oscillating, Sigmoid, Step

RISE_S = 30.0
EASE_S = 5.0
AMPLITUDE_FRACTION = 0.2
PERIOD_S = 8.0

LINEAR = Linear(rise_s=RISE_S, ease_s=EASE_S)
SIGMOID = Sigmoid(rise_s=RISE_S)
OSCILLATING = Oscillating(
    rise_s=RISE_S, ease_s=EASE_S, amplitude_fraction=AMPLITUDE_FRACTION, period_s=PERIOD_S
)

SHAPED = [LINEAR, SIGMOID, OSCILLATING]
RISING = [LINEAR, SIGMOID]

# ATK-002 as a profile: 47 m at 1 m/s, eased on at both ends
DRIFT_OFFSET_M = 47.0
DRIFT_EASE_S = 2.5
DRIFT_AS_LINEAR = Linear(rise_s=DRIFT_OFFSET_M + DRIFT_EASE_S, ease_s=DRIFT_EASE_S)


def derivative_at(profile, since_onset_s: float, step_s: float = 1e-4) -> float:
    before = profile.evaluate(since_onset_s - step_s).fraction
    after = profile.evaluate(since_onset_s + step_s).fraction

    return (after - before) / (2 * step_s)


# The spoofed velocity is built from the rate, so it has to be the derivative of the offset
@pytest.mark.parametrize("profile", SHAPED)
@pytest.mark.parametrize("since_onset_s", [2.5, EASE_S, 12.0, RISE_S - EASE_S, RISE_S, 40.0])
def test_the_rate_is_the_derivative_of_the_fraction(profile, since_onset_s):
    assert profile.evaluate(since_onset_s).rate_per_s == pytest.approx(
        derivative_at(profile, since_onset_s), abs=1e-4
    )


# A synchronous spoofer takes over without a jump in position or velocity
@pytest.mark.parametrize("profile", SHAPED)
def test_a_shaped_profile_starts_from_rest(profile):
    progress = profile.evaluate(0.0)

    assert progress.fraction == 0.0
    assert progress.rate_per_s == pytest.approx(0.0, abs=1e-12)


@pytest.mark.parametrize("since_onset_s", [0.0, 1.0, 100.0])
def test_a_step_is_complete_and_still(since_onset_s):
    assert Step().evaluate(since_onset_s) == (1.0, 0.0)


@pytest.mark.parametrize("profile", RISING)
@pytest.mark.parametrize("since_onset_s", [RISE_S, RISE_S + 0.1, 300.0])
def test_a_rising_profile_holds_once_it_arrives(profile, since_onset_s):
    progress = profile.evaluate(since_onset_s)

    assert progress.fraction == pytest.approx(1.0)
    assert progress.rate_per_s == pytest.approx(0.0)


# The bias is the rate, so a rate that stops dead hands a detector the end of the attack for free
@pytest.mark.parametrize("profile", RISING)
def test_a_rising_profile_hands_back_without_a_step(profile):
    settling = profile.evaluate(RISE_S - 0.01).rate_per_s

    assert settling < 0.01 * profile.evaluate(RISE_S / 2.0).rate_per_s


# The wobble outlasts the rise, so a spoofer sitting on its target is still moving
def test_the_wobble_stays_within_its_amplitude_and_outlasts_the_rise():
    samples = [OSCILLATING.evaluate(index / 10.0) for index in range(600)]

    assert all(0.0 <= sample.fraction <= 1.0 + AMPLITUDE_FRACTION for sample in samples)
    assert any(sample.fraction > 1.0 for sample in samples)
    assert any(sample.rate_per_s < 0.0 for sample in samples)


# ATK-002 was flown and written up against the ramp-then-linear drift this profile replaces
@pytest.mark.parametrize(
    ("since_onset_s", "offset_m", "bias_mps"),
    [
        (0.0, 0.0, 0.0),
        (1.25, 0.3125, 0.5),
        (DRIFT_EASE_S, 1.25, 1.0),
        (12.0, 10.75, 1.0),
        (40.0, 38.75, 1.0),
        (48.5, 46.8, 0.4),
    ],
)
def test_the_linear_profile_reproduces_the_drift_it_replaces(since_onset_s, offset_m, bias_mps):
    progress = DRIFT_AS_LINEAR.evaluate(since_onset_s)

    assert DRIFT_OFFSET_M * progress.fraction == pytest.approx(offset_m)
    assert DRIFT_OFFSET_M * progress.rate_per_s == pytest.approx(bias_mps)


@pytest.mark.parametrize(
    ("profile", "fields", "message"),
    [
        (Linear, {"rise_s": 0.0, "ease_s": 1.0}, "rise_s is 0.0"),
        (Linear, {"rise_s": 10.0, "ease_s": 0.0}, "ease_s is 0.0"),
        (Linear, {"rise_s": 10.0, "ease_s": 11.0}, "the 10.0 rise has to hold two of them"),
        (Linear, {"rise_s": 10.0, "ease_s": 6.0}, "the 10.0 rise has to hold two of them"),
        (Sigmoid, {"rise_s": -1.0}, "rise_s is -1.0"),
        (
            Oscillating,
            {"rise_s": 10.0, "ease_s": 1.0, "amplitude_fraction": 1.0, "period_s": 2.0},
            "amplitude_fraction is 1.0",
        ),
        (
            Oscillating,
            {"rise_s": 10.0, "ease_s": 1.0, "amplitude_fraction": 0.2, "period_s": 0.0},
            "period_s is 0.0",
        ),
    ],
)
def test_a_profile_out_of_range_is_refused(profile, fields, message):
    with pytest.raises(ValueError, match=message):
        profile(**fields)
