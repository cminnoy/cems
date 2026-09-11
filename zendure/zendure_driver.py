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

import time
import signal
import requests
import flatbuffers
import fabrix
import random
import os
import argparse
from zendure_properties import *
from fabrix import rcu
from typing import Optional

from CEMS.Zendure import BatteryPack
from CEMS.Zendure import BatteryStatus
from CEMS.Zendure import Target

# ================== SIGNAL HANDLING ==================

stop = False
def interrupt_handler(signum, frame):
    global stop
    stop = True

# ===================== COMPONENT =====================

class ZendureDriver(fabrix.Component):
    """Component to interface with Zendure batteries via their local API and expose status/control via fabrix IPC."""

    # ================== CONFIGURATION ==================

    # Battery Specs
    MAX_HW_CHARGE = 2400          # AC charge cap
    MAX_HW_DISCHARGE = 2400       # AC Discharge cap

    # Dynamic SOC Target bounds
    SOC_MAX_HARD = 100            # Never charge above this (safety ceiling)
    SOC_MIN_HARD = 10             # Never discharge below this (safety floor)

    # Timers (Hysteresis)
    DELAY_STATE_CHANGE  = 14     # Seconds between AC mode switches
    DELAY_UPDATE_STATUS = 1      # Seconds between fetching reports from battery
    DELAY_POWER_UPDATE  = 2      # Seconds between power changes
    DELAY_STANDBY       = 30     # Seconds delay before going to standby when not receiving a new power update
    DELAY_LOW_POWER     = 120    # Two minute delay to go to low power

    def __init__(self, *args, **kwargs):
        self._zendure_ip = kwargs.pop("zendure_ip")
        self._zendure_sn = kwargs.pop("zendure_sn")
        self._verbose = kwargs.pop("verbose", False)
        self._debug = kwargs.pop("debug", False)
        super().__init__(*args, **kwargs)

        self._AREA_NAME_BATTERY_STATUS = "BatteryStatus"

        # State tracking
        self.report: Optional[SolarFlowReport] = None

        # Target Power
        self.target_power : int = 0    # Last target power received 

        # Logic timers
        now = time.time()
        self.last_state_change_ts = now   # Last time state transition of battery
        self.last_power_update_ts = now   # Last time we wrote a new power to battery
        self.standby_entry_ts = now       # Timestamp of entering standby mode (0 W power)
        self.low_power_step_ts = now      # Tracks the 2-step low power transition
        self._write({"smartMode": 0, "lampSwitch": 0, "acMode": 0, "inputLimit": 0, "outputLimit": 0})

    def run(self):
        """Main loop."""
        timestep = 1.0
        next_timepoint = time.time() + timestep
        self.process()
        while not stop:
            self._act()
            self.process_until(next_timepoint)
            while next_timepoint <= time.time(): next_timepoint += timestep
        self.cleanup()

    def _on_error(self, other_end, error_code):
        print(f"Error: {other_end.identifier().name() if other_end else '<>'} with error code {fabrix.EnumNameErrorCode(error_code)}")

    def _on_start(self):
        print(f"Component {self.identifier().name()} is online with pid {os.getpid()}.")

        # Create status area
        self.status_area = rcu.create_area(self.public_endpoint(), self._AREA_NAME_BATTERY_STATUS)
        # Set grace to 1 minute; we consider after that period data may be reclaimed.
        self.status_area.grace_period(60)

    def _on_command_request(self, sender_endpoint, delivery_endpoint, timestamp, command, priority, cr_identifier, data):
        """Handles incoming commands from the controller via fabrix IPC."""
        if command == "set_target_power":
            target = Target.Target()
            target.Init(data, 4)
            self.target_power = int(target.Power())
            if self.target_power != 0:
                now = time.time()
                self.standby_entry_ts = now
                self.low_power_step_ts = now
            return True
        return False

    def _on_list_commands_request(self, sender_endpoint, commands):
        commands.append("set_target_power")

    def _apply_power_target(self):
        """
        Translates the signed target power to Zendure hardware commands.
        power_w < 0: Charge from grid
        power_w > 0: Discharge to home
        power_w == 0: Standby
        """
        now = time.time()
        if now - self.standby_entry_ts > self.DELAY_STANDBY:
            self.target_power = 0
        power_w = self.target_power
        if power_w < 0:
            if self.report.properties.electric_level == self.SOC_MAX_HARD or self.report.properties.electric_level >= self.report.properties.soc_set:
                self._set_standby()
                if self._verbose:
                    print("Max SOC reached: STANDBY (0W)")
                return
            charge_power = min(abs(power_w), self.MAX_HW_CHARGE)
            self._set_hardware_charge(charge_power)

        elif power_w > 0:
            if self.report.properties.electric_level == self.SOC_MIN_HARD or self.report.properties.electric_level <= self.report.properties.min_soc:
                self._set_standby()
                if self._verbose:
                    print("Min SOC reached: STANDBY (0W)")
                return
            discharge_power = min(min(power_w, self.report.properties.charge_max_limit), self.MAX_HW_DISCHARGE)
            self._set_hardware_discharge(discharge_power)
        else:
            self._set_standby()

    def _set_bat_state_low_power(self):
        if self.report.properties.ac_status != 0 or self.report.properties.ac_mode != 0:
            now = time.time()
            if now >= (self.low_power_step_ts + self.DELAY_LOW_POWER) and self.report.properties.ac_mode != 0:
                self._write({"smartMode": 0, "acMode": 0, "lampSwitch": 0, "inputLimit": 0, "outputLimit": 0})
            if now >= (self.low_power_step_ts + self.DELAY_LOW_POWER - self.DELAY_STATE_CHANGE) and self.report.properties.ac_status != 0 and \
               self.report.properties.ac_mode != 0 and self.report.properties.ac_mode != 1:
                self._write({"smartMode": 1, "acMode": 1, "lampSwitch": 0, "inputLimit": 0, "outputLimit": 0})

    def _set_standby(self):
        """Set to zero power. Keep mode."""
        now = time.time()
        if now <= self.last_state_change_ts + self.DELAY_STATE_CHANGE: return
        if now >= self.last_power_update_ts + self.DELAY_POWER_UPDATE:
            if self.report.properties.input_limit != 0 or self.report.properties.output_limit != 0:
                self.last_power_update_ts = now
                self.last_state_change_ts = now
                self.standby_entry_ts = now
                self.low_power_step_ts = now
                self._write({"smartMode": 1, "lampSwitch": 1, "inputLimit": 0, "outputLimit": 0})
            elif now >= self.standby_entry_ts + self.DELAY_STANDBY:
                self._set_bat_state_low_power()

    def _set_hardware_charge(self, power):
        """Set charge mode and power."""
        now = time.time()
        if self.report.properties.ac_mode != 1 and now <= self.last_state_change_ts + self.DELAY_STATE_CHANGE: return
        if now >= self.last_power_update_ts + self.DELAY_POWER_UPDATE:
            do_write = False
            if self.report.properties.ac_mode != 1:
                self.last_state_change_ts = now
                do_write = True
            if power != self.report.properties.input_limit:
                do_write = True
            self.last_power_update_ts = now
            if do_write:
                self._write({"smartMode": 1, "acMode": 1, "lampSwitch": 1, "inputLimit": power, "outputLimit": 0})
                if self._verbose:
                        print(f"Target set: CHARGE at {power} W")

    def _set_hardware_discharge(self, power):
        """Hardware wrapper for the Zendure local API discharge limit."""
        now = time.time()
        if self.report.properties.ac_mode != 2 and now <= self.last_state_change_ts + self.DELAY_STATE_CHANGE: return
        if now >= self.last_power_update_ts + self.DELAY_POWER_UPDATE:
            do_write = False
            if self.report.properties.ac_mode != 2:
                self.last_state_change_ts = now
                do_write = True
            if power != self.report.properties.output_limit:
                do_write = True
            self.last_power_update_ts = now
            if do_write:
                self._write({"smartMode": 1, "acMode": 2, "lampSwitch": 1, "inputLimit": 0, "outputLimit": power})
                if self._verbose:
                    print(f"Target set: DISCHARGE at {power} W")

    def _act(self):
        now = time.time()
        if self.report == None or now - self.report.timestamp >= self.DELAY_UPDATE_STATUS:
            if not self._update_status():
                self.report = None
        if self.report != None and self.report.properties.data_ready == 1:
            self._apply_power_target()

            properties = self.report.properties
            current_timestamp = float(properties.timestamp)
            builder = flatbuffers.Builder(1024)

            # 1. Pre-build all strings and tables for child packs to collect offsets
            pack_offsets = []
            for pack in self.report.pack_data:
                # Add string values first
                sn_offset = builder.CreateString(pack.sn)

                # Construct BatteryPack table
                BatteryPack.Start(builder)
                BatteryPack.AddSerialNumber(builder, sn_offset)
                BatteryPack.AddPackType(builder, pack.pack_type)
                BatteryPack.AddStateOfCharge(builder, pack.soc_level)
                BatteryPack.AddState(builder, pack.state)
                BatteryPack.AddPower(builder, pack.power)
                BatteryPack.AddMaxTemperature(builder, pack.max_temperature)
                BatteryPack.AddTotalVoltage(builder, pack.total_voltage)
                BatteryPack.AddBatteryCurrent(builder, pack.battery_current)
                BatteryPack.AddMaxCellVoltage(builder, pack.max_voltage)
                BatteryPack.AddMinCellVoltage(builder, pack.min_voltage)
                BatteryPack.AddSoftVersion(builder, pack.soft_version)
                BatteryPack.AddHeatState(builder, bool(pack.heat_state))

                pack_offset = BatteryPack.End(builder)
                pack_offsets.append(pack_offset)

            # 2. Build the packs vector BEFORE starting the parent BatteryStatus table
            BatteryStatus.StartPacksVector(builder, len(pack_offsets))
            # FlatBuffers vectors should be serialized in reverse order
            for offset in reversed(pack_offsets):
                builder.PrependUOffsetTRelative(offset)
            packs_vector_offset = builder.EndVector()

            # 3. Create parent-level strings and reference offsets
            product_off = builder.CreateString(self.report.product)
            serial_off = builder.CreateString(self.report.sn)
            tz_offset = builder.CreateString(properties.time_zone_str)

            # 4. Assemble top-level parent table (BatteryStatus)
            BatteryStatus.Start(builder)
            BatteryStatus.AddTimestamp(builder, current_timestamp)
            BatteryStatus.AddProduct(builder, product_off)
            BatteryStatus.AddSerialNumber(builder, serial_off)
            BatteryStatus.AddVersion(builder, self.report.version)

            BatteryStatus.AddTemperature(builder, properties.hyper_temperature)
            BatteryStatus.AddIsError(builder, bool(properties.is_error))
            BatteryStatus.AddFaultLevel(builder, properties.fault_level)
            BatteryStatus.AddDataReady(builder, bool(properties.data_ready))
            BatteryStatus.AddWriteResponse(builder, bool(properties.write_rsp))
            BatteryStatus.AddRssi(builder, properties.rssi)

            BatteryStatus.AddTargetPower(builder, self.target_power)
            BatteryStatus.AddInputPackPower(builder, properties.input_pack)
            BatteryStatus.AddOutputPackPower(builder, properties.output_pack)
            BatteryStatus.AddOutputHomePower(builder, properties.output_home)
            BatteryStatus.AddSolarTotalPower(builder, properties.solar_total)
            BatteryStatus.AddGridInputPower(builder, properties.grid_input_power)
            BatteryStatus.AddGridOffPower(builder, properties.grid_off_power)

            BatteryStatus.AddSolarPower1(builder, properties.pv1)
            BatteryStatus.AddSolarPower2(builder, properties.pv2)
            BatteryStatus.AddSolarPower3(builder, properties.pv3)
            BatteryStatus.AddSolarPower4(builder, properties.pv4)
            BatteryStatus.AddSolarPower5(builder, properties.pv5)
            BatteryStatus.AddSolarPower6(builder, properties.pv6)

            BatteryStatus.AddStateOfCharge(builder, properties.electric_level)
            BatteryStatus.AddTargetStateOfCharge(builder, properties.soc_set)
            BatteryStatus.AddMinStateOfCharge(builder, properties.min_soc)
            BatteryStatus.AddRemainOutTime(builder, properties.remain_outtime)
            BatteryStatus.AddPackState(builder, properties.pack_state)
            BatteryStatus.AddSocLimitReached(builder, properties.soc_limit)
            BatteryStatus.AddChargeMaxLimit(builder, properties.charge_max_limit)
            BatteryStatus.AddInputLimit(builder, properties.input_limit)
            BatteryStatus.AddOutputLimit(builder, properties.output_limit)

            BatteryStatus.AddHeating(builder, bool(properties.heat_state))
            BatteryStatus.AddPassthrough(builder, bool(properties.pass_prop))
            BatteryStatus.AddReverseFlow(builder, bool(properties.reverse_state))
            BatteryStatus.AddCalibrating(builder, bool(properties.soc_status))
            BatteryStatus.AddAcStatus(builder, properties.ac_status)
            BatteryStatus.AddDcStatus(builder, properties.dc_status)
            BatteryStatus.AddPvStatus(builder, properties.pv_status)
            BatteryStatus.AddAcMode(builder, properties.ac_mode)
            BatteryStatus.AddGridState(builder, bool(properties.grid_state))
            BatteryStatus.AddSmartMode(builder, bool(properties.smart_mode))
            BatteryStatus.AddAcCouplingState(builder, properties.ac_coupling_state)
            BatteryStatus.AddDryNodeState(builder, properties.dry_node_state)
            BatteryStatus.AddBatteryVolt(builder, properties.battery_voltage)
            BatteryStatus.AddFmVolt(builder, properties.fm_volt)
            BatteryStatus.AddTimeZone(builder, tz_offset)

            BatteryStatus.AddFanSwitch(builder, bool(properties.fan_switch))
            BatteryStatus.AddFanSpeed(builder, properties.fan_speed)
            BatteryStatus.AddLampSwitch(builder, bool(properties.lamp_switch))

            BatteryStatus.AddGridStandard(builder, properties.grid_standard)
            BatteryStatus.AddGridReverseAllowed(builder, properties.grid_reverse)
            BatteryStatus.AddGridOffMode(builder, properties.grid_off_mode)
            BatteryStatus.AddIotState(builder, properties.iot_state)
            BatteryStatus.AddOtaState(builder, properties.ota_state)
            BatteryStatus.AddBindState(builder, properties.bind_state)
            BatteryStatus.AddVoltWakeup(builder, properties.volt_wakeup)
            BatteryStatus.AddBatteryCalibrationTime(builder, properties.bat_cal_time)

            BatteryStatus.AddCountdownPowerUpdate(builder, int(max(self.DELAY_POWER_UPDATE - (time.time() - self.last_power_update_ts), 0)))
            BatteryStatus.AddCountdownStateChange(builder, int(max(self.DELAY_STATE_CHANGE - (time.time() - self.last_state_change_ts), 0)))
            BatteryStatus.AddCountdownStandby(builder, int(max(self.DELAY_STANDBY - (time.time() - self.standby_entry_ts), 0)))
            BatteryStatus.AddCountdownLowPower(builder, int(max(self.DELAY_LOW_POWER - (time.time() - self.low_power_step_ts), 0)))

            # Link pre-built vector offset to the parent object field
            BatteryStatus.AddPacks(builder, packs_vector_offset)

            offset = BatteryStatus.End(builder)
            builder.Finish(offset)

            storage = self.status_area.create_storage(builder.Output())
            self.status_area.publish_storage(storage)
        else:
            self.status_area.publish_none()
        self.status_area.tick()
        self.status_area.reclaim()

    def cleanup(self):
        if self.report and (self.report.properties.ac_status != 0 or self.report.properties.ac_mode != 0):
            self._write({"smartMode": 1, "lampSwitch": 0, "acMode": 1, "inputLimit": 0, "outputLimit": 0})
            self.process_period(self.DELAY_STATE_CHANGE)
            self._write({"smartMode": 0, "lampSwitch": 0, "acMode": 0, "inputLimit": 0, "outputLimit": 0})

    def _update_status(self):
        try:
            result = requests.get(
                f"http://{self._zendure_ip}/properties/report",
                timeout=3
            )
            self.report = SolarFlowReport.model_validate_json(result.content)
            return True
        except Exception as e:
            print(e, file=sys.stderr)
            return False

    def _write(self, props):
        """
        Optimized write: Updates internal cache and sends request.
        """
        try:
            requests.post(
                f"http://{self._zendure_ip}/properties/write",
                json={"sn": self._zendure_sn, "properties": props},
                timeout=3
            )
            if self._debug:
                print(f"WRITE >> {props}")
        except Exception as e:
            if self._debug:
                print(f"Write failed: {e}")

# ================== MAIN ==================

def parse_args():
    parser = argparse.ArgumentParser(description="Zendure Control")

    parser.add_argument("-n", "--name", required=True,
                        help="Component name")

    parser.add_argument("-r", "--realm", default="cems",
                        help="Realm name (default: cems)")

    parser.add_argument("--ip", required=True,
                        help="Zendure device IP")

    parser.add_argument("--sn", required=True,
                        help="Zendure serial number")

    parser.add_argument("--verbose", action="store_true",
                        help="Enable verbose logging")

    parser.add_argument("--debug", action="store_true",
                        help="Enable debug logging")

    return parser.parse_args()

def main():
    args = parse_args()

    # Random seed
    random.seed()

    # Register 'break' handler
    signal.signal(signal.SIGINT, interrupt_handler)
    signal.signal(signal.SIGTERM, interrupt_handler)

    # Create component
    ctrl = ZendureDriver(
        args.name,
        args.realm,
        65535,
        zendure_ip=args.ip,
        zendure_sn=args.sn,
        verbose=args.verbose,
        debug=args.debug,
    )

    # Loop
    ctrl.run()

    # Cleanup
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGTERM, signal.SIG_DFL)

if __name__ == "__main__":
    main()
    sys.exit()
