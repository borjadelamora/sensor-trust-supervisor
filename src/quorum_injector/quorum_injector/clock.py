UNSET_STAMP_US = 0


class Clock:
    """Elapsed seconds on px4's clock, counted from the first stamp given to it."""

    def __init__(self) -> None:
        self._start_s: float | None = None
        self._now_s = 0.0

    @property
    def start_s(self) -> float | None:
        return self._start_s

    @property
    def elapsed_s(self) -> float | None:
        if self._start_s is None:
            return None

        return self._now_s - self._start_s

    def mark(self, stamp_us: int) -> None:
        # px4 leaves a stamp it has not filled in at zero.
        if stamp_us == UNSET_STAMP_US:
            return

        # A message from a dead session arrives stamped from before this run.
        self._now_s = max(self._now_s, stamp_us / 1e6)

        if self._start_s is None:
            self._start_s = self._now_s
