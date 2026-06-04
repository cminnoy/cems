#pragma once

#include <schema/ime_generated.h>

namespace CEMS::IME {

namespace Verifier {

constexpr int phases = 3;
constexpr float max_ampere_per_phase = 32.0f;
constexpr float max_ampere = max_ampere_per_phase * phases;
constexpr float peak_voltage_per_phase = 246.0f;
constexpr float max_va_per_phase = peak_voltage_per_phase * max_ampere_per_phase;
constexpr float max_va_total = max_va_per_phase * phases;
constexpr float max_injection = 10000.0f;

bool Check(CEMS::IME::InstantReading const & o) {
    bool const result =
        o.frequency() >= 0.0f and o.frequency() <= 60.0f and
        o.tri_phase_active_power() >= -16170.0f and o.tri_phase_active_power() <= max_va_total and
        o.l1_l2_voltage() >= 0.0f and o.l1_l2_voltage() <= 500.0f and
        o.l2_l3_voltage() >= 0.0f and o.l2_l3_voltage() <= 500.0f and
        o.l3_l1_voltage() >= 0.0f and o.l3_l1_voltage() <= 500.0f and
        o.phase1_active_power() >= -5390.0f and o.phase1_active_power() <= max_va_per_phase and
        o.phase1_current() >= 0.0f and o.phase1_current() <= max_ampere_per_phase and
        o.phase1_voltage() >= 0.0f and o.phase1_voltage() <= 500.0f and
        o.phase2_active_power() >= -5390.0f and o.phase2_active_power() <= max_va_per_phase and
        o.phase2_current() >= 0.0f and o.phase2_current() <= max_ampere_per_phase and
        o.phase2_voltage() >= 0.0f and o.phase2_voltage() <= 500.0f and
        o.phase3_active_power() >= -5390.0f and o.phase3_active_power() <= max_va_per_phase and
        o.phase3_current() >= 0.0f and o.phase3_current() <= max_ampere_per_phase and
        o.phase3_voltage() >= 0.0f and o.phase3_voltage() <= 500.0f and
        true
        ;
    if (not result) std::cerr << "CEMS::IME::InstantReading check did not pass!" << std::endl;
    return result;
}

bool Check(CEMS::IME::EnergyReading const & o) {
    bool const result =
        o.tri_phase_total_positive_active_energy() >= 0.0f and
        o.tri_phase_total_positive_reactive_energy() >= 0.0f and
        o.tri_phase_partial_positive_active_energy() >= 0.0f and
        o.tri_phase_partial_positive_reactive_energy() >= 0.0f and
        o.tri_phase_total_positive_active_energy2() >= 0.0f and
        o.tri_phase_total_positive_reactive_energy2() >= 0.0f and
        o.tri_phase_partial_second_tariff_positive_active_energy() >= 0.0f and
        o.tri_phase_partial_second_tariff_positive_reactive_energy() >= 0.0f and
        true
        ;
    if (not result) std::cerr << "CEMS::IME::EnergyReading check did not pass!" << std::endl;
    return result;
}

} // namespace Verifier

} // namespace CEMS::IME
