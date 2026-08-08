import pytest
from quorum_injector.topics import OUT_PREFIX, TARGET_PREFIX, relayed_topics, twin_name


def test_twin_replaces_the_px4_prefix():
    assert twin_name("/fmu/out/sensor_combined") == "/quorum/sensor_combined"


def test_twin_rejects_a_topic_outside_fmu_out():
    with pytest.raises(AssertionError):
        twin_name("/quorum/sensor_combined")


def test_every_relayed_topic_maps_across():
    for topic in relayed_topics():
        assert topic.source.startswith(OUT_PREFIX)
        assert topic.twin.startswith(TARGET_PREFIX)
