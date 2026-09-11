from typing import List, Optional
from pydantic import BaseModel, Field

class BatteryPackProps(BaseModel):
    sn: str # Serial number
    pack_type: int = Field(alias="packType") # Not used
    soc_level: int = Field(alias="socLevel") # Battery charge level
    state: int # 0: Standby, 1: Charging, 2: Discharging
    power: int # Battery pack power
    max_temp_raw: int = Field(alias="maxTemp") # Maximum battery temperature (maxTemp) is stored as one-tenth of the Kelvin temperature. The conversion formula to Celsius (°C) is:float maxTemp_Celsius = (maxTemp - 2731) / 10.0; // Unit: °C
    total_vol_raw: int = Field(alias="totalVol") # Total voltage
    batcur_raw: int = Field(alias="batcur") # The raw batcur data is stored as a 16-bit two’s complement value in a uint8_t[2] array.During parsing, it needs to be converted into a signed 16-bit integer.According to the protocol definition, divide the value by 10 to obtain the actual current, in amperes (A).
    max_vol_raw: int = Field(alias="maxVol") # Maximum cell voltage, stored in units of 0.01V. The conversion formula to actual voltage:maxVol / 100.0 (Unit: V)
    min_vol_raw: int = Field(alias="minVol") # Minimum cell voltage, stored in units of 0.01V. The conversion formula to actual voltage:minVol / 100.0 (Unit: V)
    soft_version: int = Field(alias="softVersion") # Software version
    heat_state: int = Field(alias="heatState") # 0: Not heating, 1: Heating

    @property
    def max_temperature(self) -> float:
        """Formula: (maxTemp - 2731) / 10.0"""
        return (self.max_temp_raw - 2731) / 10.0

    @property
    def total_voltage(self) -> float:
        return (self.total_vol_raw / 100.0)

    @property
    def battery_current(self) -> float:
        """
        Converts 16-bit uint raw value to signed int (two's complement)
        then divides by 10 as per protocol.
        """
        # Interpret as signed 16-bit
        val = self.batcur_raw
        if val > 32767:
            val -= 65536
        return val / 10.0

    @property
    def max_voltage(self) -> float:
        return self.max_vol_raw / 100.0

    @property
    def min_voltage(self) -> float:
        return self.min_vol_raw / 100.0

class GlobalProperties(BaseModel):
    # System Identifiers
    sn: Optional[str] = None # serial number
    pack_num: int = Field(alias="packNum") # Number of battery packs
    heat_state: int = Field(alias="heatState") # 0: Not heating, 1: Heating

    # Power Metrics
    input_pack: int = Field(alias="packInputPower") # Battery pack input power (discharging)
    output_pack: int = Field(alias="outputPackPower") # Output power to battery pack (charging)
    output_home: int = Field(alias="outputHomePower") # Output power to home electricity
    solar_total: int = Field(alias="solarInputPower") # Total solar input power
    grid_input_power: int = Field(alias="gridInputPower") # Grid input power

    # PV String Detail
    pv1: int = Field(alias="solarPower1") # Solar line 1 input power
    pv2: int = Field(alias="solarPower2") # Solar line 2 input power
    pv3: int = Field(alias="solarPower3") # Solar line 3 input power
    pv4: int = Field(alias="solarPower4") # Solar line 4 input power
    pv5: int = Field(alias="solarPower5") # Solar line 5 input power
    pv6: int = Field(alias="solarPower6") # Solar line 6 input power

    # Status & Limits
    remain_outtime: int = Field(alias="remainOutTime") # Remaining discharge time (unit: minutes)
    pack_state: int = Field(alias="packState") # 0: Standby, 1: Charging, 2: Discharging
    electric_level: int = Field(alias="electricLevel") # Average battery pack charge level
    pass_prop: int = Field(alias="pass") # 0: No, 1: Yes
    reverse_state: int = Field(alias="reverseState") # 0: No, 1: Reverse flow
    soc_status: int = Field(alias="socStatus") # 0: No, 1: Calibrating
    hyper_temperature_raw: int = Field(alias="hyperTmp") # Enclosure temperature
    grid_off_power: int = Field(alias="gridOffPower") #
    dc_status: int = Field(alias="dcStatus") # 0: Stopped, 1: Battery input, 2: Battery output
    pv_status: int = Field(alias="pvStatus") # 0: Stopped, 1: Running
    ac_status: int = Field(alias="acStatus") # 0: Stopped, 1: Grid-connected operation, 2: Charging operation
    data_ready: int = Field(alias="dataReady") # 0: Not ready, 1: Ready
    grid_state: int = Field(alias="gridState") # 0: Not connected, 1: Connected
    battery_voltage_raw: int = Field(alias="BatVolt", default=0) #
    soc_limit: int = Field(alias="socLimit") # 0: Normal state, 1: Charge limit reached, 2: Discharge limit reached
    fault_level: int = Field(alias="faultLevel") #
    write_rsp: int = Field(alias="writeRsp") # Read/write response acknowledgment
    ac_mode: int = Field(alias="acMode") # 1: Input, 2: Output
    soc_set_raw: int = Field(alias="socSet") # 700-1000: 70%-100%
    min_soc_raw: int = Field(alias="minSoc") # 	0-500: 0%-50%
    input_limit: int = Field(alias="inputLimit") # AC charging power limit
    output_limit: int = Field(alias="outputLimit") # Output power limit
    grid_standard: int = Field(alias="gridStandard") # Grid connection standard 0: Germany 1: France 2: Austria
    grid_reverse: int = Field(alias="gridReverse") # 0: Disabled, 1: Allowed reverse flow, 2: Forbidden reverse flow
    inverse_max_power: int = Field(alias="inverseMaxPower") # Maximum output power limit
    lamp_switch: int = Field(alias="lampSwitch") #
    grid_off_mode: int = Field(alias="gridOffMode") #
    iot_state: int = Field(alias="IOTState") #
    fan_switch: int = Field(alias="fanSwitch") #
    fan_speed: int = Field(alias="fanSpeed") #
    bind_state: int = Field(alias="bindstate") #
    volt_wakeup: int = Field(alias="VoltWakeup") #
    old_mode: int = Field(alias="OldMode") #
    ota_state: int = Field(alias="OTAState") #
    factory_mode_state: int = Field(alias="factoryModeState") #
    timestamp: int = Field(alias="ts") # timestamp
    ts_zone: int = Field(alias="tsZone") # time zone
    smart_mode: int = Field(alias="smartMode") # 1: The setting parameter is not written to flash. After an unexpected power loss and restart, the device will use the value stored in flash. 0: The setting parameter is written to flash. If you frequently set device properties, set this to 1.
    charge_max_limit: int = Field(alias="chargeMaxLimit") # 
    phase_switch: int = Field(alias="phaseSwitch") #
    rssi: int #
    is_error: int #
    ac_coupling_state: int = Field(alias="acCouplingState", default=0)
    dry_node_state: int = Field(alias="dryNodeState", default=0)
    fm_volt: int = Field(alias="FMVolt", default=0)
    time_zone_str: str = Field(alias="timeZone", default="UTC")
    bat_cal_time: int = Field(alias="batCalTime", default=0)

    @property
    def soc_set(self) -> float:
        return self.soc_set_raw / 10.0

    @property
    def min_soc(self) -> float:
        return self.min_soc_raw / 10.0

    @property
    def hyper_temperature(self) -> float:
        return self.hyper_temperature_raw / 100.0

    @property
    def battery_voltage(self) -> float:
        return self.battery_voltage_raw / 100.0

class SolarFlowReport(BaseModel):
    timestamp: int
    sn: str
    product: str
    version: int
    properties: GlobalProperties
    pack_data: List[BatteryPackProps] = Field(alias="packData")
