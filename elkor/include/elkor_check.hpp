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
    bool const result =
        o.frequency() >= 0.0f and o.frequency() <= 60.0f and
        o.total_real_power() >= -max_injection and o.total_real_power() <= max_va_total and
        o.total_system_power_factor() >= -1.0f and o.total_system_power_factor() <= 1.0f and
        o.total_current() >= 0.0f and o.total_current() <= max_ampere and
        o.phase_a_real_power() >= -5390.0f and o.phase_a_real_power() <= max_va_per_phase and
        o.phase_a_current() >= 0.0f and o.phase_a_current() <= max_ampere_per_phase and
        o.phase_a_voltage_to_neutral() >= 0.0f and o.phase_a_voltage_to_neutral() <= 500.0f and
        o.phase_b_real_power() >= -5390.0f and o.phase_b_real_power() <= max_va_per_phase and
        o.phase_b_current() >= 0.0f and o.phase_b_current() <= max_ampere_per_phase and
        o.phase_b_voltage_to_neutral() >= 0.0f and o.phase_b_voltage_to_neutral() <= 500.0f and
        o.phase_c_real_power() >= -5390.0f and o.phase_c_real_power() <= max_va_per_phase and
        o.phase_c_current() >= 0.0f and o.phase_c_current() <= max_ampere_per_phase and
        o.phase_c_voltage_to_neutral() >= 0.0f and o.phase_c_voltage_to_neutral() <= 500.0f and
        true
        ;
    if (not result) std::cerr << "CEMS::Elkor::InstantReading check did not pass!" << std::endl;
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
