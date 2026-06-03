#include <schema/circutor_generated.h>

namespace CEMS::Circutor {

namespace Verifier {

bool Check(CEMS::Circutor::InstantReading const & o) {
    bool const result =
        o.phase1_voltage() >= 0.0f and o.phase1_voltage() <= 400.0f and
        o.phase2_voltage() >= 0.0f and o.phase2_voltage() <= 400.0f and
        o.phase3_voltage() >= 0.0f and o.phase3_voltage() <= 400.0f and
        o.phase1_current() >= -32.0f and o.phase1_current() <= 32.0f and
        o.phase2_current() >= -32.0f and o.phase2_current() <= 32.0f and
        o.phase3_current() >= -32.0f and o.phase3_current() <= 32.0f and
        o.phase1_cos_phi() >= 0.0f and o.phase1_cos_phi() <= 1.0f and
        o.phase2_cos_phi() >= 0.0f and o.phase2_cos_phi() <= 1.0f and
        o.phase3_cos_phi() >= 0.0f and o.phase3_cos_phi() <= 1.0f and
        o.phase1_active_power() >= -7840.0f and o.phase1_active_power() <= 7840.0f and
        o.phase2_active_power() >= -7840.0f and o.phase2_active_power() <= 7840.0f and
        o.phase3_active_power() >= -7840.0f and o.phase3_active_power() <= 7840.0f and
        o.total_active_power() >= -23520.0f and o.total_active_power() <= 23520.0f and
        true;
    if (not result) std::cerr << "CEMS::Circutor::InstantReading check did not pass!" << std::endl;
    return result;
}

bool Check(CEMS::Circutor::EnergyReading const & o) {
    bool const result =
        o.imported_active_energy() >= 0 and
        o.exported_active_energy() >= 0 and
        o.q1_reactive_energy() >= 0 and
        o.q2_reactive_energy() >= 0 and
        o.q3_reactive_energy() >= 0 and
        o.q4_reactive_energy() >= 0 and
        true;
    if (not result) std::cerr << "CEMS::Circutor::EnergyReading check did not pass!" << std::endl;
    return result;
}

} // namespace Verifier

} // namespace CEMS::Circutor