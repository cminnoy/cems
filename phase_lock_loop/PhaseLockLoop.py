import time

class PhaseLockLoop:
    """Phase Lock Loop for synchronizing with master clock."""

    def __init__(self, frequency_hz):
        """Initialize PLL with nominal frequency."""
        self.nominal_frequency_hz = frequency_hz
        self.period_s = 1.0 / frequency_hz
        self.next_tick = time.time()
        self.lead_time_s = 0.1  # Default 100ms lead time
        self.adjustment_s = 0.0

    def synchronize(self, master_timestamp_s):
        """Align internal timeline with the master clock."""
        raw_error_s = master_timestamp_s - self.next_tick

        # Shortest-path phase wrapping: Error in [-P/2, P/2]
        wrapped_error_s = raw_error_s % self.period_s
        if wrapped_error_s > self.period_s / 2:
            wrapped_error_s -= self.period_s

        # Proportional steering
        self.adjustment_s = wrapped_error_s / 10.0

    def set_lead_time(self, lead_time_s):
        """Set the lead time with 1.2x scaling."""
        self.lead_time_s = lead_time_s * 1.2

    def advance(self):
        """Advance to next tick."""
        self.next_tick += self.period_s + self.adjustment_s
        self.adjustment_s = 0.0

    def frequency(self):
        """Get nominal frequency."""
        return self.nominal_frequency_hz

    def at_tick(self):
        """Get time of next tick."""
        return self.next_tick

    def pre_tick(self):
        """Get time of pre-tick (before lead time)."""
        return self.next_tick - self.lead_time_s

    def should_tick(self):
        """Check if current time is at or past the next tick."""
        return time.time() >= self.next_tick