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

import time
import sys
import os
import signal
import random
import argparse
import fabrix
import curses
from datetime import datetime, timezone
from fabrix import rcu
import CEMS.Zendure.ControlStatus

from CEMS.MasterClock.ClockTick import ClockTick as MasterClockTick
from CEMS.Elkor.InstantReading import InstantReading as ElkorInstantReading
from CEMS.Elkor.EnergyReading import EnergyReading as ElkorEnergyReading
from CEMS.IME.InstantReading import InstantReading as IMEInstantReading
from CEMS.IME.EnergyReading import EnergyReading as IMEEnergyReading
from CEMS.IME.Sector import Sector as IMESector
from CEMS.Circutor.InstantReading import InstantReading as EVMeterInstantReading
from CEMS.Circutor.EnergyReading import EnergyReading as EVMeterEnergyReading
from CEMS.Zendure.BatteryStatus import BatteryStatus
from CEMS.Zendure.BatteryState import BatteryState
from CEMS.Zendure.PackState import PackState
from CEMS.Zendure.ACStatus import ACStatus
from CEMS.Zendure.DCStatus import DCStatus
from CEMS.Zendure.PVStatus import PVStatus
from CEMS.Zendure.ACMode import ACMode
from CEMS.Zendure.GridStandard import GridStandard
from CEMS.Zendure.OffGridMode import OffGridMode
from CEMS.Zendure.FanMode import FanMode
from CEMS.Zendure.FanSpeed import FanSpeed
from CEMS.Zendure.GridReverse import GridReverse
from CEMS.Zendure.ControlStatus import ControlStatus
from CEMS.Zendure.ControlState import ControlState
from CEMS.OpenMeteo.WeatherCode import WeatherCode
from CEMS.OpenMeteo.WeatherCurrent import WeatherCurrent
from CEMS.OpenMeteo.WeatherForecast import WeatherForecast

exit_code = 0
stop = False    

# Signal handler to handle Ctrl+C
def interrupt_handler(signum, frame):
    global stop
    if signum == signal.SIGINT:
        stop = True

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Observer component that reads sensor data and prints it on the screen using the RCU mechanism")
    parser.add_argument('-n', '--name', default='observer_py', help='Component name (default: observer_py)')
    return parser.parse_args()

def format_master_clock(clock):
    if clock is None:
        return "Master Clock: N/A"
    return f"Master Clock: {clock.Timestamp():.3f} | Freq: {clock.Frequency():.2f} Hz | Arrow keys: Scroll up/down/left/right | (i) Toggle instant, (e) Toggle energy, (d) Toggle details, (q) Quit"

def format_elkor_instant(ir, details:bool=True):
    if ir is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(ir.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    def tpf_desc(tpf):
        if tpf >= 0:
            return "INDUCTIVE"
        elif tpf < 0.0:
            return "CAPACITIVE"
        else:
            return "RESISTIVE"

    l = [
        f"Unix time: {ir.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        "",
        "--- TOTAL ---",
        f"f: {ir.Frequency():.2f} Hz",
        f"P: {ir.TotalRealPower():7.0f} ({ir.PhaseARealPower()+ir.PhaseBRealPower()+ir.PhaseCRealPower():.0f}) W",
        f"Q: {ir.TotalReactivePower():.0f} var",
        f"S: {ir.TotalApparentPower():.0f} VA",
        f"TPF: {ir.TotalSystemPowerFactor():.4f} ({tpf_desc(ir.TotalSystemPowerFactor())})",
        f"Total Current: {ir.TotalCurrent():.2f} A",
        "",
        "--- AVERAGES ---",
        f"I: {ir.TotalAverageCurrent():.2f} A",
        f"L-N: {ir.TotalAverageVoltageToNeutral():.1f} V",
        f"L-L: {ir.TotalAverageVoltageToLine():.1f} V"
    ]

    if details:
        l += [
            "",
            "--- PHASE A ---",
            f"P: {ir.PhaseARealPower():.1f} W",
            f"I: {ir.PhaseACurrent():.2f} A",
            f"V: {ir.PhaseAVoltageToNeutral():.1f} V",
            f"Q: {ir.PhaseAReactivePower():.1f} var",
            f"S: {ir.PhaseAApparentPower():.1f} VA",
            f"TPF: {ir.PhaseAPowerFactor():.1f} ({tpf_desc(ir.PhaseAPowerFactor())})",
            f"L1-L2: {ir.PhaseAVoltageToB():.1f} V",
            "",
            "--- PHASE B ---",
            f"P: {ir.PhaseBRealPower():.1f} W",
            f"I: {ir.PhaseBCurrent():.2f} A",
            f"V: {ir.PhaseBVoltageToNeutral():.1f} V",
            f"Q: {ir.PhaseBReactivePower():.1f} var",
            f"S: {ir.PhaseBApparentPower():.1f} VA",
            f"TPF: {ir.PhaseBPowerFactor():.1f} ({tpf_desc(ir.PhaseBPowerFactor())})",
            f"L2-L3: {ir.PhaseBVoltageToC():.1f} V",
            "",
            "--- PHASE C ---",
            f"P: {ir.PhaseCRealPower():.1f} W",
            f"I: {ir.PhaseCCurrent():.2f} A",
            f"V: {ir.PhaseCVoltageToNeutral():.1f} V",
            f"Q: {ir.PhaseCReactivePower():.1f} var",
            f"S: {ir.PhaseCApparentPower():.1f} VA",
            f"TPF: {ir.PhaseCPowerFactor():.1f} ({tpf_desc(ir.PhaseCPowerFactor())})",
            f"L3-L1: {ir.PhaseCVoltageToA():.1f} V"
        ]
    return l

def format_elkor_energy(er, details:bool=True):
    if er is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(er.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {er.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        "",
        "--- TOTAL ---",
        f"Net: {er.TotalNetEnergy()/1000.0:.1f} kWh",
        f"Import: {er.TotalImportEnergy()/1000.0:.1f} kWh",
        f"Export: {er.TotalExportEnergy()/1000.0:.1f} kWh",
        f"Inductive: {er.TotalInductiveEnergy()/1000.0:.1f} kvarh",
        f"Capacitive: {er.TotalCapacitiveEnergy()/1000.0:.1f} kvarh",
        f"Net reactive: {er.TotalNetReactiveEnergy()/1000.0:.1f} kvarh",
        f"Apparent: {er.TotalApparentEnergy()/1000.0:.1f} kVAh"
    ]

    if details:
        l += [
            "",
            "--- PHASE A ---",
            f"Net: {er.PhaseANetEnergy()/1000.0:.1f} kWh",
            f"Import: {er.PhaseAImportEnergy()/1000.0:.1f} kWh",
            f"Export: {er.PhaseAExportEnergy()/1000.0:.1f} kWh",
            f"Inductive: {er.PhaseAInductiveEnergy()/1000.0:.1f} kvarh",
            f"Capacitive: {er.PhaseACapacitiveEnergy()/1000.0:.1f} kvarh",
            f"Net reactive: {er.PhaseANetReactiveEnergy()/1000.0:.1f} kvarh",
            f"Apparent: {er.PhaseAApparentEnergy()/1000.0:.1f} kvarh",
            "",
            "--- PHASE B ---",
            f"Net: {er.PhaseBNetEnergy()/1000.0:.1f} kWh",
            f"Import: {er.PhaseBImportEnergy()/1000.0:.1f} kWh",
            f"Export: {er.PhaseBExportEnergy()/1000.0:.1f} kWh",
            f"Inductive: {er.PhaseBInductiveEnergy()/1000.0:.1f} kvarh",
            f"Capacitive: {er.PhaseBCapacitiveEnergy()/1000.0:.1f} kvarh",
            f"Net reactive: {er.PhaseBNetReactiveEnergy()/1000.0:.1f} kvarh",
            f"Apparent: {er.PhaseBApparentEnergy()/1000.0:.1f} kvarh",
            "",
            "--- PHASE C ---",
            f"Net: {er.PhaseCNetEnergy()/1000.0:.1f} kWh",
            f"Import: {er.PhaseCImportEnergy()/1000.0:.1f} kWh",
            f"Export: {er.PhaseCExportEnergy()/1000.0:.1f} kWh",
            f"Inductive: {er.PhaseCInductiveEnergy()/1000.0:.1f} kvarh",
            f"Capacitive: {er.PhaseCCapacitiveEnergy()/1000.0:.1f} kvarh",
            f"Net reactive: {er.PhaseCNetReactiveEnergy()/1000.0:.1f} kvarh",
            f"Apparent: {er.PhaseCApparentEnergy()/1000.0:.1f} kvarh"
        ]
    return l

def format_ime_instant(ir, details:bool=True):
    if ir is None:
        return ["N/A"]

    sector_value = ir.TriPhaseSectorOfPowerFactor()

    dt = datetime.fromtimestamp(ir.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    def sector_code_to_name(code):
        for name, value in IMESector.__dict__.items():
            if value == code:
                return name
        return None

    l = [
        f"Unix time: {ir.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        "",
        "--- TOTAL ---",
        f"f: {ir.Frequency():.2f} Hz",
        f"P: {ir.TriPhaseActivePower():.0f} W",
        f"Q: {ir.TriPhaseReactivePower():.0f} var",
        f"S: {ir.TriPhaseApparentPower():.0f} VA",
        f"PF: 0.{int(ir.TriPhasePowerFactor())} cos φ",
        f"Sector: {sector_value} ({sector_code_to_name(sector_value) or 'Unknown'})",
        f"Peak: {ir.TriPhasePeakMaximumDemand():.0f} W",
        f"Second Tariff Peak: {ir.TriPhaseSecondTariffPeakMaximumDemand():.0f} W",
        "",
        "--- AVERAGES ---",
        f"Time Counter Avg. P: {ir.TimeCounterForAveragePower()} m",
        f"Avg. P: {ir.TriPhaseAveragePower():.0f} W",
        f"L-N: {ir.AverageVoltageLineNeutral():.1f} V",
        f"L-L: {ir.AverageVoltageLineLine():.1f} V",
        f"I: {ir.AverageCurrent():.2f} A"
    ]

    if details:
        l += [
            "",
            "--- PHASE 1 ---",
            f"P: {ir.Phase1ActivePower():.1f} W",
            f"I: {ir.Phase1Current():.2f} A",
            f"V: {ir.Phase1Voltage():.1f} V",
            f"Q: {ir.Phase1ReactivePower():.1f} var",
            f"L1-L2: {ir.L1L2Voltage():.1f} V",
            "",
            "--- PHASE 2 ---",
            f"P: {ir.Phase2ActivePower():.1f} W",
            f"I: {ir.Phase2Current():.2f} A",
            f"V: {ir.Phase2Voltage():.1f} V",
            f"Q: {ir.Phase2ReactivePower():.1f} var",
            f"L2-L3: {ir.L2L3Voltage():.1f} V",
            "",
            "--- PHASE 3 ---",
            f"P: {ir.Phase3ActivePower():.1f} W",
            f"I: {ir.Phase3Current():.2f} A",
            f"V: {ir.Phase3Voltage():.1f} V",
            f"Q: {ir.Phase3ReactivePower():.1f} var",
            f"L3-L1: {ir.L3L1Voltage():.1f} V"
        ]
    return l

def format_ime_energy(er, details:bool=True):
    if er is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(er.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {er.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        f"Total Pos. Active: {er.TriPhaseTotalPositiveActiveEnergy():.1f} kWh",
        f"Total Pos. Reactive: {er.TriPhaseTotalPositiveReactiveEnergy():.1f} kvarh",
        f"Partial Pos. Active: {er.TriPhasePartialPositiveActiveEnergy():.1f} kWh",
        f"Partial Pos. Reactive: {er.TriPhasePartialPositiveReactiveEnergy():.1f} kvarh",
        f"Partial Sec. Tar. Pos. Active: {er.TriPhasePartialSecondTariffPositiveActiveEnergy():.1f} kWh",
        f"Partial Sec. Tar. Pos. Reactive: {er.TriPhasePartialSecondTariffPositiveReactiveEnergy():.1f} kvarh"
    ]
    return l

def format_circutor_instant(ir, details:bool=True):
    if ir is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(ir.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {ir.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        "",
        "--- TOTAL ---",
        f"P: {ir.TotalActivePower()} W",
        f"Q: {ir.TotalReactivePower()} var",
        f"S: {ir.TotalApparentPower()} VA",
    ]

    if details:
        l += [
            "",
            "--- PHASE 1 ---",
            f"P: {ir.Phase1ActivePower()} W",
            f"I: {ir.Phase1Current():.2f} A",
            f"V: {ir.Phase1Voltage():.1f} V",
            f"Q: {ir.Phase1ReactivePower():.1f} var",
            f"S: {ir.Phase1ApparentPower()} VA",
            f"PF: {ir.Phase1CosPhi():.2f} cos φ",
            "",
            "--- PHASE 2 ---",
            f"P: {ir.Phase2ActivePower()} W",
            f"I: {ir.Phase2Current():.2f} A",
            f"V: {ir.Phase2Voltage():.1f} V",
            f"Q: {ir.Phase2ReactivePower():.1f} var",
            f"S: {ir.Phase2ApparentPower()} VA",
            f"PF: {ir.Phase2CosPhi():.2f} cos φ",
            "",
            "--- PHASE 3 ---",
            f"P: {ir.Phase3ActivePower()} W",
            f"I: {ir.Phase3Current():.2f} A",
            f"V: {ir.Phase3Voltage():.1f} V",
            f"Q: {ir.Phase3ReactivePower():.1f} var",
            f"S: {ir.Phase3ApparentPower()} VA",
            f"PF: {ir.Phase3CosPhi():.2f} cos φ"
        ]
    return l

def format_circutor_energy(er, details:bool=True):
    if er is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(er.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {er.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        f"Imported: {er.ImportedActiveEnergy() / 1000.0:<.1f} kWh",
        f"Exported: {er.ExportedActiveEnergy() / 1000.0:<.1f} kWh"
    ]

    if details:
        l += [
            f"Q1 Reactive: {er.Q1ReactiveEnergy() / 1000.0:<.1f} kvarh",
            f"Q2 Reactive: {er.Q2ReactiveEnergy() / 1000.0:<.1f} kvarh",
            f"Q3 Reactive: {er.Q3ReactiveEnergy() / 1000.0:<.1f} kvarh",
            f"Q4 Reactive: {er.Q4ReactiveEnergy() / 1000.0:<.1f} kvarh"
        ]
    return l

def format_zendure(bs, details:bool=True):
    if bs is None:
        return ["N/A"]

    pack_state_map = {v: k for k, v in PackState.__dict__.items() if not k.startswith('_')}
    ac_status_map = {v: k for k, v in ACStatus.__dict__.items() if not k.startswith('_')}
    dc_status_map = {v: k for k, v in DCStatus.__dict__.items() if not k.startswith('_')}
    pv_status_map = {v: k for k, v in PVStatus.__dict__.items() if not k.startswith('_')}
    ac_mode_map = {v: k for k, v in ACMode.__dict__.items() if not k.startswith('_')}
    battery_state_map = {v: k for k, v in BatteryState.__dict__.items() if not k.startswith('_')}
    grid_standard_map = {v: k for k, v in GridStandard.__dict__.items() if not k.startswith('_')}
    off_grid_mode_map = {v: k for k, v in OffGridMode.__dict__.items() if not k.startswith('_')}
    fan_mode_map = {v: k for k, v in FanMode.__dict__.items() if not k.startswith('_')}
    fan_speed_map = {v: k for k, v in FanSpeed.__dict__.items() if not k.startswith('_')}
    grid_reverse_map = {v: k for k, v in GridReverse.__dict__.items() if not k.startswith('_')}

    raw_pack_state = bs.PackState()
    raw_ac_status = bs.AcStatus()
    raw_dc_status = bs.DcStatus()
    raw_pv_status = bs.PvStatus()
    raw_ac_mode = bs.AcMode()
    raw_grid_standard = bs.GridStandard()
    raw_off_grid_mode = bs.GridOffMode()
    raw_fan_mode = bs.FanSwitch()
    raw_fan_speed = bs.FanSpeed()
    raw_grid_reverse = bs.GridReverseAllowed()

    pack_state_label = pack_state_map.get(raw_pack_state, f"UNKNOWN ({raw_pack_state})")
    ac_status_label = ac_status_map.get(raw_ac_status, f"UNKNOWN ({raw_ac_status})")
    dc_status_label = dc_status_map.get(raw_dc_status, f"UNKNOWN ({raw_dc_status})")
    pv_status_label = pv_status_map.get(raw_pv_status, f"UNKNOWN ({raw_pv_status})")
    ac_mode_label = ac_mode_map.get(raw_ac_mode, f"UNKNOWN ({raw_ac_mode})")
    grid_standard_label = grid_standard_map.get(raw_grid_standard, f"UNKNOWN ({raw_grid_standard})")
    off_grid_mode_label = off_grid_mode_map.get(raw_off_grid_mode, f"UNKNOWN ({raw_off_grid_mode})")
    fan_mode_label = fan_mode_map.get(raw_fan_mode, f"UNKNOWN ({raw_fan_mode})")
    fan_speed_label = fan_speed_map.get(raw_fan_speed, f"UNKNOWN ({raw_fan_speed})")
    grid_reverse_label = grid_reverse_map.get(raw_grid_reverse, f"UNKNOWN ({raw_grid_reverse})")

    dt = datetime.fromtimestamp(bs.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {bs.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        f"Product: {bs.Product().decode('utf-8')}",
        f"Serial Num.: {bs.SerialNumber().decode('utf-8')}",
        f"Version: {bs.Version()}",
        f"Temperature: {bs.Temperature():.1f} °C",
        f"Target Power: {bs.TargetPower()} W",
        f"Countdown Power Update: {bs.CountdownPowerUpdate()} s",
        f"Countdown State Change: {bs.CountdownStateChange()} s",
        f"Countdown Standby: {bs.CountdownStandby()} s",
        f"Countdown Low Power: {bs.CountdownLowPower()} s",
        f"Error: {bs.IsError()}",
        f"Fault Level: {bs.FaultLevel()}",
        f"Data Ready: {bs.DataReady()}",
        f"Write Response: {bs.WriteResponse()}",
        f"RSSI: {bs.Rssi()} dBm",
        f"Input Pack Power: {bs.InputPackPower()} W",
        f"Output Pack Power: {bs.OutputPackPower()} W",
        f"Output Home Power: {bs.OutputHomePower()} W",
        f"Solar Total Power: {bs.SolarTotalPower()} W",
        f"Grid Input Power: {bs.GridInputPower()} W",
        f"Grid Off Power: {bs.GridOffPower()} W",
        f"State Of Charge: {bs.StateOfCharge()} %",
        f"Target SOC: {bs.TargetStateOfCharge()} %",
        f"Minimum SOC: {bs.MinStateOfCharge()} %",
        f"Remaining Outtime: {bs.RemainOutTime()} min.",
        f"Pack State: {pack_state_label}",
        f"SOC Limit Reached: {bs.SocLimitReached()}",
        f"Charge Max Limit: {bs.ChargeMaxLimit()} W",
        f"Heating: {bs.Heating()}",
        f"Passthrough: {bs.Passthrough()}",
        f"Reverse Flow: {bs.ReverseFlow()}",
        f"Calibrating: {bs.Calibrating()}",
        f"AC Status: {ac_status_label}",
        f"DC Status: {dc_status_label}",
        f"PV Status: {pv_status_label}",
        f"AC Mode: {ac_mode_label}",
        f"Grid Connected: {bs.GridState()}",
        f"AC Coupling: {bs.AcCouplingState()}", # Bit detection needed
        f"Battery Volt: {bs.BatteryVolt():.2f} V",
        f"Bat. Cal. Time: {bs.BatteryCalibrationTime()} m",
        f"Timezone: {bs.TimeZone().decode('utf-8')}",
        f"Grid Standard: {grid_standard_label}",
        f"Grid Reverse Allowed: {grid_reverse_label}",
        f"Grid Off Mode: {off_grid_mode_label}",
        f"Input Limit: {bs.InputLimit()} W",
        f"Output Limit: {bs.OutputLimit()} W",
        f"Packs: {bs.PacksLength()}"
    ]

    if details:
        l += [
            "",
            "--- SOLAR ---",
            f"Solar Power 1: {bs.SolarPower1()}",
            f"Solar Power 2: {bs.SolarPower2()}",
            f"Solar Power 3: {bs.SolarPower3()}",
            f"Solar Power 4: {bs.SolarPower4()}",
            f"Solar Power 5: {bs.SolarPower5()}",
            f"Solar Power 6: {bs.SolarPower6()}",
            "",
            "--- FAN/LAMP ---",
            f"Lamp Switch: {bs.LampSwitch()}",
            f"Fan Switch: {fan_mode_label}",
            f"Fan Speed: {fan_speed_label}",
            "",
            "--- VARIA ---",
            f"Smart Mode: {bs.SmartMode()}",
            f"IOT State: {bs.IotState()}",
            f"OTA State: {bs.OtaState()}",
            f"LCN State: {bs.LcnState()}",
            f"Bind State: {bs.BindState()}",
            f"Dry Node: {bs.DryNodeState()}",
            f"FM Volt: {bs.FmVolt()} V",
            f"Volt Wakeup: {bs.VoltWakeup()} V"
        ]
        for j in range(bs.PacksLength()):
            pack = bs.Packs(j)
            raw_battery_state = pack.State()
            battery_state_label = battery_state_map.get(raw_battery_state, f"UNKNOWN ({raw_battery_state})")
            l += [
                "",
                f"--- BATTERY {j} ---",
                f"Serial Number: {pack.SerialNumber().decode('utf-8')}",
                f"Pack Type: {pack.PackType()}",
                f"State Of Charge: {pack.StateOfCharge()} %",
                f"State: {battery_state_label}",
                f"Power: {pack.Power()} W",
                f"Max Temperature: {pack.MaxTemperature()} °C",
                f"Voltage: {pack.TotalVoltage():.2f} V",
                f"Current: {pack.BatteryCurrent():.2f} A",
                f"Max Cell Voltage: {pack.MaxCellVoltage():.2f} V",
                f"Min Cell Voltage: {pack.MinCellVoltage():.2f} V",
                f"Software Version: {pack.SoftVersion()}",
                f"Heating: {pack.HeatState()}"
            ]
    return l

def format_zendure_control(cs, details:bool=True):
    if cs is None:
        return ["N/A"]

    control_state_map = {v: k for k, v in ControlState.__dict__.items() if not k.startswith('_')}

    raw_control_state = cs.Status()
    control_state_label = control_state_map.get(raw_control_state, f"UNKNOWN ({raw_control_state})")

    # Convert hours and minutes to a more readable format
    sunrise_hours = int(cs.Sunrise())
    sunrise_minutes = int((cs.Sunrise() * 60) % 60)
    sunrise_label = f"{sunrise_hours}:{sunrise_minutes:02d}"
    sunset_hours = int(cs.Sunset())
    sunset_minutes = int((cs.Sunset() * 60) % 60)
    sunset_label = f"{sunset_hours}:{sunset_minutes:02d}"

    dt = datetime.fromtimestamp(cs.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"

    l = [
        f"Unix time: {cs.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        f"Control State: {control_state_label}",
        f"Target SOC: {cs.TargetSoc()} %",
        f"Sunrise: {sunrise_label} hours",
        f"Sunset: {sunset_label} hours",
        f"Is Night: {cs.IsNight()}",
        f"Is Day: {cs.IsDay()}",
        f"EV Charging: {cs.EvCharging()}",
        f"Linear Change Cap: {cs.LinearChangeCap()} W/s/bat",
    ]

    if details:
        if cs.BatteryA():
            l += [
                "",
                "--- BATTERY A ---",
                f"Target Power: {cs.BatteryA().TargetPower()}"
            ]
        if cs.BatteryB():
            l += [
                "",
                "--- BATTERY B ---",
                f"Target Power: {cs.BatteryB().TargetPower()}",
            ]
    return l

def format_weather_current(ws, details:bool=True):
    if ws is None:
        return ["N/A"]

    dt = datetime.fromtimestamp(ws.Timestamp())
    timestamp_label = dt.strftime('%H:%M:%S') + f".{dt.microsecond // 10000:02d}"
    weather_code = ws.WeatherCode()
    weather_code_map = {v: k for k, v in WeatherCode.__dict__.items() if not k.startswith('_')}
    weather_code_label = weather_code_map.get(weather_code, f"UNKNOWN ({weather_code})")

    l = [
        f"Unix time: {ws.Timestamp():.3f}",
        f"Timestamp: {timestamp_label}",
        f"Timezone: {ws.TimezoneAbbreviation().decode('utf-8')}",
        f"UTC Offset: {ws.UtcOffsetSecond()} s",
        f"Latitude: {ws.Latitude():.5f} °N",
        f"Longitude: {ws.Longitude():.5f} °E",
        f"Elevation: {ws.Elevation():.2f} m a.s.l.",
        f"Temperature: {ws.Temperature():.1f} °C",
        f"Weather Code: {weather_code_label}",
        f"Direct Irradiance: {ws.DirectNormalIrradiance():.2f} W/m2",
        f"Diffuse Irradiance: {ws.DiffuseHorizontalIrradiance():.2f} W/m2",
        f"Direct Radiation: {ws.DirectRadiation():.2f} W/m2",
        f"Shortwave Radiation: {ws.ShortwaveRadiation():.2f} W/m2",
        f"Relative Humidity: {ws.RelativeHumidity():.1f} %",
        f"Precipitation Probability: {ws.PrecipitationProbability():.1f} %",
        f"Precipitation: {ws.Precipitation():.2f} mm",
        f"Pressure MSL: {ws.PressureMsl():.1f} hPa",
        f"Cloud Cover Total: {ws.CloudCover():.1f} %",
        f"Cloud Cover Low: {ws.CloudCoverLow():.1f} %",
        f"Cloud Cover Mid: {ws.CloudCoverMid():.1f} %",
        f"Cloud Cover High: {ws.CloudCoverHigh():.1f} %",
        f"Visibility: {ws.Visibility():.1f} m",
        f"Wind Speed: {ws.WindSpeed():.2f} m/s",
        f"Rain: {ws.Rain():.2f} mm",
        f"Showers: {ws.Showers():.2f} mm",
        f"Snowfall: {ws.Snowfall():.2f} mm",
        f"Dew Point 2m: {ws.DewPoint2m():.2f} °C"
    ]
    return l

def format_weather_forecast(wf, details:bool=True):
    if wf is None:
        return ["N/A"]

    gen_time = datetime.fromtimestamp(wf.GeneratedTimestamp()).strftime('%Y-%m-%d %H:%M:%S')
    weather_code_map = {v: k for k, v in WeatherCode.__dict__.items() if not k.startswith('_')}

    l = [
        f"Gen. Time: {gen_time}",
        f"Latitude: {wf.Latitude():.5f} °N",
        f"Longitude: {wf.Longitude():.5f} °E"
    ]
    for idx in range(wf.HourlyLength()):
        hourly = wf.Hourly(idx)
        timestamp = hourly.Timestamp()
        if timestamp < time.time():
            continue
        hour_time = datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M')
        weather_code = hourly.WeatherCode()
        weather_code_label = weather_code_map.get(weather_code, f"UNKNOWN ({weather_code})")
        l += [
            f"- {hour_time}",
            f"  Temperature   : {hourly.Temperature():.1f} °C",
            f"  Weather Code: : {weather_code_label}",
            f"  Dir. Irrad.   : {hourly.DirectNormalIrradiance():.2f} W/m2",
            f"  Diff. Irrad.  : {hourly.DiffuseHorizontalIrradiance():.2f} W/m2",
            f"  Direct Rad.   : {hourly.DirectRadiation():.2f} W/m2",
            f"  Short. Rad.   : {hourly.ShortwaveRadiation():.2f} W/m2",
            f"  Rel. Humidity : {hourly.RelativeHumidity():.1f} %",
            f"  Precip. Prob. : {hourly.PrecipitationProbability():.1f} %",
            f"  Precipitation : {hourly.Precipitation():.2f} mm",
            f"  Pressure MSL  : {hourly.PressureMsl():.1f} hPa",
            f"  Cloud Total   : {hourly.CloudCover():.1f} %",
            f"  Cloud Low     : {hourly.CloudCoverLow():.1f} %",
            f"  Cloud Mid     : {hourly.CloudCoverMid():.1f} %",
            f"  Cloud High    : {hourly.CloudCoverHigh():.1f} %",
            f"  Visibility    : {hourly.Visibility():.1f} m",
            f"  Wind Speed    : {hourly.WindSpeed():.2f} m/s",
            f"  Rain          : {hourly.Rain():.2f} mm",
            f"  Showers       : {hourly.Showers():.2f} mm",
            f"  Snowfall      : {hourly.Snowfall():.2f} mm",
            f"  Dew Point 2m  : {hourly.DewPoint2m():.2f} °C"
        ]
    return l

def safe_addstr(target_win, y, x, text, width=None):
    """Safely adds string to an explicit pad/window context and clips it horizontally"""
    height, max_w = target_win.getmaxyx()
    if y < 0 or y >= height or x < 0 or x >= max_w:
        return

    available_w = max_w - x
    if width is not None:
        available_w = min(available_w, width)

    if available_w <= 0:
        return

    try:
        target_win.addstr(y, x, text[:available_w])
    except curses.error:
        pass

def draw_panel_on_pad(pad, y, x, width, title, lines):
    """Renders single sub-component columns onto the global virtual canvas pad"""
    safe_addstr(pad, y, x, f"[ {title} ]", width)
    for i, line in enumerate(lines):
        safe_addstr(pad, y + i + 1, x, line, width - 1)

class ColumnRenderer:
    def __init__(self, stdscr, max_y, max_x, col_width, scroll_col):
        self.stdscr = stdscr
        self.max_y = max_y
        self.max_x = max_x
        self.col_w = col_width
        self.scroll_col = scroll_col

    def draw(self, native_x, title, lines, start_row, visible_height):
        x_offset = native_x - self.scroll_col
        end_row = start_row + visible_height

        try:
            # Render column header within screen visibility bounds
            if 2 < self.max_y - 1 and 0 <= x_offset < self.max_x:
                visible_title = title[:min(self.col_w - 1, self.max_x - x_offset)]
                self.stdscr.addstr(2, x_offset, visible_title, curses.A_BOLD)

            for i, text_line in enumerate(lines[start_row:end_row]):
                y_pos = 3 + i
                if y_pos < self.max_y - 1:
                    # Guard draw limits strictly to prevent off-screen exceptions
                    if 0 <= x_offset < self.max_x:
                        max_printable = min(self.col_w - 1, self.max_x - x_offset)
                        safe_text = str(text_line)[:max_printable]
                        self.stdscr.addstr(y_pos, x_offset, safe_text)
        except curses.error:
            pass

class Observer(fabrix.Component):
    """Observer - reads sensor data and prints it on the screen using the RCU mechanism."""

    _COMPONENT_NAME_MASTER_CLOCK = "master_clock"
    _COMPONENT_NAME_ELKOR = "elkor"
    _COMPONENT_NAME_IME = "ime"
    _COMPONENT_NAME_EV_METER = "ev_meter"
    _COMPONENT_NAME_ZENDURE_BATTERY_A = "zendure_battery_a"
    _COMPONENT_NAME_ZENDURE_BATTERY_B = "zendure_battery_b"
    _COMPONENT_NAME_ZENDURE_CONTROL = "zendure_control"
    _COMPONENT_NAME_WEATHER = "weather"

    def __init__(self, name):
        """Constructor."""
        super().__init__(name, "cems")
        self.master_clock_area = rcu.Reader()
        self.elkor_instant_reading_area = rcu.Reader()
        self.elkor_energy_reading_area = rcu.Reader()
        self.ime_instant_reading_area = rcu.Reader()
        self.ime_energy_reading_area = rcu.Reader()
        self.circutor_instant_reading_area = rcu.Reader()
        self.circutor_energy_reading_area = rcu.Reader()
        self.zendure_battery_a_area = rcu.Reader()
        self.zendure_battery_b_area = rcu.Reader()
        self.zendure_control_area = rcu.Reader()
        self.weather_current_area = rcu.Reader()
        self.weather_forecast_area = rcu.Reader()
        self.data = {
            "master_clock": {"clock_tick": None},
            "elkor": {"instant": None, "energy": None},
            "ime": {"instant": None, "energy": None},
            "circutor": {"instant": None, "energy": None},
            "zendure": {"a": None, "b": None, "control": None},
            "weather": {"status": None, "forecast": None}
        }
        self.instant = True
        self.details = False
        self.energy = True
        self.battery_a = True
        self.battery_b = True
        self.zendure_control = True
        self.weather = True
        self.scroll_row = 0
        self.scroll_col = 0

    def run(self):
        """Main loop."""
        curses.wrapper(self._curses_main)

    def _curses_main(self, stdscr):
        global stop
        curses.curs_set(0)
        curses.mousemask(curses.ALL_MOUSE_EVENTS)
        stdscr.nodelay(True)
        stdscr.keypad(True)

        timestep = 0.25
        next_timepoint = time.time() + timestep
        self.process()

        while not stop:
            self._act()
            stdscr.erase()

            height, width = stdscr.getmaxyx()
            if width < 60 or height < 10:
                safe_addstr(stdscr, 0, 0, "Terminal too small. Resize window.")
                stdscr.refresh()
                time.sleep(0.5)
                continue

            # Spaces for ELKOR, IME, CIRCUTOR, ZENDURE A, ZENDURE B, ZENDURE CONTROL, WEATHER
            col_w = 40
            total_content_width = col_w * 7

            # Draw static master clock header on the standard window
            clock_str = format_master_clock(self.data["master_clock"]["clock_tick"])
            safe_addstr(stdscr, 0, 0, clock_str)

            try:
                stdscr.hline(1, 0, curses.ACS_HLINE, width)
            except curses.error:
                pass

            # Gather telemetry lines
            elkor_lines = []
            if self.instant:
                elkor_lines += ["", "-- Instant --"] + format_elkor_instant(self.data["elkor"]["instant"], self.details)
            if self.energy:
                elkor_lines += ["", "-- Energy --"] + format_elkor_energy(self.data["elkor"]["energy"], self.details)

            ime_lines = []
            if self.instant:
                ime_lines += ["", "-- Instant --"] + format_ime_instant(self.data["ime"]["instant"], self.details)
            if self.energy:
                ime_lines += ["", "-- Energy --"] + format_ime_energy(self.data["ime"]["energy"], self.details)

            circ_lines = []
            if self.instant:
                circ_lines += ["", "-- Instant --"] + format_circutor_instant(self.data["circutor"]["instant"], self.details)
            if self.energy:
                circ_lines += ["", "-- Energy --"] + format_circutor_energy(self.data["circutor"]["energy"], self.details)

            bat_lines_a = []
            if self.battery_a:
                bat_lines_a += [""] + format_zendure(self.data["zendure"]["a"], self.details)

            bat_lines_b = []
            if self.battery_b:
                bat_lines_b += [""] + format_zendure(self.data["zendure"]["b"], self.details)

            bat_control_lines = []
            if self.zendure_control:
                bat_control_lines += [""] + format_zendure_control(self.data["zendure"]["control"], self.details)

            weather_lines = []
            if self.weather:
                weather_lines += ["", "-- CURRENT --"] + format_weather_current(self.data["weather"]["current"], self.details) + \
                                 ["", "-- FORECAST --"] + format_weather_forecast(self.data["weather"]["forecast"], self.details)

            total_content_height = max(len(elkor_lines), len(ime_lines), len(circ_lines), len(bat_lines_a), len(bat_lines_b), len(bat_control_lines), len(weather_lines)) + 3

            visible_viewport_height = height - 4
            if visible_viewport_height < 1:
                visible_viewport_height = 1

            max_possible_scroll = max(0, total_content_height - visible_viewport_height)
            if self.scroll_row > max_possible_scroll:
                self.scroll_row = max_possible_scroll

            max_possible_h_scroll = max(0, total_content_width - width)
            if self.scroll_col > max_possible_h_scroll:
                self.scroll_col = max_possible_h_scroll

            # Render viewport direct matrix
            renderer = ColumnRenderer(stdscr, height, width, col_w, self.scroll_col)
            renderer.draw(0, "ELKOR", elkor_lines, self.scroll_row, visible_viewport_height)
            renderer.draw(col_w, "IME", ime_lines, self.scroll_row, visible_viewport_height)
            renderer.draw(2 * col_w, "CIRCUTOR", circ_lines, self.scroll_row, visible_viewport_height)
            renderer.draw(3 * col_w, "ZENDURE A", bat_lines_a, self.scroll_row, visible_viewport_height)
            renderer.draw(4 * col_w, "ZENDURE B", bat_lines_b, self.scroll_row, visible_viewport_height)
            renderer.draw(5 * col_w, "ZENDURE CONTROL", bat_control_lines, self.scroll_row, visible_viewport_height)
            renderer.draw(6 * col_w, "WEATHER", weather_lines, self.scroll_row, visible_viewport_height)

            stdscr.refresh()

            self.process_until(next_timepoint)
            while next_timepoint <= time.time(): 
                next_timepoint += timestep

            try:
                while True:
                    c = stdscr.getch()
                    if c == -1:
                        break
                    elif c == curses.KEY_UP:
                        if self.scroll_row > 0:
                            self.scroll_row -= 1
                    elif c == curses.KEY_DOWN:
                        if self.scroll_row < max_possible_scroll:
                            self.scroll_row += 1
                    elif c == curses.KEY_LEFT:
                        if self.scroll_col > 0:
                            self.scroll_col = max(0, self.scroll_col - 4)
                    elif c == curses.KEY_RIGHT:
                        if self.scroll_col < max_possible_h_scroll:
                            self.scroll_col = min(max_possible_h_scroll, self.scroll_col + 4)
                    elif c == curses.KEY_BACKSPACE:
                        self.scroll_col = 0
                        self.scroll_row = 0
                    elif c == ord('d'):
                        self.details = not self.details
                    elif c == ord('i'):
                        self.instant = not self.instant
                    elif c == ord('e'):
                        self.energy = not self.energy
                    elif c == ord('q'):
                        stop = True
                        break
                    elif c == curses.KEY_MOUSE:
                        _, x, y, _, bstate = curses.getmouse()
            except curses.error:
                pass

    def _on_start(self):
        """Only called once during start of the component."""
        print(f"Component {self.identifier().name()} is online with pid {os.getpid()}.")
        for name in self.list_components(True):
            if name == self._COMPONENT_NAME_MASTER_CLOCK and (endpoint := self._open_endpoint(name)).is_open():
                self.master_clock_area = rcu.find_area(endpoint, "ClockTick")
            elif name == self._COMPONENT_NAME_ELKOR and (endpoint := self._open_endpoint(name)).is_open():
                self.elkor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.elkor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_IME and (endpoint := self._open_endpoint(name)).is_open():
                self.ime_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.ime_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_EV_METER and (endpoint := self._open_endpoint(name)).is_open():
                self.circutor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.circutor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_A and (endpoint := self._open_endpoint(name)).is_open():
                self.zendure_battery_a_area = rcu.find_area(endpoint, "BatteryStatus")
            elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_B and (endpoint := self._open_endpoint(name)).is_open():
                self.zendure_battery_b_area = rcu.find_area(endpoint, "BatteryStatus")
            elif name == self._COMPONENT_NAME_ZENDURE_CONTROL and (endpoint := self._open_endpoint(name)).is_open():
                self.zendure_control_area = rcu.find_area(endpoint, "ControlStatus")
            elif name == self._COMPONENT_NAME_WEATHER and (endpoint := self._open_endpoint(name)).is_open():
                self.weather_current_area = rcu.find_area(endpoint, "WeatherCurrent")
                self.weather_forecast_area = rcu.find_area(endpoint, "WeatherForecast")

    def _on_endpoint_create(self, name, is_private):
        """When a new known endpoint is created find RCU areas."""
        if is_private:
            return
        if name == self._COMPONENT_NAME_MASTER_CLOCK and (endpoint := self._open_endpoint(name)).is_open():
            self.master_clock_area = rcu.find_area(endpoint, "ClockTick")
        elif name == self._COMPONENT_NAME_ELKOR and (endpoint := self._open_endpoint(name)).is_open():
            self.elkor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
            self.elkor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
        elif name == self._COMPONENT_NAME_IME and (endpoint := self._open_endpoint(name)).is_open():
            self.ime_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
            self.ime_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
        elif name == self._COMPONENT_NAME_EV_METER and (endpoint := self._open_endpoint(name)).is_open():
            self.circutor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
            self.circutor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
        elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_A and (endpoint := self._open_endpoint(name)).is_open():
            self.zendure_battery_a_area = rcu.find_area(endpoint, "BatteryStatus")
        elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_B and (endpoint := self._open_endpoint(name)).is_open():
            self.zendure_battery_b_area = rcu.find_area(endpoint, "BatteryStatus")
        elif name == self._COMPONENT_NAME_ZENDURE_CONTROL and (endpoint := self._open_endpoint(name)).is_open():
            self.zendure_control_area = rcu.find_area(endpoint, "ControlStatus")
        elif name == self._COMPONENT_NAME_WEATHER and (endpoint := self._open_endpoint(name)).is_open():
            self.weather_current_area = rcu.find_area(endpoint, "WeatherCurrent")
            self.weather_forecast_area = rcu.find_area(endpoint, "WeatherForecast")

    def _on_endpoint_remove(self, name):
        """When an endpoint is removed, let the user know."""
        if name == self._COMPONENT_NAME_MASTER_CLOCK:
            self.master_clock_area.reset()
            self.data["master_clock"]["clock_tick"] = None
        elif name == self._COMPONENT_NAME_ELKOR:
            self.elkor_instant_reading_area.reset()
            self.elkor_energy_reading_area.reset()
            self.data["elkor"]["instant"] = None
            self.data["elkor"]["energy"] = None
        elif name == self._COMPONENT_NAME_IME:
            self.ime_instant_reading_area.reset()
            self.ime_energy_reading_area.reset()
            self.data["ime"]["instant"] = None
            self.data["ime"]["energy"] = None
        elif name == self._COMPONENT_NAME_EV_METER:
            self.circutor_instant_reading_area.reset()
            self.circutor_energy_reading_area.reset()
            self.data["circutor"]["instant"] = None
            self.data["circutor"]["energy"] = None
        elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_A:
            self.zendure_battery_a_area.reset()
            self.data["zendure"]["a"] = None
        elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_B:
            self.zendure_battery_b_area.reset()
            self.data["zendure"]["b"] = None
        elif name == self._COMPONENT_NAME_ZENDURE_CONTROL:
            self.zendure_control_area.reset()
            self.data["zendure"]["control"] = None
        elif name == self._COMPONENT_NAME_WEATHER:
            self.weather_current_area.reset()
            self.weather_forecast_area.reset()
            self.data["weather"]["current"] = None
            self.data["weather"]["forecast"] = None

    def _act(self):
        do_scan = False
        with rcu.ScopedAccess(self.master_clock_area) as access:
            if access:
                clock = MasterClockTick()
                clock.Init(access.get(), 8)
                self.data["master_clock"]["clock_tick"] = clock
            else:
                do_scan = True

        with rcu.ScopedAccess(self.elkor_instant_reading_area) as access:
            if access:
                ir = ElkorInstantReading()
                ir.Init(access.get(), 8)
                self.data["elkor"]["instant"] = ir
            else:
                do_scan = True
        with rcu.ScopedAccess(self.elkor_energy_reading_area) as access:
            if access:
                er = ElkorEnergyReading()
                er.Init(access.get(), 8)
                self.data["elkor"]["energy"] = er
            else:
                do_scan = True

        with rcu.ScopedAccess(self.ime_instant_reading_area) as access:
            if access:
                ir = IMEInstantReading()
                ir.Init(access.get(), 8)
                self.data["ime"]["instant"] = ir
            else:
                do_scan = True
        with rcu.ScopedAccess(self.ime_energy_reading_area) as access:
            if access:
                er = IMEEnergyReading()
                er.Init(access.get(), 8)
                self.data["ime"]["energy"] = er
            else:
                do_scan = True

        with rcu.ScopedAccess(self.circutor_instant_reading_area) as access:
            if access:
                ir = EVMeterInstantReading()
                ir.Init(access.get(), 8)
                self.data["circutor"]["instant"] = ir
            else:
                do_scan = True
        with rcu.ScopedAccess(self.circutor_energy_reading_area) as access:
            if access:
                er = EVMeterEnergyReading()
                er.Init(access.get(), 8)
                self.data["circutor"]["energy"] = er
            else:
                do_scan = True

        with rcu.ScopedAccess(self.zendure_battery_a_area) as access:
            if access:
                bs = BatteryStatus.GetRootAsBatteryStatus(access.get(), 0)
                self.data["zendure"]["a"] = bs
            else:
                do_scan = True
        with rcu.ScopedAccess(self.zendure_battery_b_area) as access:
            if access:
                bs = BatteryStatus.GetRootAsBatteryStatus(access.get(), 0)
                self.data["zendure"]["b"] = bs
            else:
                do_scan = True

        with rcu.ScopedAccess(self.zendure_control_area) as access:
            if access:
                cs = ControlStatus.GetRootAsControlStatus(access.get(), 0)
                self.data["zendure"]["control"] = cs
            else:
                do_scan = True

        with rcu.ScopedAccess(self.weather_current_area) as access:
            if access:
                wc = WeatherCurrent.GetRootAsWeatherCurrent(access.get(), 0)
                self.data["weather"]["current"] = wc
            else:
                do_scan = True
        with rcu.ScopedAccess(self.weather_forecast_area) as access:
            if access:
                fc = WeatherForecast.GetRootAsWeatherForecast(access.get(), 0)
                self.data["weather"]["forecast"] = fc
            else:
                do_scan = True

        def scan_rcu(endpoint):
            name = endpoint.identifier().name()
            if name == self._COMPONENT_NAME_MASTER_CLOCK and not self.master_clock_area:
                self.master_clock_area = rcu.find_area(endpoint, "ClockTick")
            elif name == self._COMPONENT_NAME_ELKOR and (not self.elkor_instant_reading_area or not self.elkor_energy_reading_area):
                self.elkor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.elkor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_IME and (not self.ime_instant_reading_area or not self.ime_energy_reading_area):
                self.ime_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.ime_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_EV_METER and (not self.circutor_instant_reading_area or not self.circutor_energy_reading_area):
                self.circutor_instant_reading_area = rcu.find_area(endpoint, "InstantReading")
                self.circutor_energy_reading_area = rcu.find_area(endpoint, "EnergyReading")
            elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_A and (not self.zendure_battery_a_area):
                self.zendure_battery_a_area = rcu.find_area(endpoint, "BatteryStatus")
            elif name == self._COMPONENT_NAME_ZENDURE_BATTERY_B and (not self.zendure_battery_b_area):
                self.zendure_battery_b_area = rcu.find_area(endpoint, "BatteryStatus")
            elif name == self._COMPONENT_NAME_ZENDURE_CONTROL and (not self.zendure_control_area):
                self.zendure_control_area = rcu.find_area(endpoint, "ControlStatus")
            elif name == self._COMPONENT_NAME_WEATHER and (not self.weather_current_area or not self.weather_forecast_area):
                self.weather_current_area = rcu.find_area(endpoint, "WeatherCurrent")
                self.weather_forecast_area = rcu.find_area(endpoint, "WeatherForecast")

        if do_scan:
            self.for_each_open_endpoint(scan_rcu)

    def _on_error(self, other_end, error_code):
        """Print errors."""      
        print(f"Error: {other_end.identifier().name() if other_end else '<>'} with error code {fabrix.EnumNameErrorCode(error_code)}")

    def _on_halt_component_request(self, sender_endpoint):
        """Accept a halt request."""
        global stop
        stop = True
        return True

def main():
    """Main function"""
    global exit_code

    try:
        args = parse_arguments()
    except SystemExit:
        return 1
    except Exception as e:
        print(f"Error parsing command line arguments: {e}", file=sys.stderr)
        return 1

    random.seed()
    signal.signal(signal.SIGINT, interrupt_handler)

    try:
        observer = Observer(args.name)
        observer.run()
    except Exception as e:
        print(f"Component execution failed: {e}", file=sys.stderr)
        exit_code = 1
    finally:
        signal.signal(signal.SIGINT, signal.SIG_DFL)

    return exit_code

if __name__ == "__main__":
    sys.exit(main())
