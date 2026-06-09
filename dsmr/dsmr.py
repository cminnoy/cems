#!/usr/bin/python3

import sys
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

import serial
import re
import os
import random
import signal
import argparse
import sys
import time
from datetime import datetime, timezone, timedelta
import pytz
from typing import Dict, Any
import fabrix
from fabrix import rcu
import flatbuffers

from CEMS.DSMR import DSMRData, Instant, Energy, NaturalGas, PeakConsumption

# ================== SIGNAL HANDLING ==================

stop = False
def interrupt_handler(signum, frame):
    global stop
    stop = True

# ====================== AUXILARY =====================

def dsmr_to_epoch(ts):
    if isinstance(ts, tuple):
        timestamp, dst = ts
    else:
        timestamp = ts
        dst = None

    dt = datetime.strptime(timestamp, "%y%m%d%H%M%S")

    if dst == 'S':
        tz = timezone(timedelta(hours=2))
    elif dst == 'W':
        tz = timezone(timedelta(hours=1))
    else:
        tz = timezone.utc

    dt = dt.replace(tzinfo=tz)
    return int(dt.timestamp())

class DSMRReader:
    """Class to read and parse DSMR data from a serial port."""

    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.obis_map = {
            # Power measurements
            '1-0:1.7.0': 'Totaal Afname (kW)',
            '1-0:2.7.0': 'Totaal Injectie (kW)',
            '1-0:21.7.0': 'Vermogen L1 Afname (kW)',
            '1-0:41.7.0': 'Vermogen L2 Afname (kW)',
            '1-0:61.7.0': 'Vermogen L3 Afname (kW)',
            '1-0:22.7.0': 'Vermogen L1 Injectie (kW)',
            '1-0:42.7.0': 'Vermogen L2 Injectie (kW)',
            '1-0:62.7.0': 'Vermogen L3 Injectie (kW)',

            # Voltage measurements
            '1-0:32.7.0': 'Spanning L1 (V)',
            '1-0:52.7.0': 'Spanning L2 (V)',
            '1-0:72.7.0': 'Spanning L3 (V)',

            # Current measurements
            '1-0:31.7.0': 'Stroom L1 (A)',
            '1-0:51.7.0': 'Stroom L2 (A)',
            '1-0:71.7.0': 'Stroom L3 (A)',

            # Energy counters
            '1-0:1.8.1': 'Afname Dagtarief (kWh)',
            '1-0:1.8.2': 'Afname Nachttarief (kWh)',
            '1-0:2.8.1': 'Injectie Dagtarief (kWh)',
            '1-0:2.8.2': 'Injectie Nachttarief (kWh)',

            # Capacity tariff / quarter-hour data
            '1-0:1.4.0': 'Huidig Kwartiergemiddelde (kW)',
            '1-0:1.6.0': 'Maandpiek Lopende Maand (kW)',
            '0-0:98.1.0': 'Historische Maandpieken',

            # Meter information
            '0-0:1.0.0': 'Meter Tijdstip',
            '0-0:96.14.0': 'Tariefindicator',
            '0-0:96.3.10': 'Schakelstatus Meter',
            '0-0:96.13.0': 'Tekstbericht Meter',
            '0-0:96.1.1': 'Elektriciteitsmeter Serienummer',
            '0-0:96.1.4': 'Meter Configuratiecode',

            # Limits
            '0-0:17.0.0': 'Vermogenslimiet (kW)',
            '1-0:31.4.0': 'Stroomlimiet (A)',

            # Gas meter information
            '0-1:24.1.0': 'Gastype',
            '0-1:24.4.0': 'Gas Klepstatus',
            '0-1:96.1.1': 'Gasmeter Serienummer',

            # Gas
            '0-1:24.2.3': 'Gasstand (m3)',
        }

    def parse_line(self, line: str) -> Dict[str, Any]:
        """Parse a single DSMR line into value and unit."""
        matches = re.findall(r'\((.*?)\)', line)
        if not matches:
            return None

        # Extract value and unit from the last match
        val_part = matches[-1]
        if '*' in val_part:
            value, unit = val_part.split('*')
            return {'value': value, 'unit': unit}
        return {'value': val_part, 'unit': ''}

    def parse_history(self, line):
        tokens = re.findall(r'\(([^)]+)\)', line)

        records = []
        i = 0

        while i < len(tokens):
            t = tokens[i]

            # timestamp: YYMMDDHHMMSS + S/W
            if len(t) == 13 and t[:12].isdigit() and t[12] in "SW":
                ts = (t[:12], t[12])

                # next token should be peak
                if i + 1 < len(tokens) and '*kW' in tokens[i + 1]:
                    peak = tokens[i + 1].split('*')[0]

                    records.append({
                        "timestamp": ts,
                        "peak": peak
                    })

                    i += 2
                    continue

            i += 1

        return records

    def read_data(self) -> Dict[str, Any]:
        """Read and parse DSMR data from the serial port."""
        data = {}
        try:
            with serial.Serial(self.port, self.baudrate, timeout=10) as ser:
                while True:
                    line = ser.readline().decode('ascii', errors='ignore').strip()

                    # Parse historical data
                    if '0-0:98.1.0' in line:
                        data['history'] = self.parse_history(line)
                        continue

                    # Parse regular data
                    for code, label in self.obis_map.items():
                        if code in line:
                            parsed = self.parse_line(line)
                            if parsed:
                                data[label] = parsed

                    # End of telegram
                    if line.startswith('!'):
                        break

        except Exception as e:
            print(f"DSMR reading error: {e}")

        return data

# ===================== COMPONENT =====================

class DSMRComponent(fabrix.Component):
    """Component to read DSMR data from a serial port and publish it via Fabrix IPC."""

    _AREA_NAME_DSMR = "DSMRData"
    _TOPIC_NAME_DSMR_DATA = "DSMRData"

    def __init__(self, *args, **kwargs):
        self._serial_port = kwargs.pop("serial_port", "/dev/ttyUSB0")
        self._verbose = kwargs.pop("verbose", False)
        super().__init__(*args, **kwargs)

        self.dsmr_reader = DSMRReader(self._serial_port)
        self.dsmr_data = {}

    def run(self):
        """Main loop."""
        timestep = 1.0
        next_timepoint = time.time() + timestep
        self.process()
        while not stop:
            self._act()
            self.process_until(next_timepoint)
            while next_timepoint <= time.time(): next_timepoint += timestep

    def _on_error(self, other_end, error_code):
        print(f"Error: {other_end.identifier().name() if other_end else '<>'} with error code {fabrix.EnumNameErrorCode(error_code)}")

    def _on_start(self):
        print(f"Component {self.identifier().name()} is online with pid {os.getpid()}.")

        # Create data area
        self.data_area = rcu.create_area(self.public_endpoint(), self._AREA_NAME_DSMR)
        # Set grace to 1 minute; we consider after that period data may be reclaimed.
        self.data_area.grace_period(60)

    def _on_subscribe_request(self, sender_endpoint, delivery_endpoint, topic_name):
        print(f"Received subscribe request from '{sender_endpoint.identifier().name()}' for topic '{topic_name}'")
        if topic_name == self._TOPIC_NAME_DSMR_DATA: return True
        return False

    def _on_unsubscribe_request(self, sender_endpoint, delivery_endpoint, topic_name):
        print(f"Received unsubscribe request from '{sender_endpoint.identifier().name()}' for topic '{topic_name}'")
        return topic_name == self._TOPIC_NAME_DSMR_DATA

    def _act(self):
        # Read DSMR data
        new_dsmr_data = self.dsmr_reader.read_data()
        self.dsmr_data.update(new_dsmr_data)

        # Publish data via RCU
        if self.dsmr_data:
            builder = flatbuffers.Builder(1024)
            # Monthly peaks vector
            history_items = self.dsmr_data.get('history', {})
            if history_items:
                history_items.sort(key=lambda x: x['timestamp'][0], reverse=True)
                num_peaks = len(history_items)
                if num_peaks > 0:
                    DSMRData.StartMonthPeaksVector(builder, num_peaks)
                    for item in reversed(history_items):
                        PeakConsumption.CreatePeakConsumption(
                            builder,
                            dsmr_to_epoch(item['timestamp']),
                            float(item['peak'])
                        )
                    peaks_vector = builder.EndVector()
            else:
                peaks_vector = 0

            # Start the DSMRData table before adding slots
            DSMRData.Start(builder)
            # Natural gas
            natural_gas = NaturalGas.CreateNaturalGas(builder, float(self.dsmr_data.get('Gasstand (m3)', {}).get('value', float('nan'))))
            DSMRData.AddNaturalGas(builder, natural_gas)
            # Monthly peaks
            DSMRData.AddMonthPeaks(builder, peaks_vector)
            # Timestamp
            DSMRData.AddTimestamp(builder, time.time())
            # Instant electricity
            instant = Instant.CreateInstant(builder,
                                            float(self.dsmr_data.get('Vermogenslimiet (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Stroomlimiet (A)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Totaal Afname (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Totaal Injectie (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Huidig Kwartiergemiddelde (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Maandpiek Lopende Maand (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L1 Afname (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L1 Injectie (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Spanning L1 (V)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Stroom L1 (A)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L2 Afname (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L2 Injectie (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Spanning L2 (V)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Stroom L2 (A)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L3 Afname (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Vermogen L3 Injectie (kW)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Spanning L3 (V)', {}).get('value', float('nan'))),
                                            float(self.dsmr_data.get('Stroom L3 (A)', {}).get('value', float('nan')))
                                            )
            DSMRData.AddInstant(builder, instant)
            # Energy electricity
            energy = Energy.CreateEnergy(builder,
                                         float(self.dsmr_data.get('Afname Dagtarief (kWh)', {}).get('value', float('nan'))),
                                         float(self.dsmr_data.get('Afname Nachttarief (kWh)', {}).get('value', float('nan'))),
                                         float(self.dsmr_data.get('Injectie Dagtarief (kWh)', {}).get('value', float('nan'))),
                                         float(self.dsmr_data.get('Injectie Nachttarief (kWh)', {}).get('value', float('nan'))),
                                         )
            DSMRData.AddEnergy(builder, energy)
            dsmrdata = DSMRData.End(builder)
            builder.Finish(dsmrdata)
            storage = self.data_area.create_storage(builder.Output())
            self.data_area.publish_storage(storage)
            self.broadcast_topic(self._TOPIC_NAME_DSMR_DATA, builder.Output())
        else:
            self.data_area.publish_none()
        self.data_area.tick()
        self.data_area.reclaim()

def parse_args():
    parser = argparse.ArgumentParser(description="Zendure Control")

    parser.add_argument("-n", "--name", required=True,
                        help="Component name")

    parser.add_argument("-r", "--realm", default="cems",
                        help="Realm name (default: cems)")

    parser.add_argument("-s", "--serial-port", default="/dev/ttyUSB0",
                        help="Serial port to read DSMR data from (default: /dev/ttyUSB0)")

    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose logging")

    return parser.parse_args()

def main():
    args = parse_args()

    # Random seed
    random.seed()

    # Register 'break' handler
    signal.signal(signal.SIGINT, interrupt_handler)
    signal.signal(signal.SIGTERM, interrupt_handler)

    # Create component
    ctrl = DSMRComponent(
        args.name,
        args.realm,
        65535,
        serial_port=args.serial_port,
        verbose=args.verbose
    )

    # Loop
    ctrl.run()

    # Cleanup
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGTERM, signal.SIG_DFL)

if __name__ == "__main__":
    main()
    sys.exit()
