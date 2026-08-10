import dataclasses
from pathlib import Path
from typing import Any

import pytest
import yaml
from quorum_injector.scenario import GpsDrift, GpsFreeze, GpsJump, ScenarioError, load_scenario

FREEZE_FIELDS = {
    "scenario_id": "ATK-001",
    "attack": "gps_freeze",
    "onset_s": 20.0,
    "onset_jitter_s": 2.0,
    "seed": 7,
}

DRIFT_FIELDS = FREEZE_FIELDS | {
    "scenario_id": "ATK-002",
    "attack": "gps_drift",
    "rate_mps": 1.0,
    "bearing_deg": 60.0,
    "ramp_s": 5.0,
}

JUMP_FIELDS = FREEZE_FIELDS | {
    "scenario_id": "ATK-003",
    "attack": "gps_jump",
    "magnitude_m": 50.0,
    "bearing_deg": 60.0,
}


def write_scenario(tmp_path: Path, fields: dict[str, Any]) -> Path:
    path = tmp_path / "scenario.yaml"
    path.write_text(yaml.safe_dump(fields))
    return path


def test_loads_a_valid_scenario(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, FREEZE_FIELDS))

    assert scenario.scenario_id == "ATK-001"
    assert scenario.attack == GpsFreeze()
    assert scenario.onset_s == 20.0
    assert scenario.seed == 7


@pytest.mark.parametrize(
    ("fields", "attack"),
    [
        (DRIFT_FIELDS, GpsDrift(rate_mps=1.0, bearing_deg=60.0, ramp_s=5.0)),
        (JUMP_FIELDS, GpsJump(magnitude_m=50.0, bearing_deg=60.0)),
    ],
)
def test_loads_the_attack_parameters(tmp_path, fields, attack):
    assert load_scenario(write_scenario(tmp_path, fields)).attack == attack


def test_seed_override_beats_the_file(tmp_path):
    path = write_scenario(tmp_path, FREEZE_FIELDS)

    assert load_scenario(path, seed_override=11).seed == 11


def test_scenario_is_frozen(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, FREEZE_FIELDS))

    with pytest.raises(dataclasses.FrozenInstanceError):
        scenario.onset_s = 1.0


def test_missing_key_is_rejected(tmp_path):
    fields = dict(FREEZE_FIELDS)
    del fields["onset_jitter_s"]

    with pytest.raises(ScenarioError, match="missing onset_jitter_s"):
        load_scenario(write_scenario(tmp_path, fields))


def test_missing_attack_parameter_is_rejected(tmp_path):
    fields = dict(DRIFT_FIELDS)
    del fields["ramp_s"]

    with pytest.raises(ScenarioError, match="missing ramp_s"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_key_is_rejected(tmp_path):
    fields = FREEZE_FIELDS | {"onset_sec": 9.0}

    with pytest.raises(ScenarioError, match="gps_freeze does not take onset_sec"):
        load_scenario(write_scenario(tmp_path, fields))


def test_another_attacks_parameter_is_rejected(tmp_path):
    fields = JUMP_FIELDS | {"ramp_s": 5.0}

    with pytest.raises(ScenarioError, match="gps_jump does not take ramp_s"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_attack_is_rejected(tmp_path):
    fields = FREEZE_FIELDS | {"attack": "gps_freze"}

    with pytest.raises(ScenarioError, match="not a known attack"):
        load_scenario(write_scenario(tmp_path, fields))


def test_negative_onset_is_rejected(tmp_path):
    fields = FREEZE_FIELDS | {"onset_s": -1.0}

    with pytest.raises(ScenarioError, match="cannot be negative"):
        load_scenario(write_scenario(tmp_path, fields))


def test_jitter_wider_than_the_onset_is_rejected(tmp_path):
    fields = FREEZE_FIELDS | {"onset_s": 1.0, "onset_jitter_s": 2.0}

    with pytest.raises(ScenarioError, match="wider than"):
        load_scenario(write_scenario(tmp_path, fields))


# A zero ramp steps the velocity at onset, which is what the drift is shaped to avoid.
def test_an_instant_ramp_is_rejected(tmp_path):
    fields = DRIFT_FIELDS | {"ramp_s": 0.0}

    with pytest.raises(ScenarioError, match="ramp_s is 0.0, it must be positive"):
        load_scenario(write_scenario(tmp_path, fields))


def test_absent_file_is_rejected(tmp_path):
    with pytest.raises(ScenarioError):
        load_scenario(tmp_path / "nowhere.yaml")
