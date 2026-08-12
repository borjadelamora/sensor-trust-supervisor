import pytest
from quorum_injector.clock import Clock

STAMP_US = 1_786_539_422_252_097


def test_there_is_no_elapsed_time_before_the_first_stamp():
    assert Clock().elapsed_s is None


def test_the_first_stamp_is_the_zero():
    clock = Clock()

    clock.mark(STAMP_US)

    assert clock.elapsed_s == 0.0
    assert clock.start_s == STAMP_US / 1e6


def test_elapsed_time_follows_the_stamps():
    clock = Clock()

    clock.mark(STAMP_US)
    clock.mark(STAMP_US + 20_500_000)

    assert clock.elapsed_s == pytest.approx(20.5)


# Anchoring on an unset stamp would put every onset in the past and corrupt from the first message.
def test_an_unset_stamp_does_not_start_the_clock():
    clock = Clock()

    clock.mark(0)

    assert clock.elapsed_s is None


def test_an_unset_stamp_does_not_move_the_clock():
    clock = Clock()

    clock.mark(STAMP_US)
    clock.mark(0)

    assert clock.elapsed_s == 0.0


def test_a_stale_stamp_does_not_move_the_clock_back():
    clock = Clock()

    clock.mark(STAMP_US)
    clock.mark(STAMP_US + 20_500_000)
    clock.mark(STAMP_US + 1_000_000)

    assert clock.elapsed_s == pytest.approx(20.5)
