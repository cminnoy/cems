#!/usr/bin/env python3

import sys
import os
from pathlib import Path

def add_schema_path():
    # Determine starting directory
    if "__file__" in globals():
        start = Path(__file__).resolve().parent
    else:
        # interactive python
        start = Path.cwd()

    for p in [start] + list(start.parents):
        candidate = p / "build" / "schema"
        if candidate.exists():
            sys.path.insert(0, str(candidate))
            return candidate

    raise RuntimeError("Could not locate build/schema directory")

sys.path.append("/usr/lib/python3.8/site-packages")
add_schema_path()

import time
import signal
import select
import argparse
import flatbuffers
import fabrix

from CEMS.Zendure import Target

# ================== SIGNAL HANDLING ==================

stop = False
def interrupt_handler(signum, frame):
    global stop
    stop = True

class ZendureCLI(fabrix.Component):
    """Command Line Interface to send power targets for Zendure battery."""

    def __init__(self, *args, **kwargs):
        self._target_name = kwargs.pop("target_name")
        self._driver_endpoint = fabrix.Endpoint()
        super().__init__(*args, **kwargs)

    def run(self):
        """Main loop."""
        timestep = 0.1 # Faster timestep for responsive CLI
        next_timepoint = time.time() + timestep
        self.process()

        print(f"--- Zendure Interactive CLI ---")
        print(f"Targeting component: {self._target_name}")
        print("Enter a power value in Watts (e.g., -500 for charge, 1000 for discharge, 0 for standby).")
        print("Press Ctrl+C to exit.\n> ", end="", flush=True)

        while not stop:
            self._act()
            self.process_until(next_timepoint)
            while next_timepoint <= time.time(): 
                next_timepoint += timestep

    # ================== FABRIX ==================

    def _on_error(self, other_end, error_code):
        print(f"\nError: {other_end.identifier().name() if other_end else '<>'} with error code {fabrix.EnumNameErrorCode(error_code)}")

    def _on_start(self):
        """Only called once during start of the component."""
        print(f"Component {self.identifier().name()} is online with pid {os.getpid()}.")
        for name in self.list_components(True):
            if name == self._target_name and (endpoint := self._open_endpoint(name)).is_open():
                self._driver_endpoint = endpoint

    def _on_endpoint_create(self, name, is_private):
        """When a new known endpoint is created find RCU areas."""
        if is_private:
            return
        if name == self._target_name and (endpoint := self._open_endpoint(name)).is_open():
            self._driver_endpoint = endpoint

    def _on_endpoint_remove(self, name):
        """When an endpoint is removed, let the user know."""
        if name == self._target_name:
            self._close_endpoint(self._target_name)
            self._driver_endpoint = fabrix.Endpoint()

    def _act(self):
        """Check for keyboard input non-blockingly."""
        dr, _, _ = select.select([sys.stdin], [], [], 0.0)
        if dr:
            line = sys.stdin.readline().strip()
            if line:
                try:
                    power_w = int(line)
                    self._send_power_target(power_w)
                except ValueError:
                    print("\nInvalid input. Please enter an integer.")
            print("> ", end="", flush=True)

    def _send_power_target(self, power_w):
        """Builds the FlatBuffer and sends the command to the driver."""
        if not self._driver_endpoint:
            print("\nError: Driver endpoint not initialized.")
            return
        builder = flatbuffers.Builder(1024)
        offset = Target.CreateTarget(builder, power_w)
        builder.Finish(offset)
        if self.command(self._driver_endpoint, "set_target_power", 0, 0, builder.Output()):
            print(f"\n[Command Sent] set_target_power: {power_w} W")
        else:
            print(f"Failure delivering command!")

# ================== MAIN ==================

def parse_args():
    parser = argparse.ArgumentParser(description="Zendure Interactive CLI Tester")

    parser.add_argument("-n", "--name", default="zendure_cli",
                        help="Component name (default: zendure_cli)")

    parser.add_argument("-r", "--realm", default="cems",
                        help="Realm name (default: cems)")

    parser.add_argument("-t", "--target", required=True,
                        help="Name of the target Zendure Driver component to command")

    return parser.parse_args()

def main():
    args = parse_args()

    # Register 'break' handler
    signal.signal(signal.SIGINT, interrupt_handler)
    signal.signal(signal.SIGTERM, interrupt_handler)

    # Create component
    cli = ZendureCLI(
        args.name,
        args.realm,
        65535,
        target_name=args.target
    )

    # Loop
    cli.run()

    # Cleanup
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGTERM, signal.SIG_DFL)

if __name__ == "__main__":
    main()
    sys.exit()
