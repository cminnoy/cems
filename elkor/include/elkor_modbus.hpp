/* Copyright 2012-2025 Chris Minnoy */

#pragma once

#ifndef ELKOR_MODBUS_HPP
#define ELKOR_MODBUS_HPP

// C++
#include <string>

struct _modbus;
typedef struct _modbus modbus_t;
class WattsOnModbusInterface;   // low level RTU device
class ElkorModbusInterface;     // RTU manager

/**
 * @brief Elkor MODBUS interface
 */
class WattsOnModbusInterface {
    friend class ElkorModbusInterface;
private:

    enum WattsOnAddress {
        // Integer
        // WATTSON_INT_TOTAL_ENERGY_CONSUMPTION            = 0x0000, // Wh  , 32-bit   signed, scale 1

        // WATTSON_INT_TOTAL_REAL_POWER                    = 0x0002, // W   , 16-bit   signed, scale 1
        // WATTSON_INT_TOTAL_REACTIVE_POWER                = 0x0003, // VAR , 16-bit   signed, scale 1
        // WATTSON_INT_TOTAL_APPARENT_POWER                = 0x0004, // VA  , 16-bit unsigned, scale 1
        // WATTSON_INT_AVERAGE_VOLTAGE_L_N                 = 0x0005, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_AVERAGE_VOLTAGE_L_L                 = 0x0006, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_AVERAGE_CURRENT                     = 0x0007, // A   , 16-bit unsigned, scale 1000
        // WATTSON_INT_TOTAL_SYSTEM_POWER_FACTOR           = 0x0008, //     , 16-bit   signed, scale 10000
        // WATTSON_INT_FREQUENCY                           = 0x0009, // Hz  , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_A_N                   = 0x000A, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_B_N                   = 0x000B, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_C_N                   = 0x000C, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_A_B                   = 0x000D, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_B_C                   = 0x000E, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_VOLTAGE_PHASE_A_C                   = 0x000F, // V   , 16-bit unsigned, scale 10
        // WATTSON_INT_CURRENT_PHASE_A                     = 0x0010, // A   , 16-bit unsigned, scale 1000
        // WATTSON_INT_CURRENT_PHASE_B                     = 0x0011, // A   , 16-bit unsigned, scale 1000
        // WATTSON_INT_CURRENT_PHASE_C                     = 0x0012, // A   , 16-bit unsigned, scale 1000
        // WATTSON_INT_REAL_POWER_PHASE_A                  = 0x0013, // W   , 16-bit   signed, scale 10
        // WATTSON_INT_REAL_POWER_PHASE_B                  = 0x0014, // W   , 16-bit   signed, scale 10
        // WATTSON_INT_REAL_POWER_PHASE_C                  = 0x0015, // W   , 16-bit   signed, scale 10
        // WATTSON_INT_REACTIVE_POWER_PHASE_A              = 0x0016, // VAR , 16-bit   signed, scale 10
        // WATTSON_INT_REACTIVE_POWER_PHASE_B              = 0x0017, // VAR , 16-bit   signed, scale 10
        // WATTSON_INT_REACTIVE_POWER_PHASE_C              = 0x0018, // VAR , 16-bit   signed, scale 10
        // WATTSON_INT_APPARENT_POWER_PHASE_A              = 0x0019, // VA  , 16-bit unsigned, scale 10
        // WATTSON_INT_APPARENT_POWER_PHASE_B              = 0x001A, // VA  , 16-bit unsigned, scale 10
        // WATTSON_INT_APPARENT_POWER_PHASE_C              = 0x001B, // VA  , 16-bit unsigned, scale 10
        // WATTSON_INT_POWER_FACTOR_PHASE_A                = 0x001C, //     , 16-bit   signed, scale 10000
        // WATTSON_INT_POWER_FACTOR_PHASE_B                = 0x001D, //     , 16-bit   signed, scale 10000
        // WATTSON_INT_POWER_FACTOR_PHASE_C                = 0x001E, //     , 16-bit   signed, scale 10000

        // WATTSON_INT_SOFTWARE_VERSION                    = 0x001F, //     , 16-bit unsigned, scale 10

        // WATTSON_INT_IMPORT_ENERGY_PHASE_A               = 0x0020, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_IMPORT_ENERGY_PHASE_B               = 0x0022, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_IMPORT_ENERGY_PHASE_C               = 0x0024, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_TOTAL_IMPORT_ENERGY                 = 0x0026, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_EXPORT_ENERGY_PHASE_A               = 0x0028, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_EXPORT_ENERGY_PHASE_B               = 0x002A, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_EXPORT_ENERGY_PHASE_C               = 0x002C, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_TOTAL_EXPORT_ENERGY                 = 0x002E, // Wh  , 32-bit unsigned, scale 1
        // WATTSON_INT_NET_TOTAL_ENERGY_PHASE_A            = 0x0030, // Wh  , 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_ENERGY_PHASE_B            = 0x0032, // Wh  , 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_ENERGY_PHASE_C            = 0x0034, // Wh  , 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_ENERGY                    = 0x0036, // Wh  , 32-bit   signed, scale 1, same as 0x0000
        // WATTSON_INT_INDUCTIVE_ENERGY_PHASE_A            = 0x0038, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_INDUCTIVE_ENERGY_PHASE_B            = 0x003A, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_INDUCTIVE_ENERGY_PHASE_C            = 0x003C, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_TOTAL_INDUCTIVE_ENERGY              = 0x003E, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_CAPACITIVE_ENERGY_PHASE_A           = 0x0040, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_CAPACITIVE_ENERGY_PHASE_B           = 0x0042, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_CAPACITIVE_ENERGY_PHASE_C           = 0x0044, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_TOTAL_CAPACITIVE_ENERGY             = 0x0046, // VARh, 32-bit unsigned, scale 1
        // WATTSON_INT_NET_TOTAL_VARH_PHASE_A              = 0x0048, // VARh, 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_VARH_PHASE_B              = 0x004A, // VARh, 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_VARH_PHASE_C              = 0x004C, // VARh, 32-bit   signed, scale 1
        // WATTSON_INT_NET_TOTAL_VARH                      = 0x004E, // VARh, 32-bit   signed, scale 1
        // WATTSON_INT_APPARENT_ENERGY_PHASE_A             = 0x0050, // VAh , 32-bit unsigned, scale 1
        // WATTSON_INT_APPARENT_ENERGY_PHASE_B             = 0x0052, // VAh , 32-bit unsigned, scale 1
        // WATTSON_INT_APPARENT_ENERGY_PHASE_C             = 0x0054, // VAh , 32-bit unsigned, scale 1
        // WATTSON_INT_TOTAL_APPARENT_ENERGY               = 0x0056, // VAh , 32-bit unsigned, scale 1
        // WATTSON_INT_SLIDING_WINDOW_REAL_POWER_DEMAND    = 0x0058, // WD  , 16-bit   signed, scale 1

        WATTSON_INT_PT_RATIO_PRIMARY                    = 0x0080, // 16-bit unsigned
        WATTSON_INT_PT_RATIO_SECONDARY                  = 0x0081, // 16-bit unsigned
        WATTSON_INT_CT_RATIO_PRIMARY                    = 0x0082, // 16-bit unsigned
        WATTSON_INT_CT_RATIO_SECONDARY                  = 0x0083, // 16-bit unsigned
        WATTSON_INT_DEMAND_PERIOD                       = 0x0084, // 16-bit unsigned
        WATTSON_INT_DEBUG                               = 0x0085, // 16-bit unsigned
        WATTSON_INT_PULSE_VALUE                         = 0x0086, // 16-bit unsigned
        WATTSON_INT_RESET                               = 0x0087, // 16-bit unsigned
        WATTSON_INT_OUTPUT_A_SOURCE                     = 0x0088, // 16-bit unsigned
        WATTSON_INT_OUTPUT_B_SOURCE                     = 0x0089, // 16-bit unsigned
        WATTSON_INT_OUTPUT_A_0V_VALUE                   = 0x008A, // 16-bit signed
        WATTSON_INT_OUTPUT_B_0V_VALUE                   = 0x008B, // 16-bit signed
        WATTSON_INT_OUTPUT_A_10V_VALUE                  = 0x008C, // 16-bit signed
        WATTSON_INT_OUTPUT_B_10V_VALUE                  = 0x008D, // 16-bit signed
        WATTSON_INT_CONFIGURATION_WORD                  = 0x008E, // 16-bit unsigned

        WATTSON_INT_SERIAL_NUMBER                       = 0x0095, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD1                         = 0x0096, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD2                         = 0x0097, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD3                         = 0x0098, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD4                         = 0x0099, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD5                         = 0x009A, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD6                         = 0x009B, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD7                         = 0x009C, // 16-bit unsigned
        WATTSON_INT_SCRATCHPAD8                         = 0x009D, // 16-bit unsigned

        WATTSON_INT_EXTENDED_CONFIGURATION_WORD         = 0x009E, // 16-bit unsigned

        // Float
        //WATTSON_FLOAT_TOTAL_ENERGY_CONSUMPTION          = 0x0300, // kWh  , 32-bit

        WATTSON_FLOAT_TOTAL_REAL_POWER                  = 0x0302, // kW   , 32-bit
        WATTSON_FLOAT_TOTAL_REACTIVE_POWER              = 0x0304, // kvar , 32-bit
        WATTSON_FLOAT_TOTAL_APPARENT_POWER              = 0x0306, // kVA  , 32-bit
        WATTSON_FLOAT_AVERAGE_VOLTAGE_L_N               = 0x0308, // V    , 32-bit
        WATTSON_FLOAT_AVERAGE_VOLTAGE_L_L               = 0x030A, // V    , 32-bit
        WATTSON_FLOAT_AVERAGE_CURRENT                   = 0x030C, // A    , 32-bit
        WATTSON_FLOAT_TOTAL_SYSTEM_POWER_FACTOR         = 0x030E, //      , 32-bit
        WATTSON_FLOAT_FREQUENCY                         = 0x0310, // Hz   , 32-bit
        WATTSON_FLOAT_SLIDING_WINDOW_REAL_POWER_DEMAND  = 0x0312, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_A_N                 = 0x0314, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_B_N                 = 0x0316, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_C_N                 = 0x0318, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_A_B                 = 0x031A, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_B_C                 = 0x031C, // V    , 32-bit
        WATTSON_FLOAT_VOLTAGE_PHASE_A_C                 = 0x031E, // V    , 32-bit
        WATTSON_FLOAT_CURRENT_PHASE_A                   = 0x0320, // A    , 32-bit
        WATTSON_FLOAT_CURRENT_PHASE_B                   = 0x0322, // A    , 32-bit
        WATTSON_FLOAT_CURRENT_PHASE_C                   = 0x0324, // A    , 32-bit
        WATTSON_FLOAT_REAL_POWER_PHASE_A                = 0x0326, // kW   , 32-bit
        WATTSON_FLOAT_REAL_POWER_PHASE_B                = 0x0328, // kW   , 32-bit
        WATTSON_FLOAT_REAL_POWER_PHASE_C                = 0x032A, // kW   , 32-bit
        WATTSON_FLOAT_REACTIVE_POWER_PHASE_A            = 0x032C, // kvar , 32-bit
        WATTSON_FLOAT_REACTIVE_POWER_PHASE_B            = 0x032E, // kvar , 32-bit
        WATTSON_FLOAT_REACTIVE_POWWR_PHASE_C            = 0x0330, // kvar , 32-bit
        WATTSON_FLOAT_APPARENT_POWER_PHASE_A            = 0x0332, // kVA  , 32-bit
        WATTSON_FLOAT_APPARENT_POWER_PHASE_B            = 0x0334, // kVA  , 32-bit
        WATTSON_FLOAT_APPARENT_POWER_PHASE_C            = 0x0336, // kVA  , 32-bit
        WATTSON_FLOAT_POWER_FACTOR_PHASE_A              = 0x0338, //      , 32-bit
        WATTSON_FLOAT_POWER_FACTOR_PHASE_B              = 0x033A, //      , 32-bit
        WATTSON_FLOAT_POWER_FACTOR_PHASE_C              = 0x033C, //      , 32-bit

        WATTSON_FLOAT_SOFTWARE_VERSION                  = 0x033E, //      , 32-bit

        WATTSON_FLOAT_IMPORT_ENERGY_PHASE_A             = 0x0340, // kWh  , 32-bit
        WATTSON_FLOAT_IMPORT_ENERGY_PHASE_B             = 0x0342, // kWh  , 32-bit
        WATTSON_FLOAT_IMPORT_ENERGY_PHASE_C             = 0x0344, // kWh  , 32-bit
        WATTSON_FLOAT_TOTAL_IMPORT_ENERGY               = 0x0346, // kWh  , 32-bit
        WATTSON_FLOAT_EXPORT_ENERGY_PHASE_A             = 0x0348, // kWh  , 32-bit
        WATTSON_FLOAT_EXPORT_ENERGY_PHASE_B             = 0x034A, // kWh  , 32-bit
        WATTSON_FLOAT_EXPORT_ENERGY_PHASE_C             = 0x034C, // kWh  , 32-bit
        WATTSON_FLOAT_TOTAL_EXPORT_ENERGY               = 0x034E, // kWh  , 32-bit
        WATTSON_FLOAT_NET_TOTAL_ENERGY_PHASE_A          = 0x0350, // kWh  , 32-bit
        WATTSON_FLOAT_NET_TOTAL_ENERGY_PHASE_B          = 0x0352, // kWh  , 32-bit
        WATTSON_FLOAT_NET_TOTAL_ENERGY_PHASE_C          = 0x0354, // kWh  , 32-bit
        WATTSON_FLOAT_NET_TOTAL_ENERGY                  = 0x0356, // kWh  , 32-bit, same as 0x0300
        WATTSON_FLOAT_INDUCTIVE_ENERGY_PHASE_A          = 0x0358, // kvarh, 32-bit
        WATTSON_FLOAT_INDUCTIVE_ENERGY_PHASE_B          = 0x035A, // kvarh, 32-bit
        WATTSON_FLOAT_INDUCTIVE_ENERGY_PHASE_C          = 0x035C, // kvarh, 32-bit
        WATTSON_FLOAT_TOTAL_INDUCTIVE_ENERGY            = 0x035E, // kvarh, 32-bit
        WATTSON_FLOAT_CAPACITIVE_ENERGY_PHASE_A         = 0x0360, // kvarh, 32-bit
        WATTSON_FLOAT_CAPACITIVE_ENERGY_PHASE_B         = 0x0362, // kvarh, 32-bit
        WATTSON_FLOAT_CAPACITIVE_ENERGY_PHASE_C         = 0x0364, // kvarh, 32-bit
        WATTSON_FLOAT_TOTAL_CAPACITIVE_ENERGY           = 0x0366, // kvarh, 32-bit
        WATTSON_FLOAT_NET_TOTAL_VARH_PHASE_A            = 0x0368, // kvarh, 32-bit
        WATTSON_FLOAT_NET_TOTAL_VARH_PHASE_B            = 0x036A, // kvarh, 32-bit
        WATTSON_FLOAT_NET_TOTAL_VARH_PHASE_C            = 0x036C, // kvarh, 32-bit
        WATTSON_FLOAT_NET_TOTAL_VARH                    = 0x036E, // kvarh, 32-bit
        WATTSON_FLOAT_TOTAL_APPARENT_ENERGY_PHASE_A     = 0x0370, // kVAh , 32-bit
        WATTSON_FLOAT_TOTAL_APPARENT_ENERGY_PHASE_B     = 0x0372, // kVAh , 32-bit
        WATTSON_FLOAT_TOTAL_APPARENT_ENERGY_PHASE_C     = 0x0374, // kVAh , 32-bit
        WATTSON_FLOAT_TOTAL_APPARENT_ENERGY             = 0x0376, // kVAh , 32-bit
    };


    struct alignas(8) WattsOnFloatRaw {
        float       total_real_power;
        float       total_reactive_power;
        float       total_apparent_power;
        float       average_voltage_l_n;
        float       average_voltage_l_l;
        float       average_current;
        float       total_system_power_factor;
        float       frequency;
        float       sliding_window_real_power_demand;
        float       voltage_phase_a_n;
        float       voltage_phase_b_n;
        float       voltage_phase_c_n;
        float       voltage_phase_a_b;
        float       voltage_phase_b_c;
        float       voltage_phase_a_c;
        float       current_phase_a;
        float       current_phase_b;
        float       current_phase_c;
        float       real_power_phase_a;
        float       real_power_phase_b;
        float       real_power_phase_c;
        float       reactive_power_phase_a;
        float       reactive_power_phase_b;
        float       reactive_power_phase_c;
        float       apparent_power_phase_a;
        float       apparent_power_phase_b;
        float       apparent_power_phase_c;
        float       power_factor_phase_a;
        float       power_factor_phase_b;
        float       power_factor_phase_c;

        float       software_version;

        float       import_energy_phase_a;
        float       import_energy_phase_b;
        float       import_energy_phase_c;
        float       total_import_energy;
        float       export_energy_phase_a;
        float       export_energy_phase_b;
        float       export_energy_phase_c;
        float       total_export_energy;
        float       net_energy_phase_a;
        float       net_energy_phase_b;
        float       net_energy_phase_c;
        float       net_total_energy;
        float       inductive_energy_phase_a;
        float       inductive_energy_phase_b;
        float       inductive_energy_phase_c;
        float       total_inductive_energy;
        float       capacitive_energy_phase_a;
        float       capacitive_energy_phase_b;
        float       capacitive_energy_phase_c;
        float       total_capacitive_energy;
        float       net_reactive_energy_phase_a;
        float       net_reactive_energy_phase_b;
        float       net_reactive_energy_phase_c;
        float       net_total_reactive_energy;
        float       apparent_energy_phase_a;
        float       apparent_energy_phase_b;
        float       apparent_energy_phase_c;
        float       total_apparent_energy;
    } __attribute__((packed));

    struct alignas(8) WattsOnConfigurationRaw {
        uint16_t    pt_ratio_primary;
        uint16_t    pt_ratio_secondary;
        uint16_t    ct_ratio_primary;
        uint16_t    ct_ratio_secondary;
        uint16_t    demand_period;
        uint16_t    debug;
        uint16_t    pulse_value;
        uint16_t    reset_register;
        uint16_t    output_a_source;
        uint16_t    output_b_source;
        int16_t     output_a_0v_value;
        int16_t     output_b_0v_value;
        int16_t     output_a_10v_value;
        int16_t     output_b_10v_value;
        union {
            uint16_t    word;
            struct {
                bool    output_2_selection:1;
                bool    reverse_watt_sign:1;
                bool    reverse_var_sign:1;
                bool    reverse_sequence:1;
                bool    disable_sequence:1;
                bool    force_watt_absolute:1;
                bool    force_var_absolute:1;
                bool    force_pf_absolute:1;
                bool    pulse_on_wh_increment:1;
                bool    pulse_on_wh_decrement:1;
                bool    pulse_on_varh_increment:1;
                bool    pulse_on_varh_decrement:1;
                bool    no_voltage_led_flash:1;
                bool    word_order:1;
                bool    average_voltage_current:1;
                bool    split_240v_load:1;
            } bit;
        } configuration_word;        
    } __attribute__((packed));

    struct alignas(8) WattsOnScratchPadRaw {
        uint16_t    serial_number;
        uint16_t    scratch_pad_1;
        uint16_t    scratch_pad_2;
        uint16_t    scratch_pad_3;
        uint16_t    scratch_pad_4;
        uint16_t    scratch_pad_5;
        uint16_t    scratch_pad_6;
        uint16_t    scratch_pad_7;
        uint16_t    scratch_pad_8;
    } __attribute__((packed));

    struct alignas(8) WattsOnExtendedConfigurationRaw {
        union {
            uint16_t    word;
            struct {
                bool        output_2_display_stream:1;
                bool        pulse_output_type:1;
                bool        baud_rate_selection:1;
                uint16_t    not_used:13;
            } bit;
        } extended_configuration_word;
    } __attribute__((packed));

public:

    class WattsOnFloat : private WattsOnFloatRaw {
        friend class WattsOnModbusInterface;
    public:
        float TotalRealPower() const { return total_real_power * 1000.0f; }
        float TotalReactivePower() const { return total_reactive_power * 1000.0f; }
        float TotalApparentPower() const { return total_apparent_power * 1000.0f; }
        float AverageVoltageLineNeutral() const { return average_voltage_l_n; }
        float AverageVoltageLineLine() const { return average_voltage_l_l; }
        float AverageCurrent() const { return average_current; }
        float TotalSystemPowerFactor() const { return total_system_power_factor; }
        float Frequency() const { return frequency; }
        float VoltagePhaseA2N() const { return voltage_phase_a_n; }
        float VoltagePhaseB2N() const { return voltage_phase_b_n; }
        float VoltagePhaseC2N() const { return voltage_phase_c_n; }
        float VoltagePhaseA2B() const { return voltage_phase_a_b; }
        float VoltagePhaseB2C() const { return voltage_phase_b_c; }
        float VoltagePhaseA2C() const { return voltage_phase_a_c; }
        float CurrentPhaseA() const { return current_phase_a; }
        float CurrentPhaseB() const { return current_phase_b; }
        float CurrentPhaseC() const { return current_phase_c; }
        float RealPowerPhaseA() const { return real_power_phase_a * 1000.0f; }
        float RealPowerPhaseB() const { return real_power_phase_b * 1000.0f; }
        float RealPowerPhaseC() const { return real_power_phase_c * 1000.0f; }
        float ReactivePowerPhaseA() const { return reactive_power_phase_a * 1000.0f; }
        float ReactivePowerPhaseB() const { return reactive_power_phase_b * 1000.0f; }
        float ReactivePowerPhaseC() const { return reactive_power_phase_c * 1000.0f; }
        float ApparentPowerPhaseA() const { return apparent_power_phase_a * 1000.0f; }
        float ApparentPowerPhaseB() const { return apparent_power_phase_b * 1000.0f; }
        float ApparentPowerPhaseC() const { return apparent_power_phase_c * 1000.0f; }
        float PowerFactorPhaseA() const { return power_factor_phase_a; }
        float PowerFactorPhaseB() const { return power_factor_phase_b; }
        float PowerFactorPhaseC() const { return power_factor_phase_c; }
        float SoftwareVersion() const { return software_version; }
        float ImportEnergyPhaseA() const { return import_energy_phase_a * 1000.0f; }
        float ImportEnergyPhaseB() const { return import_energy_phase_b * 1000.0f; }
        float ImportEnergyPhaseC() const { return import_energy_phase_c * 1000.0f; }
        float TotalImportEnergy() const { return total_import_energy * 1000.0f; }
        float ExportEnergyPhaseA() const { return export_energy_phase_a * 1000.0f; }
        float ExportEnergyPhaseB() const { return export_energy_phase_b * 1000.0f; }
        float ExportEnergyPhaseC() const { return export_energy_phase_c * 1000.0f; }
        float TotalExportEnergy() const { return total_export_energy * 1000.0f; }
        float NetEnergyPhaseA() const { return net_energy_phase_a * 1000.0f; }
        float NetEnergyPhaseB() const { return net_energy_phase_b * 1000.0f; }
        float NetEnergyPhaseC() const { return net_energy_phase_c * 1000.0f; }
        float NetTotalEnergy() const { return net_total_energy * 1000.0f; }
        float InductiveEnergyPhaseA() const { return inductive_energy_phase_a * 1000.0f; }
        float InductiveEnergyPhaseB() const { return inductive_energy_phase_b * 1000.0f; }
        float InductiveEnergyPhaseC() const { return inductive_energy_phase_c * 1000.0f; }
        float TotalInductiveEnergy() const { return total_inductive_energy * 1000.0f; }
        float CapacitiveEnergyPhaseA() const { return capacitive_energy_phase_a * 1000.0f; }
        float CapacitiveEnergyPhaseB() const { return capacitive_energy_phase_b * 1000.0f; }
        float CapacitiveEnergyPhaseC() const { return capacitive_energy_phase_c * 1000.0f; }
        float TotalCapacitiveEnergy() const { return total_capacitive_energy * 1000.0f; }
        float NetReactiveEnergyPhaseA() const { return net_reactive_energy_phase_a * 1000.0f; }
        float NetReactiveEnergyPhaseB() const { return net_reactive_energy_phase_b * 1000.0f; }
        float NetReactiveEnergyPhaseC() const { return net_reactive_energy_phase_c * 1000.0f; }
        float NetTotalReactiveEnergy() const { return net_total_reactive_energy * 1000.0f; }
        float ApparentEnergyPhaseA() const { return apparent_energy_phase_a * 1000.0f; }
        float ApparentEnergyPhaseB() const { return apparent_energy_phase_b * 1000.0f; }
        float ApparentEnergyPhaseC() const { return apparent_energy_phase_c * 1000.0f; }
        float TotalApparentEnergy() const { return total_apparent_energy * 1000.0f; }
        float SlidingWindowRealPowerDemand() const { return sliding_window_real_power_demand * 1000.0; }
    } wattson_float;

    class WattsOnConfiguration : private WattsOnConfigurationRaw {
        friend class WattsOnModbusInterface;
    public:
        uint16_t PTRatioPrimary() const { return pt_ratio_primary; }
        uint16_t PTRatioSecondary() const { return pt_ratio_secondary; }
        uint16_t CTRatioPrimary() const { return ct_ratio_primary; }
        uint16_t CTRatioSecondary() const { return ct_ratio_secondary; }
        uint16_t DemandPeriod() const { return demand_period; }
        uint16_t Debug() const { return debug; }
        uint16_t PulseValue() const { return pulse_value; }
        uint16_t OutputASource() const { return output_a_source; }
        uint16_t OutputBSource() const { return output_b_source; }
        int16_t  OutputA0VValue() const { return output_a_0v_value; }
        int16_t  OutputB0VValue() const { return output_b_0v_value; }
        int16_t  OutputA10VValue() const { return output_a_10v_value; }
        int16_t  OutputB10VValue() const { return output_b_10v_value; }
        uint16_t ConfigurationWord() const { return configuration_word.word; }
        bool     OutputTwoSelection() const { return configuration_word.bit.output_2_selection; }
        bool     ReverseWattSign() const { return configuration_word.bit.reverse_watt_sign; }
        bool     ReverseVARSign() const { return configuration_word.bit.reverse_var_sign; }
        bool     ReverseSequence() const { return configuration_word.bit.reverse_sequence; }
        bool     DisableSequence() const { return configuration_word.bit.disable_sequence; }
        bool     ForceWattAbsolute() const { return configuration_word.bit.force_watt_absolute; }
        bool     ForceVARAbsolute() const { return configuration_word.bit.force_var_absolute; }
        bool     ForcePFAbsolute() const { return configuration_word.bit.force_pf_absolute; }
        bool     PulseOnWhIncrement() const { return configuration_word.bit.pulse_on_wh_increment; }
        bool     PulseOnWhDecrement() const { return configuration_word.bit.pulse_on_wh_decrement; }
        bool     PulseOnVARhIncrement() const { return configuration_word.bit.pulse_on_varh_increment; }
        bool     PulseOnVARhDecrement() const { return configuration_word.bit.pulse_on_varh_decrement; }
        bool     NoVoltageLedFlash() const { return configuration_word.bit.no_voltage_led_flash; }
        bool     WordOrder32bit() const { return configuration_word.bit.word_order; }
        bool     AverageVoltageCurrent() const { return configuration_word.bit.average_voltage_current; }
        bool     Split240VLoad() const { return configuration_word.bit.split_240v_load; }

        int PTRatioPrimary(uint16_t value);
        int PTRatioSecondary(uint16_t value);
        int CTRatioPrimary(uint16_t value);
        int CTRatioSecondary(uint16_t value);
        int DemandPeriod(uint16_t value);
        int PulseValue(uint16_t value);
        int OutputASource(uint16_t value);
        int OutputBSource(uint16_t value);
        int OutputA0VValue(int16_t value);
        int OutputB0VValue(int16_t value);
        int OutputA10VValue(int16_t value);
        int OutputB10VValue(int16_t value);
        int ConfigurationWord(uint16_t value);
        int OutputTwoSelection(bool value);
        int ReverseWattSign(bool value);
        int ReverseVARSign(bool value);
        int ReverseSequence(bool value);
        int DisableSequence(bool value);
        int ForceWattAbsolute(bool value);
        int ForceVARAbsolute(bool value);
        int ForcePFAbsolute(bool value);
        int PulseOnWhIncrement(bool value);
        int PulseOnWhDecrement(bool value);
        int PulseOnVARhIncrement(bool value);
        int PulseOnVARhDecrement(bool value);
        int NoVoltageLedFlash(bool value);
        int WordOrder32bit(bool value);
        int AverageVoltageCurrent(bool value);
        int Split240VLoad(bool value);
    private:
        WattsOnModbusInterface * parent_;
        WattsOnConfiguration(WattsOnModbusInterface * parent) : parent_(parent) {}
    } wattson_configuration;

    class WattsOnScratchPad : private WattsOnScratchPadRaw {
        friend class WattsOnModbusInterface;
    public:
        uint16_t SerialNumber() const { return serial_number; }
        uint16_t Pad1() const { return scratch_pad_1; }
        uint16_t Pad2() const { return scratch_pad_2; }
        uint16_t Pad3() const { return scratch_pad_3; }
        uint16_t Pad4() const { return scratch_pad_4; }
        uint16_t Pad5() const { return scratch_pad_5; }
        uint16_t Pad6() const { return scratch_pad_6; }
        uint16_t Pad7() const { return scratch_pad_7; }
        uint16_t Pad8() const { return scratch_pad_8; }

        int Pad1(uint16_t value);
        int Pad2(uint16_t value);
        int Pad3(uint16_t value);
        int Pad4(uint16_t value);
        int Pad5(uint16_t value);
        int Pad6(uint16_t value);
        int Pad7(uint16_t value);
        int Pad8(uint16_t value);
    private:        
        WattsOnModbusInterface * parent_;
        WattsOnScratchPad(WattsOnModbusInterface * parent) : parent_(parent) {}
    } wattson_scratchpad;

    class WattsOnExtendedConfiguration : private WattsOnExtendedConfigurationRaw {
        friend class WattsOnModbusInterface;
    public:
        uint16_t ExtendedConfigurationWord() const { return extended_configuration_word.word; }
        bool OutputTwoDisplayStream() const { return extended_configuration_word.bit.output_2_display_stream; }
        bool PulseOutputType() const { return extended_configuration_word.bit.pulse_output_type; }
        bool BaudRateSelection() const { return extended_configuration_word.bit.baud_rate_selection; }

        int ExtendedConfigurationWord(uint16_t value);
        int OutputTwoDisplayStream(bool value);
        int PulseOutputType(bool value);
        int BaudRateSelection(bool value);
     private:
        WattsOnModbusInterface * parent_;
        WattsOnExtendedConfiguration(WattsOnModbusInterface * parent) : parent_(parent) {}
    } wattson_extended_configuration;

    WattsOnModbusInterface();
    ~WattsOnModbusInterface() noexcept;

    int  Connect(const char linux_device_driver[], const int baudrate, const int slave_number);
    int  CheckConnection();
    void Disconnect();

    void EnableDebug();
    void DisableDebug();
    char const * Error(int error_number) const;

    bool ReadWattsOnFloatInstant();  // from WattsOn Version 2.2 on
    bool ReadWattsOnFloatEnergy();   // from WattsOn Version 2.2 on

    int ReadWattsOnConfiguration();
    int ReadWattsOnScratchPad();    // from WattsOn Version 2.0 on
    int ReadWattsOnExtendedConfiguration(); // from WattsOn Version 4.5 on

    int Reset(); //* Reset meter counters

	const std::string & Port() const;
    int         		SlaveNumber() const;
    int         		BaudRate() const;

    float       PTRatio() const;
    float       CTRatio() const;

private:

    modbus_t *					mb_ctx_ = nullptr;
    WattsOnModbusInterface * 	next_ = nullptr;
    float   					pt_ratio_ = 1.0;
    float   					ct_ratio_ = 1.0;
    float                       software_version_ = 0.0;
    int     					slave_number_ = -1;
    int     					baudrate_ = 0;
    bool    					swap_msb_lsb_ = false;
	std::string					port_;
};


/**
 * @brief ElkorModbusInterface
 */
class ElkorModbusInterface {
public:

    ElkorModbusInterface();
    ~ElkorModbusInterface();

    unsigned int ScanRTUDevices(const char linux_device_driver[], unsigned int begin_slave_address = 0, unsigned int end_slave_address = 64);	
    WattsOnModbusInterface * GetFirstDevice() const;
    WattsOnModbusInterface * GetNextDevice(WattsOnModbusInterface *) const;
	void ClearDeviceList();

private:

    WattsOnModbusInterface * devices_ = nullptr;

};

std::ostream & operator<<(std::ostream & out, const ElkorModbusInterface &data);
std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnFloat &data);
std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnConfiguration &data);
std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnScratchPad &data);
std::ostream & operator<<(std::ostream & out, const WattsOnModbusInterface::WattsOnExtendedConfiguration &data);

#include "elkor_modbus.inl"

#endif /* ELKOR_MODBUS_HPP */
