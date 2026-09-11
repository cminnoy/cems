#pragma once

#include <schema/elkor_generated.h>

namespace CEMS::Elkor {

namespace Verifier {

constexpr int phases = 3;
constexpr float max_ampere_per_phase = 32.0f;
constexpr float max_ampere = max_ampere_per_phase * phases;
constexpr float peak_voltage_per_phase = 246.0f;
constexpr float max_va_per_phase = peak_voltage_per_phase * max_ampere_per_phase;
constexpr float max_va_total = max_va_per_phase * phases;
constexpr float max_injection = 10000.0f;

bool Check(CEMS::Elkor::InstantReading const & o) {
    char const * s = "";
    bool const frequency_check = o.frequency() >= 0.0f and o.frequency() <= 60.0f;
    bool const total_real_power_check = o.total_real_power() >= -max_injection and o.total_real_power() <= max_va_total;
    bool const total_system_power_factor_check = o.total_system_power_factor() >= -1.0f and o.total_system_power_factor() <= 1.0f;
    bool const total_current_check = o.total_current() >= 0.0f and o.total_current() <= max_ampere;
    bool const phase_a_real_power_check = o.phase_a_real_power() >= -5390.0f and o.phase_a_real_power() <= max_va_per_phase;
    bool const phase_a_current_check = o.phase_a_current() >= 0.0f and o.phase_a_current() <= max_ampere_per_phase;
    bool const phase_a_voltage_to_neutral_check = o.phase_a_voltage_to_neutral() >= 0.0f and o.phase_a_voltage_to_neutral() <= 500.0f;
    bool const phase_a_power_factor_check = o.phase_a_power_factor() >= -1.0f and o.phase_a_power_factor() <= 1.0f;
    bool const phase_b_real_power_check = o.phase_b_real_power() >= -5390.0f and o.phase_b_real_power() <= max_va_per_phase;
    bool const phase_b_current_check = o.phase_b_current() >= 0.0f and o.phase_b_current() <= max_ampere_per_phase;
    bool const phase_b_voltage_to_neutral_check = o.phase_b_voltage_to_neutral() >= 0.0f and o.phase_b_voltage_to_neutral() <= 500.0f;
    bool const phase_b_power_factor_check = o.phase_b_power_factor() >= -1.0f and o.phase_b_power_factor() <= 1.0f;
    bool const phase_c_real_power_check = o.phase_c_real_power() >= -5390.0f and o.phase_c_real_power() <= max_va_per_phase;
    bool const phase_c_current_check = o.phase_c_current() >= 0.0f and o.phase_c_current() <= max_ampere_per_phase;
    bool const phase_c_voltage_to_neutral_check = o.phase_c_voltage_to_neutral() >= 0.0f and o.phase_c_voltage_to_neutral() <= 500.0f;
    bool const phase_c_power_factor_check = o.phase_c_power_factor() >= -1.0f and o.phase_c_power_factor() <= 1.0f;
    if (!frequency_check) s = "frequency";
    if (!total_real_power_check) s = "total_real_power";
    if (!total_system_power_factor_check) s = "total_system_power_factor";
    if (!total_current_check) s = "total_current";
    if (!phase_a_real_power_check) s = "phase_a_real_power";
    if (!phase_a_current_check) s = "phase_a_current";
    if (!phase_a_voltage_to_neutral_check) s = "phase_a_voltage_to_neutral";
    if (!phase_a_power_factor_check) s = "phase_a_power_factor";
    if (!phase_b_real_power_check) s = "phase_b_real_power";
    if (!phase_b_current_check) s = "phase_b_current";
    if (!phase_b_voltage_to_neutral_check) s = "phase_b_voltage_to_neutral";
    if (!phase_b_power_factor_check) s = "phase_b_power_factor";
    if (!phase_c_real_power_check) s = "phase_c_real_power";
    if (!phase_c_current_check) s = "phase_c_current";
    if (!phase_c_voltage_to_neutral_check) s = "phase_c_voltage_to_neutral";
    if (!phase_c_power_factor_check) s = "phase_c_power_factor";
    bool const result =
        frequency_check and
        total_real_power_check and
        total_system_power_factor_check and
        total_current_check and
        phase_a_real_power_check and
        phase_a_current_check and
        phase_a_voltage_to_neutral_check and
        phase_a_power_factor_check and
        phase_b_real_power_check and
        phase_b_current_check and
        phase_b_voltage_to_neutral_check and
        phase_b_power_factor_check and
        phase_c_real_power_check and
        phase_c_current_check and
        phase_c_voltage_to_neutral_check and
        phase_c_power_factor_check;
    if (not result) std::cerr << "CEMS::Elkor::InstantReading check did not pass (" << s << ")." << std::endl;
    return result;
}

bool Check(CEMS::Elkor::EnergyReading const & o) {
    bool const result =
        o.total_import_energy() >= 0.0f and
        o.total_export_energy() >= 0.0f and
        o.phase_a_import_energy() >= 0.0f and
        o.phase_a_export_energy() >= 0.0f and
        o.phase_b_import_energy() >= 0.0f and
        o.phase_b_export_energy() >= 0.0f and
        o.phase_c_import_energy() >= 0.0f and
        o.phase_c_export_energy() >= 0.0f and
        true
        ;
    if (not result) std::cerr << "CEMS::Elkor::EnergyReading check did not pass!" << std::endl;
    return result;
}

} // namespace Verifier

} // namespace CEMS::Elkor
