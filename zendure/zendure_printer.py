class ZendurePrinter:
    def __init__(self, report):
        self.report = report
        self._GRID_STANDARDS = {
            0: "GERMANY", 1: "FRANCE", 2: "AUSTRIA", 3: "SWITZERLAND",
            4: "NETHERLANDS", 5: "SPAIN", 6: "BELGIUM", 7: "GREECE",
            8: "DENMARK", 9: "ITALY"
        }

    def _parse_ac_coupling_state(self, state_val: int) -> str:
        """Parses the AC coupling bitmask to human-readable flags."""
        if not state_val:
            return "None"
        flags = []
        if state_val & (1 << 0): flags.append("AC-Input Present (Auto-Clear)")
        if state_val & (1 << 1): flags.append("AC-Input Present")
        if state_val & (1 << 2): flags.append("AC Overload")
        if state_val & (1 << 3): flags.append("Excess AC Power")
        return " | ".join(flags) if flags else f"Unknown ({state_val})"

    def get_full_diagnostic(self, with_pv=False, with_packs=False):
        r = self.report
        p = r.properties
        grid_std_name = self._GRID_STANDARDS.get(p.grid_standard, f"UNKNOWN ({p.grid_standard})")

        print(f"Timestamp: {r.timestamp}")
        print(f"Device: {r.product} SN {r.sn} (v{r.version})")
        print(f"Timezone: {p.time_zone_str}")
        print(f"Temperature: {p.hyper_temperature}°C State: {p.heat_state}")
        print(f"Global SOC: {p.electric_level}% Status: {p.soc_status} Limit: {p.soc_set}% | Min: {p.min_soc}%")
        print(f"Power In: {p.input_pack}W Out: {p.output_pack}W Solar: {p.solar_total}W Home: {p.output_home}W Grid: {p.grid_input_power}W")
        print(f"Pack Num: {p.pack_num} State: {p.pack_state}")
        print(f"State AC: {p.ac_status} DC: {p.dc_status} PV: {p.pv_status}")
        print(f"Remaining discharge time: {p.remain_outtime}s")
        print(f"Charge Max Limit: {p.charge_max_limit}W")
        print(f"Fan Switch: {p.fan_switch} Speed: {p.fan_speed}")
        print(f"RSSI: {p.rssi}dB")
        print(f"Is Error: {p.is_error} Fault Level: {p.fault_level}")
        print(f"Grid Standard: {grid_std_name} (Reverse Allowed: {p.grid_reverse})")
        print(f"Dry Node State: {'Connected' if p.dry_node_state == 1 else 'Disconnected'} ({p.dry_node_state})")
        print(f"Activation Threshold (FMVolt): {p.fm_volt}V")
        print(f"AC Coupling Flags: {self._parse_ac_coupling_state(p.ac_coupling_state)}")

        if with_pv:
            print()
            print("Photovoltaic:")
            print(f"  - Line 1: {p.pv1}W")
            print(f"  - Line 2: {p.pv2}W")
            print(f"  - Line 3: {p.pv3}W")
            print(f"  - Line 4: {p.pv4}W")
            print(f"  - Line 5: {p.pv5}W")
            print(f"  - Line 6: {p.pv6}W")
        if with_packs:
            print()
            print("Packs:")
            for pack in self.report.pack_data:
                print(f"  - Pack {pack.sn}: {pack.soc_level}%")
                print(f"    Temp: {pack.max_temperature}°C")
                print(f"    Firmware: {pack.soft_version}")
                print(f"    State: {pack.state}")
                print(f"    Heat state: {pack.heat_state}")
                print(f"    Power: {pack.power}W")
                print(f"    Total voltage.: {pack.total_voltage}V")
                print(f"    Max voltage: {pack.max_voltage}V")
                print(f"    Min voltage: {pack.min_voltage}V")
