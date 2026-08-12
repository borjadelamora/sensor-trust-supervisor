import pytest
from quorum_injector.topics import TARGET_PREFIX, relayed_topics, twin_name


def test_twin_replaces_the_px4_prefix():
    assert twin_name("/fmu/out/sensor_combined") == "/quorum/sensor_combined"


def test_twin_rejects_an_already_relayed_topic():
    with pytest.raises(AssertionError):
        twin_name("/quorum/sensor_combined")


def test_every_relayed_topic_maps_across():
    for topic in relayed_topics():
        assert topic.twin.startswith(TARGET_PREFIX)


# Only the last segment names the twin, so a clash would silently drop one of the two.
def test_no_two_topics_share_a_twin():
    topics = relayed_topics()

    assert len({topic.twin for topic in topics}) == len(topics)
