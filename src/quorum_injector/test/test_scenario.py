import dataclasses
from pathlib import Path
from typing import Any

import pytest
import yaml
from quorum_injector.scenario import AttackMode, ScenarioError, load_scenario

VALID_FIELDS = {
    "scenario_id": "ATK-001",
    "attack": "gps_freeze",
    "onset_s": 20.0,
    "onset_jitter_s": 2.0,
    "seed": 7,
}


def write_scenario(tmp_path: Path, fields: dict[str, Any]) -> Path:
    path = tmp_path / "scenario.yaml"
    path.write_text(yaml.safe_dump(fields))
    return path


def test_loads_a_valid_scenario(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, VALID_FIELDS))

    assert scenario.scenario_id == "ATK-001"
    assert scenario.mode is AttackMode.GPS_FREEZE
    assert scenario.onset_s == 20.0
    assert scenario.seed == 7


def test_seed_override_beats_the_file(tmp_path):
    path = write_scenario(tmp_path, VALID_FIELDS)

    assert load_scenario(path, seed_override=11).seed == 11


def test_scenario_is_frozen(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, VALID_FIELDS))

    with pytest.raises(dataclasses.FrozenInstanceError):
        scenario.onset_s = 1.0


def test_missing_key_is_rejected(tmp_path):
    fields = dict(VALID_FIELDS)
    del fields["onset_jitter_s"]

    with pytest.raises(ScenarioError, match="missing onset_jitter_s"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_key_is_rejected(tmp_path):
    fields = VALID_FIELDS | {"onset_sec": 9.0}

    with pytest.raises(ScenarioError, match="unrecognised onset_sec"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_attack_is_rejected(tmp_path):
    fields = VALID_FIELDS | {"attack": "gps_freze"}

    with pytest.raises(ScenarioError, match="not a known attack"):
        load_scenario(write_scenario(tmp_path, fields))


def test_negative_onset_is_rejected(tmp_path):
    fields = VALID_FIELDS | {"onset_s": -1.0}

    with pytest.raises(ScenarioError, match="cannot be negative"):
        load_scenario(write_scenario(tmp_path, fields))


def test_jitter_wider_than_the_onset_is_rejected(tmp_path):
    fields = VALID_FIELDS | {"onset_s": 1.0, "onset_jitter_s": 2.0}

    with pytest.raises(ScenarioError, match="wider than"):
        load_scenario(write_scenario(tmp_path, fields))


def test_absent_file_is_rejected(tmp_path):
    with pytest.raises(ScenarioError):
        load_scenario(tmp_path / "nowhere.yaml")
