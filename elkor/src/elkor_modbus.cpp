/* Copyright 2012-2026 Chris Minnoy */

#include <elkor_modbus.hpp>
#include <modbus/modbus.h>

// C++
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>
#include <new>
#include <thread>
#include <iostream>

// TODO Remove new/delete and use a std::pmr container

// Auxilary functions
// ==================

std::ostream & operator<<(std::ostream & out, const ElkorModbusInterface & data) {
	WattsOnModbusInterface * itt = data.GetFirstDevice();
	while (itt) {
		out << '{' << itt->Port() << ',' << itt->SlaveNumber() << ',' << itt->BaudRate() << '}' << std::endl;
		itt = data.GetNextDevice(itt);
	}
	return out;
}

std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnFloat &data) {
    char s[256];
    out <<          "|- Float --------------------------------------------------------------------------------------------------------------|" << std::endl;
    out <<          "| Indicator                         |       Total      |      Phase A     |      Phase B     |      Phase C     | Unit |" << std::endl;
    out <<          "|----------------------------------------------------------------------------------------------------------------------|" << std::endl;
    std::sprintf(s, "| Software Version                  |        %6.1f    |                  |                  |                  |      |", data.SoftwareVersion());
    out << s << std::endl;
    std::sprintf(s, "| Sliding Window Real Power Demand  |       %5.0f      |                  |                  |                  |   WD |", data.SlidingWindowRealPowerDemand());
    out << s << std::endl;
    std::sprintf(s, "| Frequency                         |        %6.1f    |                  |                  |                  |   Hz |", data.Frequency());
    out << s << std::endl;
    std::sprintf(s, "| Voltage line to neutral           |(Av.)   %6.1f    |        %6.1f    |        %6.1f    |        %6.1f    |    V |", data.AverageVoltageLineNeutral(), data.VoltagePhaseA2N(), data.VoltagePhaseB2N(), data.VoltagePhaseC2N());    
    out << s << std::endl;
    std::sprintf(s, "| Voltage line to line              |(Av.)   %6.1f    |(A-B)   %6.1f    |(B-C)   %6.1f    |(A-C)   %6.1f    |    V |", data.AverageVoltageLineLine(), data.VoltagePhaseA2B(), data.VoltagePhaseB2C(), data.VoltagePhaseA2C());
    out << s << std::endl;
    std::sprintf(s, "| Current                           |(Av.)     %6.3f  |          %6.3f  |          %6.3f  |          %6.3f  |    A |", data.AverageCurrent(), data.CurrentPhaseA(), data.CurrentPhaseB(), data.CurrentPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Real Power                        |(Tot.)%6.0f      |       %7.1f    |       %7.1f    |       %7.1f    |    W |", data.TotalRealPower(), data.RealPowerPhaseA(), data.RealPowerPhaseB(), data.RealPowerPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Reactive Power                    |(Tot,)%6.0f      |        %6.1f    |        %6.1f    |        %6.1f    |  VAR |", data.TotalReactivePower(), data.ReactivePowerPhaseA(), data.ReactivePowerPhaseB(), data.ReactivePowerPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Apparent Power                    |(Tot.)%6.0f      |        %6.1f    |        %6.1f    |        %6.1f    |   VA |", data.TotalApparentPower(), data.ApparentPowerPhaseA(), data.ApparentPowerPhaseB(), data.ApparentPowerPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Power Factor                      |          %7.4f |          %7.4f |          %7.4f |          %7.4f |      |", data.TotalSystemPowerFactor(), data.PowerFactorPhaseA(), data.PowerFactorPhaseB(), data.PowerFactorPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Import Energy                     | %11.0f      | %11.0f      | %11.0f      | %11.0f      |   Wh |", data.TotalImportEnergy(), data.ImportEnergyPhaseA(), data.ImportEnergyPhaseB(), data.ImportEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Export Energy                     | %11.0f      | %11.0f      | %11.0f      | %11.0f      |   Wh |", data.TotalExportEnergy(), data.ExportEnergyPhaseA(), data.ExportEnergyPhaseB(), data.ExportEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Net Energy                        | %11.0f      | %11.0f      | %11.0f      | %11.0f      |   Wh |", data.NetTotalEnergy(), data.NetEnergyPhaseA(), data.NetEnergyPhaseB(), data.NetEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Inductive Energy                  | %11.0f      | %11.0f      | %11.0f      | %11.0f      | VARh |", data.TotalInductiveEnergy(), data.InductiveEnergyPhaseA(), data.InductiveEnergyPhaseB(), data.InductiveEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Capacitive Energy                 | %11.0f      | %11.0f      | %11.0f      | %11.0f      | VARh |", data.TotalCapacitiveEnergy(), data.CapacitiveEnergyPhaseA(), data.CapacitiveEnergyPhaseB(), data.CapacitiveEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Net Reactive Energy               | %11.0f      | %11.0f      | %11.0f      | %11.0f      | VARh |", data.NetTotalReactiveEnergy(), data.NetReactiveEnergyPhaseA(), data.NetReactiveEnergyPhaseB(), data.NetReactiveEnergyPhaseC());
    out << s << std::endl;
    std::sprintf(s, "| Apparent Energy                   | %11.0f      | %11.0f      | %11.0f      | %11.0f      |  VAh |", data.TotalApparentEnergy(), data.ApparentEnergyPhaseA(), data.ApparentEnergyPhaseB(), data.ApparentEnergyPhaseC());
    out << s << std::endl;
    out <<          "|----------------------------------------------------------------------------------------------------------------------|" << std::endl;
    return out;
}

std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnConfiguration &data) {
    char s[160];
    char s_true[] = "true";
    char s_false[] = "false";
    out <<          "|- Configuration --------------------------|" << std::endl;
    std::sprintf(s, "| PT ratio primary     |             %5hu |", data.PTRatioPrimary());
    out << s << std::endl;
    std::sprintf(s, "| PT ratio secondary   |             %5hu |", data.PTRatioSecondary());
    out << s << std::endl;
    std::sprintf(s, "| CT ratio primary     |             %5hu |", data.CTRatioPrimary());
    out << s << std::endl;
    std::sprintf(s, "| CT ratio secondary   |             %5hu |", data.CTRatioSecondary());
    out << s << std::endl;
    std::sprintf(s, "| Demand period        |             %5hu |", data.DemandPeriod());
    out << s << std::endl;
    std::sprintf(s, "| Debug                |             %5hu |", data.Debug());
    out << s << std::endl;
    std::sprintf(s, "| Pulse value          |             %5hu |", data.PulseValue());
    out << s << std::endl;
    std::sprintf(s, "| Output A source      |            %#06hx |", data.OutputASource());
    out << s << std::endl;
    std::sprintf(s, "| Output B source      |            %#06hx |", data.OutputBSource());
    out << s << std::endl;
    std::sprintf(s, "| Output A 0V value    |            %6hd |", data.OutputA0VValue());
    out << s << std::endl;
    std::sprintf(s, "| Output B 0V value    |            %6hd |", data.OutputB0VValue());    
    out << s << std::endl;
    std::sprintf(s, "| Output A 10V value   |            %6hd |", data.OutputA10VValue());
    out << s << std::endl;
    std::sprintf(s, "| Output B 10V value   |            %6hd |", data.OutputB10VValue());    
    out << s << std::endl;
    std::sprintf(s, "| Configuration Word   |            %#06hx |", data.ConfigurationWord());
    out << s << std::endl;
    std::sprintf(s, "| Output Two Selection | %17s |", data.OutputTwoSelection() ? "power flow" : "VARh accumulation");
    out << s << std::endl;
    std::sprintf(s, "| Reverse W sign       | %17s |", data.OutputTwoSelection() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Reverse VAR sign     | %17s |", data.OutputTwoSelection() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Reverse sequence     | %17s |", data.ReverseSequence() ? "true (ABC)" : "false (ACB)");
    out << s << std::endl;
    std::sprintf(s, "| Disable sequence     | %17s |", data.DisableSequence() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Force W absolute     | %17s |", data.ForceWattAbsolute() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Force VAR absolute   | %17s |", data.ForceVARAbsolute() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Force PF absolute    | %17s |", data.ForcePFAbsolute() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Pulse on +Wh         | %17s |", data.PulseOnWhIncrement() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Pulse on -Wh         | %17s |", data.PulseOnWhDecrement() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Pulse on +VARh       | %17s |", data.PulseOnVARhIncrement() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| Pulse on -VARh       | %17s |", data.PulseOnVARhDecrement() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| No voltage LED flash | %17s |", data.NoVoltageLedFlash() ? s_true : s_false);
    out << s << std::endl;
    std::sprintf(s, "| 32bit Word order     | %17s |", data.WordOrder32bit() ? "LSB-MSB" : "MSB-LSB");
    out << s << std::endl;
    std::sprintf(s, "| V/I Average          | %17s |", data.AverageVoltageCurrent() ? "calc as total" : "calc as averages");
    out << s << std::endl;
    std::sprintf(s, "| Split +240V Load     | %17s |", data.Split240VLoad() ? s_true : s_false);
    out << s << std::endl;
    out <<          "|------------------------------------------|" << std::endl;
    return out;
}

std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnScratchPad &data) {
    char s[160];
    out <<          "|- ScratchPad ------------------------------|" << std::endl;
    std::sprintf(s, "| Serial number | %5hu |        |          |", data.SerialNumber());
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 1 | %5hu | %#6x | '%c', '%c' |", data.Pad1(), data.Pad1(), (char)(data.Pad1() >> 8), (char)(data.Pad1() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 2 | %5hu | %#6x | '%c', '%c' |", data.Pad2(), data.Pad2(), (char)(data.Pad2() >> 8), (char)(data.Pad2() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 3 | %5hu | %#6x | '%c', '%c' |", data.Pad3(), data.Pad3(), (char)(data.Pad3() >> 8), (char)(data.Pad3() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 4 | %5hu | %#6x | '%c', '%c' |", data.Pad4(), data.Pad4(), (char)(data.Pad4() >> 8), (char)(data.Pad4() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 5 | %5hu | %#6x | '%c', '%c' |", data.Pad5(), data.Pad5(), (char)(data.Pad5() >> 8), (char)(data.Pad5() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 6 | %5hu | %#6x | '%c', '%c' |", data.Pad6(), data.Pad6(), (char)(data.Pad6() >> 8), (char)(data.Pad6() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 7 | %5hu | %#6x | '%c', '%c' |", data.Pad7(), data.Pad7(), (char)(data.Pad7() >> 8), (char)(data.Pad7() & 0xFF));
    out << s << std::endl;
    std::sprintf(s, "| Scratch pad 8 | %5hu | %#6x | '%c', '%c' |", data.Pad8(), data.Pad8(), (char)(data.Pad8() >> 8), (char)(data.Pad8() & 0xFF));
    out << s << std::endl;
    out <<          "|-------------------------------------------|" << std::endl;
    return out;
}

std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnExtendedConfiguration &data) {
    char s[160];
    out <<          "|- Extended Configuration -------------------|" << std::endl;
    std::sprintf(s, "| Extended Configuration Word |       %#06hx |", data.ExtendedConfigurationWord());
    out << s << std::endl;
    std::sprintf(s, "| Output #2 Display Stream    | %12s |", data.OutputTwoDisplayStream() ? "WattsOn-DISP" : "pulse/relay");
    out << s << std::endl;
    std::sprintf(s, "| Pulse Output Type           | %12s |", data.PulseOutputType() ? "state change" : "100ms pulse");
    out << s << std::endl;
    std::sprintf(s, "| Baudrate selection          | %12s |", data.BaudRateSelection() ? "57600" : "9600");
    out << s << std::endl;
    out <<          "|--------------------------------------------|" << std::endl;
    return out;
}

constexpr std::uint32_t swap_lr(std::uint32_t v) {
   return (v << 16) | (v >> 16);
}

float swap_lr(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "float must be 4 bytes");
    std::uint32_t temp;
    std::memcpy(&temp, &value, sizeof(temp));
    temp = swap_lr(temp);
    std::memcpy(&value, &temp, sizeof(value));
    return value;
}

//
// WattsOnModBusInterface::WattsOnConfiguration
// ============================================

int WattsOnModbusInterface::WattsOnConfiguration::PTRatioPrimary(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_PT_RATIO_PRIMARY, value);
    if (mb_error == 1) pt_ratio_primary = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PTRatioSecondary(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_PT_RATIO_SECONDARY, value);
    if (mb_error == 1) pt_ratio_secondary = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::CTRatioPrimary(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CT_RATIO_PRIMARY, value);
	if (mb_error == 1) ct_ratio_primary = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::CTRatioSecondary(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CT_RATIO_SECONDARY, value);
	if (mb_error == 1) ct_ratio_secondary = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::DemandPeriod(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_DEMAND_PERIOD, value);
    if (mb_error == 1) demand_period = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PulseValue(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_PULSE_VALUE, value);
    if (mb_error == 1) pulse_value = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputASource(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_A_SOURCE, value);
    if (mb_error == 1) output_a_source = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputBSource(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_B_SOURCE, value);
    if (mb_error == 1) output_b_source = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputA0VValue(std::int16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_A_0V_VALUE, value);
    if (mb_error == 1) output_a_0v_value = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputB0VValue(std::int16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_B_0V_VALUE, value);
    if (mb_error == 1) output_b_0v_value = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputA10VValue(std::int16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_A_10V_VALUE, value);
    if (mb_error == 1) output_a_10v_value = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputB10VValue(std::int16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_OUTPUT_B_10V_VALUE, value);
    if (mb_error == 1) output_b_10v_value = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ConfigurationWord(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, value);
    if (mb_error == 1) configuration_word.word = value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::OutputTwoSelection(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.output_2_selection = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ReverseWattSign(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.reverse_watt_sign = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ReverseVARSign(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.reverse_var_sign = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ReverseSequence(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.reverse_sequence = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::DisableSequence(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.disable_sequence = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ForceWattAbsolute(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.force_watt_absolute = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ForceVARAbsolute(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.force_var_absolute = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::ForcePFAbsolute(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.force_pf_absolute = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PulseOnWhIncrement(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.pulse_on_wh_increment = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PulseOnWhDecrement(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.pulse_on_wh_decrement = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PulseOnVARhIncrement(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.pulse_on_varh_increment = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::PulseOnVARhDecrement(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.pulse_on_varh_decrement = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::NoVoltageLedFlash(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.no_voltage_led_flash = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::WordOrder32bit(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.word_order = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::AverageVoltageCurrent(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.average_voltage_current = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnConfiguration::Split240VLoad(bool value) {
    const uint16_t old_value = configuration_word.word;
    configuration_word.bit.split_240v_load = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_CONFIGURATION_WORD, configuration_word.word);
    if (mb_error < 0) configuration_word.word = old_value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}


//
// WattsOnModBusInterface::WattsOnScratchPad
// =========================================

int WattsOnModbusInterface::WattsOnScratchPad::Pad1(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD1, value);
    if (mb_error == 1) scratch_pad_1 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad2(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD2, value);
    if (mb_error == 1) scratch_pad_2 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad3(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD3, value);
    if (mb_error == 1) scratch_pad_3 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad4(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD4, value);
    if (mb_error == 1) scratch_pad_4 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad5(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD5, value);
    if (mb_error == 1) scratch_pad_5 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad6(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD6, value);
    if (mb_error == 1) scratch_pad_6 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad7(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD7, value);
    if (mb_error == 1) scratch_pad_7 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnScratchPad::Pad8(std::uint16_t value) {
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_SCRATCHPAD8, value);
    if (mb_error == 1) scratch_pad_8 = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}


//
// WattsOnModBusInterface::WattsOnExtendedConfiguration
// ====================================================

int WattsOnModbusInterface::WattsOnExtendedConfiguration::ExtendedConfigurationWord(std::uint16_t value) {
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_EXTENDED_CONFIGURATION_WORD, value);
    if (mb_error >= 0) extended_configuration_word.word = value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnExtendedConfiguration::OutputTwoDisplayStream(bool value) {
    const uint16_t old_value = extended_configuration_word.word;
    extended_configuration_word.bit.output_2_display_stream = value;
	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_EXTENDED_CONFIGURATION_WORD, extended_configuration_word.word);
    if (mb_error < 0) extended_configuration_word.word = old_value;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return mb_error;
}

int WattsOnModbusInterface::WattsOnExtendedConfiguration::PulseOutputType(bool value) {
    const uint16_t old_value = extended_configuration_word.word;
    extended_configuration_word.bit.pulse_output_type = value;
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_EXTENDED_CONFIGURATION_WORD, extended_configuration_word.word);
    if (mb_error < 0) extended_configuration_word.word = old_value;
    return mb_error;
}

int WattsOnModbusInterface::WattsOnExtendedConfiguration::BaudRateSelection(bool value) {
    const uint16_t old_value = extended_configuration_word.word;
    extended_configuration_word.bit.baud_rate_selection = value;
 	int mb_error = modbus_write_register(parent_->mb_ctx_, WATTSON_INT_EXTENDED_CONFIGURATION_WORD, extended_configuration_word.word);
    if (mb_error < 0) extended_configuration_word.word = old_value;
    return mb_error;
}


//
// WattsOnModBusInterface
// ======================

WattsOnModbusInterface::WattsOnModbusInterface()
: wattson_configuration(this)
, wattson_scratchpad(this)
, wattson_extended_configuration(this)
{}

WattsOnModbusInterface::~WattsOnModbusInterface() noexcept {
    Disconnect();
}

int WattsOnModbusInterface::Connect(const char linux_device_driver[], const int baudrate, const int slave_number) {
    //* Init modbus structure
    mb_ctx_ = modbus_new_rtu(linux_device_driver, baudrate, 'N', 8, 1);
    int mb_error = modbus_rtu_set_serial_mode(mb_ctx_, MODBUS_RTU_RS485);
    //if (!modbus_set_byte_timeout(mb_ctx_, 0, 2000)) return -1;
    //if (!modbus_set_response_timeout(mb_ctx_, 0, 100'000)) return -1;
    slave_number_ = slave_number;
    baudrate_ = baudrate;
	port_ = linux_device_driver;
    //* Connect to device
    mb_error = modbus_connect(mb_ctx_);
    if (mb_error == 0) {
        uint16_t value;
        //* Read debug register to check live communication
		mb_error = modbus_set_slave(mb_ctx_, slave_number);
		if (mb_error != 0) return -1;
        mb_error = modbus_read_registers(mb_ctx_, WATTSON_INT_DEBUG, 1, &value);
        if (mb_error != 1) return mb_error;
	    //* Check known debug register value
        if (value != 0x3039) return -1;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
        //* Read configuration registers
        mb_error = ReadWattsOnConfiguration();
        if (mb_error != 15) return -1;
        swap_msb_lsb_ = wattson_configuration.WordOrder32bit();
        pt_ratio_ = wattson_configuration.PTRatioPrimary() / wattson_configuration.PTRatioSecondary();
        ct_ratio_ = wattson_configuration.CTRatioPrimary() / wattson_configuration.CTRatioSecondary();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

        //* Read scratchpad registers        
        mb_error = ReadWattsOnScratchPad();
        if (mb_error != 9) return -1;
#if __BYTE_ORDER == __LITTLE_ENDIAN
        if (wattson_scratchpad.Pad1() == ('L' << 8 | 'E') and
            wattson_scratchpad.Pad2() == ('O' << 8 | 'K') and
            wattson_scratchpad.Pad3() == ('-' << 8 | 'R') and
            wattson_scratchpad.Pad4() == ('A' << 8 | 'W') and
            wattson_scratchpad.Pad5() == ('T' << 8 | 'T') and
            wattson_scratchpad.Pad6() == ('O' << 8 | 'S') and
            wattson_scratchpad.Pad7() == ('-' << 8 | 'N') and
            (wattson_scratchpad.Pad8() >>   8) >= '0' and (wattson_scratchpad.Pad8() >>   8) <= '9' and
            (wattson_scratchpad.Pad8() & 0xFF) >= '0' and (wattson_scratchpad.Pad8() & 0xFF) <= '9')
        {
            //han_linked_ = true;
        }
        else {
            //han_linked_ = false;
        }
#else
        if (wattson_scratchpad.Pad1() == ('E' << 8 | 'L') and
            wattson_scratchpad.Pad2() == ('K' << 8 | 'O') and
            wattson_scratchpad.Pad3() == ('R' << 8 | '-') and
            wattson_scratchpad.Pad4() == ('W' << 8 | 'A') and
            wattson_scratchpad.Pad5() == ('T' << 8 | 'T') and
            wattson_scratchpad.Pad6() == ('S' << 8 | 'O') and
            wattson_scratchpad.Pad7() == ('N' << 8 | '-') and
            (wattson_scratchpad.Pad8() >>   8) >= '0' and (wattson_scratchpad.Pad8() >>   8) <= '9' and
            (wattson_scratchpad.Pad8() & 0xFF) >= '0' and (wattson_scratchpad.Pad8() & 0xFF) <= '9')
        {
            //han_linked_ = true;
        } // if
        else {
            //han_linked_ = false;
        }
#endif
        return 0;
    }
    else {
        Disconnect();
    }
    return mb_error;
}

int WattsOnModbusInterface::CheckConnection() {
    std::uint16_t value;
    int mb_error = modbus_read_registers(mb_ctx_, WATTSON_INT_DEBUG, 1, &value);
    if (mb_error != 1) return mb_error;
    if (value != 0x3039) return -1;
    return 0;
}

void WattsOnModbusInterface::Disconnect() {
    modbus_close(mb_ctx_);
	modbus_free(mb_ctx_);
    mb_ctx_ = nullptr;
}

void WattsOnModbusInterface::EnableDebug() {
    modbus_set_debug(mb_ctx_, true);
}

void WattsOnModbusInterface::DisableDebug() {
    modbus_set_debug(mb_ctx_, false);
}

char const * WattsOnModbusInterface::Error(int error_number) const {
    return modbus_strerror(error_number);
}

bool WattsOnModbusInterface::ReadWattsOnFloatInstant() {
    if (-1 == modbus_read_registers(mb_ctx_, WATTSON_FLOAT_TOTAL_REAL_POWER, 60, reinterpret_cast<uint16_t*>(&wattson_float.total_real_power))) return false;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    wattson_float.total_real_power = swap_lr(wattson_float.total_real_power);
    wattson_float.total_reactive_power = swap_lr(wattson_float.total_reactive_power);
    wattson_float.total_apparent_power = swap_lr(wattson_float.total_apparent_power);
    wattson_float.average_voltage_l_n = swap_lr(wattson_float.average_voltage_l_n);
    wattson_float.average_voltage_l_l = swap_lr(wattson_float.average_voltage_l_l);
    wattson_float.average_current = swap_lr(wattson_float.average_current);
    wattson_float.total_system_power_factor = swap_lr(wattson_float.total_system_power_factor);
    wattson_float.frequency = swap_lr(wattson_float.frequency);
    wattson_float.sliding_window_real_power_demand = swap_lr(wattson_float.sliding_window_real_power_demand);
    wattson_float.voltage_phase_a_n = swap_lr(wattson_float.voltage_phase_a_n);
    wattson_float.voltage_phase_b_n = swap_lr(wattson_float.voltage_phase_b_n);
    wattson_float.voltage_phase_c_n = swap_lr(wattson_float.voltage_phase_c_n);
    wattson_float.voltage_phase_a_b = swap_lr(wattson_float.voltage_phase_a_b);
    wattson_float.voltage_phase_b_c = swap_lr(wattson_float.voltage_phase_b_c);
    wattson_float.voltage_phase_a_c = swap_lr(wattson_float.voltage_phase_a_c);
    wattson_float.current_phase_a = swap_lr(wattson_float.current_phase_a);
    wattson_float.current_phase_b = swap_lr(wattson_float.current_phase_b);
    wattson_float.current_phase_c = swap_lr(wattson_float.current_phase_c);
    wattson_float.real_power_phase_a = swap_lr(wattson_float.real_power_phase_a);
    wattson_float.real_power_phase_b = swap_lr(wattson_float.real_power_phase_b);
    wattson_float.real_power_phase_c = swap_lr(wattson_float.real_power_phase_c);
    wattson_float.reactive_power_phase_a = swap_lr(wattson_float.reactive_power_phase_a);
    wattson_float.reactive_power_phase_b = swap_lr(wattson_float.reactive_power_phase_b);
    wattson_float.reactive_power_phase_c = swap_lr(wattson_float.reactive_power_phase_c);
    wattson_float.apparent_power_phase_a = swap_lr(wattson_float.apparent_power_phase_a);
    wattson_float.apparent_power_phase_b = swap_lr(wattson_float.apparent_power_phase_b);
    wattson_float.apparent_power_phase_c = swap_lr(wattson_float.apparent_power_phase_c);
    wattson_float.power_factor_phase_a = swap_lr(wattson_float.power_factor_phase_a);
    wattson_float.power_factor_phase_b = swap_lr(wattson_float.power_factor_phase_b);
    wattson_float.power_factor_phase_c = swap_lr(wattson_float.power_factor_phase_c);
#endif
    if (std::isnan(wattson_float.power_factor_phase_a)) wattson_float.power_factor_phase_a = 0.0f;
    if (std::isnan(wattson_float.power_factor_phase_b)) wattson_float.power_factor_phase_b = 0.0f;
    if (std::isnan(wattson_float.power_factor_phase_c)) wattson_float.power_factor_phase_c = 0.0f;
    if (std::isnan(wattson_float.total_system_power_factor)) {
        wattson_float.total_system_power_factor = (wattson_float.power_factor_phase_a + wattson_float.power_factor_phase_b + wattson_float.power_factor_phase_c) / 3.0;
    }
    return true;
}

bool WattsOnModbusInterface::ReadWattsOnFloatEnergy() {
    if (-1 == modbus_read_registers(mb_ctx_, WATTSON_FLOAT_IMPORT_ENERGY_PHASE_A, 56, reinterpret_cast<uint16_t*>(&wattson_float.import_energy_phase_a))) return false;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    wattson_float.import_energy_phase_a = swap_lr(wattson_float.import_energy_phase_a);
    wattson_float.import_energy_phase_b = swap_lr(wattson_float.import_energy_phase_b);
    wattson_float.import_energy_phase_c = swap_lr(wattson_float.import_energy_phase_c);
    wattson_float.total_import_energy = swap_lr(wattson_float.total_import_energy);
    wattson_float.export_energy_phase_a = swap_lr(wattson_float.export_energy_phase_a);
    wattson_float.export_energy_phase_b = swap_lr(wattson_float.export_energy_phase_b);
    wattson_float.export_energy_phase_c = swap_lr(wattson_float.export_energy_phase_c);
    wattson_float.total_export_energy = swap_lr(wattson_float.total_export_energy);
    wattson_float.net_energy_phase_a = swap_lr(wattson_float.net_energy_phase_a);
    wattson_float.net_energy_phase_b = swap_lr(wattson_float.net_energy_phase_b);
    wattson_float.net_energy_phase_c = swap_lr(wattson_float.net_energy_phase_c);
    wattson_float.net_total_energy = swap_lr(wattson_float.net_total_energy);
    wattson_float.inductive_energy_phase_a = swap_lr(wattson_float.inductive_energy_phase_a);
    wattson_float.inductive_energy_phase_b = swap_lr(wattson_float.inductive_energy_phase_b);
    wattson_float.inductive_energy_phase_c = swap_lr(wattson_float.inductive_energy_phase_c);
    wattson_float.total_inductive_energy = swap_lr(wattson_float.total_inductive_energy);
    wattson_float.capacitive_energy_phase_a = swap_lr(wattson_float.capacitive_energy_phase_a);
    wattson_float.capacitive_energy_phase_b = swap_lr(wattson_float.capacitive_energy_phase_b);
    wattson_float.capacitive_energy_phase_c = swap_lr(wattson_float.capacitive_energy_phase_c);
    wattson_float.total_capacitive_energy = swap_lr(wattson_float.total_capacitive_energy);
    wattson_float.net_reactive_energy_phase_a = swap_lr(wattson_float.net_reactive_energy_phase_a);
    wattson_float.net_reactive_energy_phase_b = swap_lr(wattson_float.net_reactive_energy_phase_b);
    wattson_float.net_reactive_energy_phase_c = swap_lr(wattson_float.net_reactive_energy_phase_c);
    wattson_float.net_total_reactive_energy = swap_lr(wattson_float.net_total_reactive_energy);
    wattson_float.apparent_energy_phase_a = swap_lr(wattson_float.apparent_energy_phase_a);
    wattson_float.apparent_energy_phase_b = swap_lr(wattson_float.apparent_energy_phase_b);
    wattson_float.apparent_energy_phase_c = swap_lr(wattson_float.apparent_energy_phase_c);
    wattson_float.total_apparent_energy = swap_lr(wattson_float.total_apparent_energy);
#endif
    return true;
}

int WattsOnModbusInterface::ReadWattsOnConfiguration() {
    int mb_error = modbus_read_registers(mb_ctx_, WATTSON_INT_PT_RATIO_PRIMARY, 15, reinterpret_cast<uint16_t*>(&wattson_configuration.pt_ratio_primary));
    return mb_error;
}

int WattsOnModbusInterface::ReadWattsOnScratchPad() {
    int mb_error = modbus_read_registers(mb_ctx_, WATTSON_INT_SERIAL_NUMBER, 9, reinterpret_cast<uint16_t*>(&wattson_scratchpad.serial_number));
    return mb_error;
}

int WattsOnModbusInterface::ReadWattsOnExtendedConfiguration() {
    int mb_error = modbus_read_registers(mb_ctx_, WATTSON_INT_EXTENDED_CONFIGURATION_WORD, 1, reinterpret_cast<uint16_t*>(&wattson_extended_configuration.extended_configuration_word.word));
    return mb_error;
}

int WattsOnModbusInterface::Reset() {
    //* Probably doesn't work, use jumper J1 instead
	std::this_thread::sleep_for(std::chrono::milliseconds(250)); 	
	int mb_error = modbus_write_register(mb_ctx_, WATTSON_INT_RESET, 0xA5A5);
    if (mb_error != 1) return mb_error;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	mb_error = modbus_write_register(mb_ctx_, WATTSON_INT_RESET, 0x5A5A);
	std::this_thread::sleep_for(std::chrono::milliseconds(250)); 	
    return mb_error;
}


//
// ElkorModbusInterface
// ====================

ElkorModbusInterface::ElkorModbusInterface()
{}

ElkorModbusInterface::~ElkorModbusInterface() {
	ClearDeviceList();
}

unsigned int ElkorModbusInterface::ScanRTUDevices(const char linux_device_driver[], unsigned int begin_slave_address, unsigned int end_slave_address) {
    unsigned int device_count = 0;
	assert(begin_slave_address <= end_slave_address);
	assert(end_slave_address <= 64);
    //* Scan at 9600 baud
    for (unsigned int slave_address = begin_slave_address; slave_address <= end_slave_address; ++slave_address) {
        WattsOnModbusInterface * device = new WattsOnModbusInterface;
        int mb_error = device->Connect(linux_device_driver, 9600, slave_address);
        if (mb_error == 0) {
            device->next_ = devices_;
            devices_ = device;
            ++device_count;
        }
        else {
            delete device;
        }
    }
    //* Scan at 57600 baud when no devices where found at 9600 baud
    if (device_count == 0) {
        for (unsigned int slave_address = 0; slave_address <= 64; ++slave_address) {
            WattsOnModbusInterface * device = new WattsOnModbusInterface;
            int mb_error = device->Connect(linux_device_driver, 57600, slave_address);
            if (mb_error == 0) {
                device->next_ = devices_;
                devices_ = device;
                ++device_count;
            }
            else {
                delete device;
            }
        }
    }
    return device_count;
}

void ElkorModbusInterface::ClearDeviceList() {
	WattsOnModbusInterface * itt = devices_;
	while (itt) {
		WattsOnModbusInterface * next = itt->next_;
		delete itt;
		itt = next;
	}
    devices_ = nullptr;
}

