#include <csignal>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <new>
#include <thread>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <memory_resource>

#include <getopt.h>

#include <modbus/modbus.h>
#include <util/logger.hpp>
#include <fabrix/fabrix.hpp>
#include <ime_check.hpp>
#include <phase_lock_loop/phase_lock_loop.hpp>
#include <schema/ime_generated.h>
#include <schema/master_clock_generated.h>

namespace  {

bool stop = false;
int exit_code = EXIT_SUCCESS;

std::pmr::unsynchronized_pool_resource memory_pool;
std::pmr::string component_name(&memory_pool);
std::pmr::string device_name("/dev/ime", &memory_pool);
std::pmr::string master_clock_name(&memory_pool);
double read_frequency_hz = 1.0;
int slave_address = 1;
bool verbose = false;

void print_help() {
    std::cout << "Usage:\n"
                 "  ime -n|--name <component_name> [-d|--device <device>] [-s|--slave <address>]\n"
                 "Options:\n"
                 "  -n, --name <component_name>\n"
                 "      Specify the component name [mandatory]\n"
                 "  -d, --device <device>\n"
                 "      Specify the device name [default: /dev/ime]\n"
                 "  -s, --slave <address>\n"
                 "      Set the Modbus slave address [default: 1]\n";
                 "  -c, --clock <component_name>\n"
                 "      Specify the master clock component [optional]\n"
                 "  -f, --frequency <Hz>\n"
                 "      Set the frequency [default: 1]\n"
                 "  -v, --verbose\n"
                 "      Enable verbose output\n"
                 "  -h, --help\n"
                 "      Display this help and exit\n";
    std::cout.flush();
}

void parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"name", required_argument, nullptr, 'n'},
        {"device", optional_argument, nullptr, 'd'},
        {"slave", optional_argument, nullptr, 's'},
        {"clock", required_argument, nullptr, 'c'},
        {"frequency", required_argument, nullptr, 'f'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "n:d:s:c:f:vh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'n':
                component_name = optarg;
                break;
            case 'd':
                device_name = optarg;
                break;
            case 's':
                slave_address = std::atoi(optarg);
                if (slave_address < 0 or slave_address > 255) {
                    std::cerr << "Error: Slave number must be between 0 and 255.\n";
                    exit(EXIT_FAILURE);
                }
                break;
            case 'c':
                master_clock_name = optarg;
                break;
            case 'f':
                read_frequency_hz = std::atof(optarg);
                if (read_frequency_hz <= 0.0) {
                    std::cerr << "Error: Frequency must be a positive number.\n";
                    exit(EXIT_FAILURE);
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
            default:
                std::cerr << "Error: Invalid option. Use -h for help.\n";
                exit(1);
        }
    }

    if (component_name.empty()) {
        std::cerr << "Error: Component name is mandatory. Use -h for help.\n";
        exit(EXIT_FAILURE);
    }
}

constexpr std::uint32_t swap_lr(std::uint32_t v) {
   return (v << 16) | (v >> 16);
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::IME::Sector const & o) {
    switch (o) {
        case CEMS::IME::Sector::RESISTIVE_OR_REACTIVE: out << "RESISTIVE_OR_REACTIVE"; break;
        case CEMS::IME::Sector::INDUCTIVE: out << "INDUCTIVE"; break;
        case CEMS::IME::Sector::CAPACITIVE: out << "CAPACITIVE"; break;
    }
    return out;
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::IME::InstantReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"                                    ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "slave_number"                                 ANSI_TOK ":" ANSI_NRM << o.slave_number()
        << ANSI_TOK "," ANSI_LBL "device_identifier"                            ANSI_TOK ":" ANSI_NRM << o.device_identifier()
        << ANSI_TOK "," ANSI_LBL "frequency"                                    ANSI_TOK ":" ANSI_NRM << o.frequency()
        << ANSI_TOK "," ANSI_LBL "l1_l2_voltage"                                ANSI_TOK ":" ANSI_NRM << o.l1_l2_voltage()
        << ANSI_TOK "," ANSI_LBL "l2_l3_voltage"                                ANSI_TOK ":" ANSI_NRM << o.l2_l3_voltage()
        << ANSI_TOK "," ANSI_LBL "l3_l1_voltage"                                ANSI_TOK ":" ANSI_NRM << o.l3_l1_voltage()
        << ANSI_TOK "," ANSI_LBL "tri_phase_active_power"                       ANSI_TOK ":" ANSI_NRM << o.tri_phase_active_power()
        << ANSI_TOK "," ANSI_LBL "tri_phase_reactive_power"                     ANSI_TOK ":" ANSI_NRM << o.tri_phase_reactive_power()
        << ANSI_TOK "," ANSI_LBL "tri_phase_apparent_power"                     ANSI_TOK ":" ANSI_NRM << o.tri_phase_apparent_power()
        << ANSI_TOK "," ANSI_LBL "tri_phase_power_factor"                       ANSI_TOK ":" ANSI_NRM << o.tri_phase_power_factor()
        << ANSI_TOK "," ANSI_LBL "tri_phase_sector_of_power_factor"             ANSI_TOK ":" ANSI_NRM << o.tri_phase_sector_of_power_factor()
        << ANSI_TOK "," ANSI_LBL "tri_phase_peak_maximum_demand"                ANSI_TOK ":" ANSI_NRM << o.tri_phase_peak_maximum_demand()
        << ANSI_TOK "," ANSI_LBL "tri_phase_second_tariff_peak_maximum_demand"  ANSI_TOK ":" ANSI_NRM << o.tri_phase_second_tariff_peak_maximum_demand()
        << ANSI_TOK "," ANSI_LBL "time_counter_for_average_power"               ANSI_TOK ":" ANSI_NRM << o.time_counter_for_average_power()
        << ANSI_TOK "," ANSI_LBL "tri_phase_average_power"                      ANSI_TOK ":" ANSI_NRM << o.tri_phase_average_power()
        << ANSI_TOK "," ANSI_LBL "average_voltage_line_neutral"                 ANSI_TOK ":" ANSI_NRM << o.average_voltage_line_neutral()
        << ANSI_TOK "," ANSI_LBL "average_voltage_line_line"                    ANSI_TOK ":" ANSI_NRM << o.average_voltage_line_line()
        << ANSI_TOK "," ANSI_LBL "average_current"                              ANSI_TOK ":" ANSI_NRM << o.average_current()
        << ANSI_TOK "," ANSI_LBL "phase1_active_power"                          ANSI_TOK ":" ANSI_NRM << o.phase1_active_power()
        << ANSI_TOK "," ANSI_LBL "phase1_current"                               ANSI_TOK ":" ANSI_NRM << o.phase1_current()
        << ANSI_TOK "," ANSI_LBL "phase1_voltage"                               ANSI_TOK ":" ANSI_NRM << o.phase1_voltage()
        << ANSI_TOK "," ANSI_LBL "phase1_reactive_power"                        ANSI_TOK ":" ANSI_NRM << o.phase1_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase2_active_power"                          ANSI_TOK ":" ANSI_NRM << o.phase2_active_power()
        << ANSI_TOK "," ANSI_LBL "phase2_current"                               ANSI_TOK ":" ANSI_NRM << o.phase2_current()
        << ANSI_TOK "," ANSI_LBL "phase2_voltage"                               ANSI_TOK ":" ANSI_NRM << o.phase2_voltage()
        << ANSI_TOK "," ANSI_LBL "phase2_reactive_power"                        ANSI_TOK ":" ANSI_NRM << o.phase2_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase3_active_power"                          ANSI_TOK ":" ANSI_NRM << o.phase3_active_power()
        << ANSI_TOK "," ANSI_LBL "phase3_current"                               ANSI_TOK ":" ANSI_NRM << o.phase3_current()
        << ANSI_TOK "," ANSI_LBL "phase3_voltage"                               ANSI_TOK ":" ANSI_NRM << o.phase3_voltage()
        << ANSI_TOK "," ANSI_LBL "phase3_reactive_power"                        ANSI_TOK ":" ANSI_NRM << o.phase3_reactive_power()
        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::IME::EnergyReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"                                                ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "slave_number"                                             ANSI_TOK ":" ANSI_NRM << o.slave_number()
        << ANSI_TOK "," ANSI_LBL "device_identifier"                                        ANSI_TOK ":" ANSI_NRM << o.device_identifier()
        << ANSI_TOK "," ANSI_LBL "tri_phase_total_positive_active_energy"                   ANSI_TOK ":" ANSI_NRM << o.tri_phase_total_positive_active_energy()
        << ANSI_TOK "," ANSI_LBL "tri_phase_total_positive_reactive_energy"                 ANSI_TOK ":" ANSI_NRM << o.tri_phase_total_positive_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "tri_phase_partial_positive_active_energy"                 ANSI_TOK ":" ANSI_NRM << o.tri_phase_partial_positive_active_energy()
        << ANSI_TOK "," ANSI_LBL "tri_phase_partial_positive_reactive_energy"               ANSI_TOK ":" ANSI_NRM << o.tri_phase_partial_positive_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "tri_phase_partial_second_tariff_positive_active_energy"   ANSI_TOK ":" ANSI_NRM << o.tri_phase_partial_second_tariff_positive_active_energy()
        << ANSI_TOK "," ANSI_LBL "tri_phase_partial_second_tariff_positive_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.tri_phase_partial_second_tariff_positive_reactive_energy()
        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

class ime final : public fabrix::component {
private:

    char const * const TOPIC_NAME_INSTANT_READING = "InstantReading";
    char const * const TOPIC_NAME_ENERGY_READING = "EnergyReading";
    char const * const TOPIC_NAME_CLOCK_TICK = "ClockTick";

    /*
     * 0x0C8 parameter reset
     *       0x03 reset partial active energy
     *       0x02 reset partial reactive energy
     *       0x10 reset peak maximum demand tariff 1 (when selected)
     *       0x20 reset peak maximum demand tariff 2 (when selected)
    */

    /**
     * @brief
     */
    enum ImeAddress {
        DEVICE_IDENTIFIER 							                = 0x0300, /*      , 16-bit unsigned, scale 1    */

        TRI_PHASE_TOTAL_POSITIVE_ACTIVE_ENERGY 		                = 0x0325, /* kWh  , 32-bit unsigned, scale 100  */
        TRI_PHASE_TOTAL_POSITIVE_REACTIVE_ENERGY 	                = 0x0329, /* kvarh, 32-bit unsigned, scale 100  */
        TRI_PHASE_PARTIAL_POSITIVE_ACTIVE_ENERGY 	                = 0x032D, /* kWh  , 32-bit unsigned, scale 100  */
        TRI_PHASE_PARTIAL_POSITIVE_REACTIVE_ENERGY 	                = 0x0331, /* kvarh, 32-bit unsigned, scale 100  */

        PHASE1_VOLTAGE 								                = 0x1000, /* V    , 32-bit unsigned, scale 1000 */
        PHASE2_VOLTAGE 								                = 0x1002, /* V    , 32-bit unsigned, scale 1000 */
        PHASE3_VOLTAGE 								                = 0x1004, /* V    , 32-bit unsigned, scale 1000 */
        PHASE1_CURRENT 								                = 0x1006, /* A    , 32-bit unsigned, scale 1000 */
        PHASE2_CURRENT 								                = 0x1008, /* A    , 32-bit unsigned, scale 1000 */
        PHASE3_CURRENT 								                = 0x100A, /* A    , 32-bit unsigned, scale 1000 */

        L1_L2_VOLTAGE 								                = 0x100E, /* V    , 32-bit unsigned, scale 1000 */
        L2_L3_VOLTAGE 								                = 0x1010, /* V    , 32-bit unsigned, scale 1000 */
        L3_L1_VOLTAGE 								                = 0x1012, /* V    , 32-bit unsigned, scale 1000 */
        TRI_PHASE_ACTIVE_POWER 						                = 0x1014, /* W    , 32-bit unsigned, scale 100  */
        TRI_PHASE_REACTIVE_POWER 					                = 0x1016, /* var  , 32-bit unsigned, scale 100  */
        TRI_PHASE_APPARENT_POWER 					                = 0x1018, /* VA   , 32-bit unsigned, scale 100  */
        TRI_PHASE_SIGN_OF_ACTIVE_POWER 				                = 0x101A, /* (2)  , 16-bit unsigned, scale 1    */
        TRI_PHASE_SIGN_OF_REACTIVE_POWER 			                = 0x101B, /* (2)  , 16-bit unsigned, scale 1    */
        TRI_PHASE_TOTAL_POSITIVE_ACTIVE_ENERGY2 	                = 0x101C, /* kWh  , 32-bit unsigned, scale 100  */
        TRI_PHASE_TOTAL_POSITIVE_REACTIVE_ENERGY2 	                = 0x101E, /* kvarh, 32-bit unsigned, scale 100  */

        TRI_PHASE_POWER_FACTOR 						                = 0x1024, /* 1/100, 16-bit unsigned, scale 1    */
        TRI_PHASE_SECTOR_OF_POWER_FACTOR 			                = 0x1025, /* (1)  , 16-bit unsigned, scale 1    */
        FREQUENCY 									                = 0x1026, /* Hz   , 16-bit unsigned, scale 10   */
        TRI_PHASE_AVERAGE_POWER 					                = 0x1027, /* W    , 32-bit unsigned, scale 100  */
        TRI_PHASE_PEAK_MAXIMUM_DEMAND 				                = 0x1029, /* W    , 32-bit unsigned, scale 100  */
        TIME_COUNTER_FOR_AVERAGE_POWER 				                = 0x102B, /* s    , 16-bit unsigned, scale 1    */
        PHASE1_ACTIVE_POWER 						                = 0x102C, /* W    , 32-bit unsigned, scale 100  */
        PHASE2_ACTIVE_POWER 						                = 0x102E, /* W    , 32-bit unsigned, scale 100  */
        PHASE3_ACTIVE_POWER 						                = 0x1030, /* W    , 32-bit unsigned, scale 100  */
        PHASE1_SIGN_OF_ACTIVE_POWER 				                = 0x1032, /* (2)  , 16-bit unsigned, scale 1    */
        PHASE2_SIGN_OF_ACTIVE_POWER 				                = 0x1033, /* (2)  , 16-bit unsigned, scale 1    */
        PHASE3_SIGN_OF_ACTIVE_POWER 			    	            = 0x1034, /* (2)  , 16-bit unsigned, scale 1    */
        PHASE1_REACTIVE_POWER 						                = 0x1035, /* var  , 32-bit unsigned, scale 100  */
        PHASE2_REACTIVE_POWER 						                = 0x1037, /* var  , 32-bit unsigned, scale 100  */
        PHASE3_REACTIVE_POWER 						                = 0x1039, /* var  , 32-bit unsigned, scale 100  */
        PHASE1_SIGN_OF_REACTIVE_POWER				                = 0x103B, /* (2)  , 16-bit unsigned, scale 1    */
        PHASE2_SIGN_OF_REACTIVE_POWER 				                = 0x103C, /* (2)  , 16-bit unsigned, scale 1    */
        PHASE3_SIGN_OF_REACTIVE_POWER 				                = 0x103D, /* (2)  , 16-bit unsigned, scale 1    */
        TRI_PHASE_PARTIAL_SECOND_TARIFF_POSITIVE_ACTIVE_ENERGY      = 0x103E, /* kWh  , 32-bit unsigned, scale 100  */
        TRI_PHASE_PARTIAL_SECOND_TARIFF_POSITIVE_REACTIVE_ENERGY    = 0x1040, /* kvarh, 32-bit unsigned, scale 100  */
        TRI_PHASE_SECOND_TARIFF_PEAK_MAXIMUM_DEMAND                 = 0x1042, /* W    , 32-bit unsigned, scale 100  */

        // (2) 0 = positive, 1 = negative
    };

    struct {
        std::uint16_t device_identifier; /* 0x300 */
        /* 0x301 -> 0x324 missing */
 	    std::uint32_t tri_phase_total_positive_active_energy; /* 0x325, 0x326 */
        /* 0x327, 0x328 missing */
 	    std::uint32_t tri_phase_total_positive_reactive_energy; /* 0x329, 0x32A */
        /* 0x32B, 0x32C missing */
        std::uint32_t tri_phase_partial_positive_active_energy; /* 0x32D, 0x32E, write 0x00 to reset */
        /* 0x32F, 0x330 missing */
        std::uint32_t tri_phase_partial_positive_reactive_energy; /* 0x331, 0x332, write 0x00 to reset */

        struct {
            std::uint32_t phase1_voltage; /* 0x1000, 0x1001 */
            std::uint32_t phase2_voltage; /* 0x1002, 0x1003 */
            std::uint32_t phase3_voltage; /* 0x1004, 0x1005 */
            std::uint32_t phase1_current; /* 0x1006, 0x1007 */
            std::uint32_t phase2_current; /* 0x1008, 0x1009 */
            std::uint32_t phase3_current; /* 0x100A, 0x100B */
        } __attribute__((packed, aligned(4)));

        /* 0x100C, 0x100D zero */

        struct {
            std::uint32_t l1_l2_voltage; /* 0x100E, 0x100F */
            std::uint32_t l2_l3_voltage; /* 0x1010, 0x1011 */
            std::uint32_t l3_l1_voltage; /* 0x1012, 0x1013 */
            std::uint32_t tri_phase_active_power; /* 0x1014, 0x1015 */
            std::uint32_t tri_phase_reactive_power; /* 0x1016, 0x1017 */
            std::uint32_t tri_phase_apparent_power; /* 0x1018, 0x1019 */
            std::uint16_t tri_phase_sign_of_active_power; /* 0x101A */
            std::uint16_t tri_phase_sign_of_reactive_power; /* 0x101B */
            std::uint32_t tri_phase_total_positive_active_energy2; /* 0x101C, 0x101D */
            std::uint32_t tri_phase_total_positive_reactive_energy2; /* 0x101E, 0x101F */
        } __attribute__((packed, aligned(4)));

        /* 0x1020, 0x1021 for future use */
        /* 0x1022, 0x1023 zero */

        struct {
            std::uint16_t tri_phase_power_factor; /* 0x1024 */
            std::uint16_t tri_phase_sector_of_power_factor; /* 0x1025, 0 = inductive, 1 = capacitive */
            std::uint16_t frequency; /* 0x1026 */
            std::uint32_t tri_phase_average_power; /* 0x1027, 0x1028 */
            std::uint32_t tri_phase_peak_maximum_demand; /* 0x1029, 0x102A */
            std::uint16_t time_counter_for_average_power;	 /* 0x102B */
            std::uint32_t phase1_active_power; /* 0x102C, 0x102D */
            std::uint32_t phase2_active_power; /* 0x102E, 0x102F */
            std::uint32_t phase3_active_power; /* 0x1030, 0x1031 */
            std::uint16_t phase1_sign_of_active_power; /* 0x1032 */
            std::uint16_t phase2_sign_of_active_power; /* 0x1033 */
            std::uint16_t phase3_sign_of_active_power; /* 0x1034 */
            std::uint32_t phase1_reactive_power; /* 0x1035, 0x1036 */
            std::uint32_t phase2_reactive_power; /* 0x1037, 0x1038 */
            std::uint32_t phase3_reactive_power; /* 0x1039, 0x103A */
            std::uint16_t phase1_sign_of_reactive_power; /* 0x103B */
            std::uint16_t phase2_sign_of_reactive_power; /* 0x103C */
            std::uint16_t phase3_sign_of_reactive_power; /* 0x103D */
            std::uint32_t tri_phase_partial_second_tariff_positive_active_energy; /* 0x103E, 0x103F */
            std::uint32_t tri_phase_partial_second_tariff_positive_reactive_energy; /* 0x1040, 0x1041 */
            std::uint32_t tri_phase_second_tariff_peak_maximum_demand; /* 0x1042, 0x1043 */
        } __attribute__((packed, aligned(4)));

        /* 0x1044, 0x1045 zero */
        /* 0x1046, 0x1047 zero */

    } read_data;

public:

    ime(std::pmr::memory_resource * const memory_resource,
        std::string_view name,
        std::string_view device_name,
        int slave_address,
        std::string_view master_clock_name = "",
        double nominal_frequency_hz = 1.0,
        std::string_view realm = "cems",
        std::size_t const size = 65536)
    : fabrix::component(memory_resource, name, realm, size)
    , pll_(nominal_frequency_hz)
    , device_name_(memory_resource)
    , master_clock_name_(master_clock_name)
    , slave_address_(slave_address)
    {
        device_name_ = device_name.data();

        if (verbose) std::clog << "Initializing modbus connection " << device_name_ << std::endl;

        mb_ctx_ = modbus_new_rtu(device_name_.c_str(), 19200, 'N', 8, 1);
        if (mb_ctx_) {
            //modbus_set_debug(mb_ctx_, 1);

            //* Connect
            int mb_error = modbus_connect(mb_ctx_);
            if (mb_error == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                cleanup();
                return;
            }
            //* Set slave number
            mb_error = modbus_set_slave(mb_ctx_, slave_address);
            if (mb_error == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                cleanup();
                return;
            }
            /* Define a new timeout of 700ms */
            mb_error = modbus_set_response_timeout(mb_ctx_, 0, 700'000);
            if (mb_error == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                cleanup();
                return;
            }

            read_config();
        } else {
            std::cerr << "Error: Failed to create Modbus context for device '" << device_name_.c_str() << "' with slave address " << slave_address_ << ".\n";
            std::cerr << (std::pmr::string("Modbus Error: ", &memory_pool) + modbus_strerror(errno)).c_str() << "'.\n";
        }
    }

    ~ime() noexcept override {
        cleanup();
    }

    void run() {
        // Loop
        try {
            do {
                sync_pll();
                process_until(pll_.at_tick());
                read_and_publish();
                pll_.advance();
            } while (!stop);
        } catch (std::exception const & e) {
            std::cerr << "Run loop error: " << e.what() << std::endl;
        }
    }

protected:

    void on_start() override {
        std::clog << "Component " << identifier().realm() << "::" << identifier().name() << " is online with pid " << getpid() << ".\n";

        // Create area
        if ((instant_reading_area_ = fabrix::rcu::create_area(*this, TOPIC_NAME_INSTANT_READING))) {
            // Set grace to 5 seconds; we consider after that after that period data may be reclaimed.
            instant_reading_area_.grace_period(5);
            // Create Flatbuffers object template
            instant_reading_storage_template_ = instant_reading_area_.create_storage(sizeof(CEMS::IME::InstantReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::IME::InstantReading()));
            });
        } else {
            std::cerr << "Error: Failed to create RCU area for '" << TOPIC_NAME_INSTANT_READING << "'.\n";
        }

        // Create area
        if ((energy_reading_area_ = fabrix::rcu::create_area(*this, TOPIC_NAME_ENERGY_READING))) {
            // Set grace to 5 seconds; we consider after that after that period data may be reclaimed.
            energy_reading_area_.grace_period(5);
            // Create Flatbuffers object template
            energy_reading_storage_template_ = energy_reading_area_.create_storage(sizeof(CEMS::IME::EnergyReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::IME::EnergyReading()));
            });
        } else {
            std::cerr << "Error: Failed to create RCU area for '" << TOPIC_NAME_ENERGY_READING << "'.\n";
        }

        // Find master clock area if specified
        if (not master_clock_name_.empty()) {
            for (auto && name : list_components(true)) {
                if (name == master_clock_name_) {
                    if (auto endpoint = open_endpoint(name)) {
                        clock_tick_area_ = fabrix::rcu::find_area(endpoint, TOPIC_NAME_CLOCK_TICK);
                    }
                }
            }
        }
    }

    void on_endpoint_create(std::string_view const name, bool const is_private) override {
        if (is_private) return;
        if (name == master_clock_name_) {
            if (auto endpoint = open_endpoint(name)) {
                clock_tick_area_ = fabrix::rcu::find_area(endpoint, TOPIC_NAME_CLOCK_TICK);
            }
        }
    }

    void on_endpoint_remove(std::string_view const name) override {
        if (name == master_clock_name_) {
            clock_tick_area_.reset();
        }
    }

    bool on_subscribe_request(endpoint_type sender_endpoint, endpoint_type delivery_endpoint, std::string_view topic_name) override {
        if (topic_name == TOPIC_NAME_INSTANT_READING or topic_name == TOPIC_NAME_ENERGY_READING) {
            std::clog << "Request 'subscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << '\'';
            if (!(delivery_endpoint == sender_endpoint)) std::clog << " with delivery point " << delivery_endpoint.identifier().name();
            std::clog << " accepted." << std::endl;
            return true;
        } else {
            std::cerr << "Request 'subscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << "' declined." << std::endl;
        }
        return false;
    }

    bool on_unsubscribe_request(endpoint_type sender_endpoint, endpoint_type delivery_endpoint, std::string_view topic_name) override {
        if (topic_name == TOPIC_NAME_INSTANT_READING or topic_name == TOPIC_NAME_ENERGY_READING) {
            std::clog << "Request 'unsubscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << '\'';
            if (delivery_endpoint) std::clog << " with delivery point " << delivery_endpoint.identifier().name();
            std::clog << " accepted." << std::endl;
            return true;
        } else {
            std::cerr << "Request 'unsubscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << "' declined." << std::endl;
        }
        return false;
    }

    void on_error(endpoint_type other_end, error_type error_code) override {
        std::cerr << "Error: '" << (other_end ? other_end.identifier().name() : "<>") << "' with error code " << error_code << '\n';
    }

    bool on_halt_component_request(MAYBE_UNUSED endpoint_type sender_endpoint) override {
        stop = true;
        return true;
    }

    void on_list_topics_request(MAYBE_UNUSED endpoint_type sender_endpoint, std::pmr::list<std::pmr::string> & topics) override {
        topics.emplace_back( TOPIC_NAME_INSTANT_READING );
        topics.emplace_back( TOPIC_NAME_ENERGY_READING );
    }

private:

    void sync_pll() {
        fabrix::rcu::scoped_access access(clock_tick_area_);
        CEMS::MasterClock::ClockTick const * const tick = flatbuffers::GetRoot<CEMS::MasterClock::ClockTick>(access.get());
        if (tick && tick->timestamp() > last_clock_timestamp_) {
            last_clock_timestamp_ = tick->timestamp();
            pll_.synchronize(last_clock_timestamp_);
        }
    }

    bool read_config() {
        if (-1 == modbus_read_registers(mb_ctx_, DEVICE_IDENTIFIER, 1, &read_data.device_identifier)) {
            fprintf(stderr, "Error reading device identifier: %s\n", modbus_strerror(errno));
            return false;
        }
        return true;
    }

    void read_and_publish() {
        if (!mb_ctx_) return;
        // Read instanteneous and energy values
        if (-1 == modbus_read_registers(mb_ctx_, TRI_PHASE_TOTAL_POSITIVE_ACTIVE_ENERGY, 2, (uint16_t*)&read_data.tri_phase_total_positive_active_energy)) return;
        if (-1 == modbus_read_registers(mb_ctx_, TRI_PHASE_TOTAL_POSITIVE_REACTIVE_ENERGY, 2, (uint16_t*)&read_data.tri_phase_total_positive_reactive_energy)) return;
        if (-1 == modbus_read_registers(mb_ctx_, TRI_PHASE_PARTIAL_POSITIVE_ACTIVE_ENERGY, 2, (uint16_t*)&read_data.tri_phase_partial_positive_active_energy)) return;
        if (-1 == modbus_read_registers(mb_ctx_, TRI_PHASE_PARTIAL_POSITIVE_REACTIVE_ENERGY, 2, (uint16_t*)&read_data.tri_phase_partial_positive_reactive_energy)) return;
        /* 0x1000 -> 0x100B */
        if (-1 == modbus_read_registers(mb_ctx_, PHASE1_VOLTAGE, 12, (uint16_t*)&read_data.phase1_voltage)) return;
        /* 0x100E -> 0x101F */
        if (-1 == modbus_read_registers(mb_ctx_, L1_L2_VOLTAGE, 18, (uint16_t*)&read_data.l1_l2_voltage)) return;
        /* 0x1024 -> 0x1043 */
        if (-1 == modbus_read_registers(mb_ctx_, TRI_PHASE_POWER_FACTOR, 32, (uint16_t*)&read_data.tri_phase_power_factor)) return;

        // Swap bytes
    	read_data.tri_phase_total_positive_active_energy = swap_lr(read_data.tri_phase_total_positive_active_energy);
    	read_data.tri_phase_total_positive_reactive_energy = swap_lr(read_data.tri_phase_total_positive_reactive_energy);
        read_data.tri_phase_partial_positive_active_energy = swap_lr(read_data.tri_phase_partial_positive_active_energy);
        read_data.tri_phase_partial_positive_reactive_energy = swap_lr(read_data.tri_phase_partial_positive_reactive_energy);
        read_data.phase1_voltage = swap_lr(read_data.phase1_voltage);
        read_data.phase2_voltage = swap_lr(read_data.phase2_voltage);
        read_data.phase3_voltage = swap_lr(read_data.phase3_voltage);
        read_data.phase1_current = swap_lr(read_data.phase1_current);
        read_data.phase2_current = swap_lr(read_data.phase2_current);
        read_data.phase3_current = swap_lr(read_data.phase3_current);
        read_data.l1_l2_voltage = swap_lr(read_data.l1_l2_voltage);
        read_data.l2_l3_voltage = swap_lr(read_data.l2_l3_voltage);
        read_data.l3_l1_voltage = swap_lr(read_data.l3_l1_voltage);
        read_data.tri_phase_active_power = swap_lr(read_data.tri_phase_active_power);
        read_data.tri_phase_reactive_power = swap_lr(read_data.tri_phase_reactive_power);
        read_data.tri_phase_apparent_power = swap_lr(read_data.tri_phase_apparent_power);
        read_data.tri_phase_total_positive_active_energy2 = swap_lr(read_data.tri_phase_total_positive_active_energy2);
        read_data.tri_phase_total_positive_reactive_energy2 = swap_lr(read_data.tri_phase_total_positive_reactive_energy2);
        read_data.tri_phase_average_power = swap_lr(read_data.tri_phase_average_power);
        read_data.tri_phase_peak_maximum_demand = swap_lr(read_data.tri_phase_peak_maximum_demand);
        read_data.phase1_active_power = swap_lr(read_data.phase1_active_power);
        read_data.phase2_active_power = swap_lr(read_data.phase2_active_power);
        read_data.phase3_active_power = swap_lr(read_data.phase3_active_power);
        read_data.phase1_reactive_power = swap_lr(read_data.phase1_reactive_power);
        read_data.phase2_reactive_power = swap_lr(read_data.phase2_reactive_power);
        read_data.phase3_reactive_power = swap_lr(read_data.phase3_reactive_power);
        read_data.tri_phase_partial_second_tariff_positive_active_energy = swap_lr(read_data.tri_phase_partial_second_tariff_positive_active_energy);
        read_data.tri_phase_partial_second_tariff_positive_reactive_energy = swap_lr(read_data.tri_phase_partial_second_tariff_positive_reactive_energy);
        read_data.tri_phase_second_tariff_peak_maximum_demand = swap_lr(read_data.tri_phase_second_tariff_peak_maximum_demand);

        auto average_voltage_line_neutral = (static_cast<float>(read_data.phase1_voltage) +
                                             static_cast<float>(read_data.phase2_voltage) +
                                             static_cast<float>(read_data.phase3_voltage)) / 3.0f;
        auto average_voltage_line_line = (static_cast<float>(read_data.l1_l2_voltage) +
                                          static_cast<float>(read_data.l2_l3_voltage) +
                                          static_cast<float>(read_data.l3_l1_voltage)) / 3.0f;
        auto average_current = (static_cast<float>(read_data.phase1_current) +
                                static_cast<float>(read_data.phase2_current) +
                                static_cast<float>(read_data.phase3_current)) / 3.0f;

        double const now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0;

        CEMS::IME::InstantReading instant_reading {
            now,
            static_cast<std::uint8_t>(slave_address_),
            read_data.device_identifier,
            static_cast<float>(read_data.frequency) / 10.0f,
            static_cast<float>(read_data.l1_l2_voltage) / 1000.0f,
            static_cast<float>(read_data.l2_l3_voltage) / 1000.0f,
            static_cast<float>(read_data.l3_l1_voltage) / 1000.0f,
            read_data.tri_phase_sign_of_active_power == 0 ? static_cast<float>(read_data.tri_phase_active_power) / 100.0f : -static_cast<float>(read_data.tri_phase_active_power) / 100.0f,
            read_data.tri_phase_sign_of_reactive_power == 0 ? static_cast<float>(read_data.tri_phase_reactive_power) / 100.0f : -static_cast<float>(read_data.tri_phase_reactive_power) / 100.0f,
            static_cast<float>(read_data.tri_phase_apparent_power) / 100.0f,
            static_cast<float>(read_data.tri_phase_power_factor),
            CEMS::IME::Sector(read_data.tri_phase_sector_of_power_factor),
            static_cast<float>(read_data.tri_phase_peak_maximum_demand) / 100.0f,
            static_cast<float>(read_data.tri_phase_second_tariff_peak_maximum_demand) / 100.0f,
            read_data.time_counter_for_average_power,
            static_cast<float>(read_data.tri_phase_average_power) / 100.0f,
            static_cast<float>(average_voltage_line_neutral) / 1000.0f,
            static_cast<float>(average_voltage_line_line) / 1000.0f,
            static_cast<float>(average_current) / 1000.0f,
            read_data.phase1_sign_of_active_power == 0 ? static_cast<float>(read_data.phase1_active_power) / 100.0f : -static_cast<float>(read_data.phase1_active_power) / 100.0f,
            read_data.phase1_sign_of_reactive_power == 0 ? static_cast<float>(read_data.phase1_reactive_power) / 100.0f : -static_cast<float>(read_data.phase1_reactive_power) / 100.0f,
            static_cast<float>(read_data.phase1_voltage) / 1000.0f,
            static_cast<float>(read_data.phase1_current) / 1000.0f,
            read_data.phase2_sign_of_active_power == 0 ? static_cast<float>(read_data.phase2_active_power) / 100.0f : -static_cast<float>(read_data.phase2_active_power) / 100.0f,
            read_data.phase2_sign_of_reactive_power == 0 ? static_cast<float>(read_data.phase2_reactive_power) / 100.0f : -static_cast<float>(read_data.phase2_reactive_power) / 100.0f,
            static_cast<float>(read_data.phase2_voltage) / 1000.0f,
            static_cast<float>(read_data.phase2_current) / 1000.0f,
            read_data.phase3_sign_of_active_power == 0 ? static_cast<float>(read_data.phase3_active_power) / 100.0f : -static_cast<float>(read_data.phase3_active_power) / 100.0f,
            read_data.phase3_sign_of_reactive_power == 0 ? static_cast<float>(read_data.phase3_reactive_power) / 100.0f : -static_cast<float>(read_data.phase3_reactive_power) / 100.0f,
            static_cast<float>(read_data.phase3_voltage) / 1000.0f,
            static_cast<float>(read_data.phase3_current) / 1000.0f
        };

        CEMS::IME::EnergyReading energy_reading {
            now,
            static_cast<std::uint8_t>(slave_address_),
            read_data.device_identifier,
            static_cast<float>(read_data.tri_phase_total_positive_active_energy) / 100.0f,
            static_cast<float>(read_data.tri_phase_total_positive_reactive_energy) / 100.0f,
            static_cast<float>(read_data.tri_phase_partial_positive_active_energy) / 100.0f,
            static_cast<float>(read_data.tri_phase_partial_positive_reactive_energy) / 100.0f,
            static_cast<float>(read_data.tri_phase_total_positive_active_energy2) / 100.0f,
            static_cast<float>(read_data.tri_phase_total_positive_reactive_energy2) / 100.0f,
            static_cast<float>(read_data.tri_phase_partial_second_tariff_positive_active_energy) / 100.0f,
            static_cast<float>(read_data.tri_phase_partial_second_tariff_positive_reactive_energy) / 100.0f
        };

        if (verbose) {
            std::cout << "\033[2J" << instant_reading << '\n' << energy_reading << std::endl;
        }

        if (CEMS::IME::Verifier::Check(instant_reading)) {
            auto storage = instant_reading_area_.try_reuse(instant_reading_storage_template_.size(), true);
            if (not storage) storage = instant_reading_storage_template_.clone();
            CEMS::IME::InstantReading * const field = flatbuffers::GetMutableRoot<CEMS::IME::InstantReading>(storage.get());
            *field = instant_reading;
            instant_reading_area_.publish_storage(storage);
            broadcast_topic(TOPIC_NAME_INSTANT_READING, &instant_reading, sizeof(instant_reading));
            instant_reading_area_.tick();
            instant_reading_area_.reclaim();
        }
        if (CEMS::IME::Verifier::Check(energy_reading)) {
            auto storage = energy_reading_area_.try_reuse(energy_reading_storage_template_.size(), true);
            if (not storage) storage = energy_reading_storage_template_.clone();
            CEMS::IME::EnergyReading * const field = flatbuffers::GetMutableRoot<CEMS::IME::EnergyReading>(storage.get());
            *field = energy_reading;
            energy_reading_area_.publish_storage(storage);
            broadcast_topic(TOPIC_NAME_ENERGY_READING, &energy_reading, sizeof(energy_reading));
            energy_reading_area_.tick();
            energy_reading_area_.reclaim();
        }
    }

    void cleanup() {
        modbus_close(mb_ctx_);
        modbus_free(mb_ctx_);
        mb_ctx_ = nullptr;
    }

    phase_lock_loop pll_;
    std::pmr::string device_name_;
    std::pmr::string master_clock_name_;
    fabrix::rcu::reader clock_tick_area_;
    fabrix::rcu::writer instant_reading_area_;
    fabrix::rcu::writer energy_reading_area_;
    fabrix::rcu::storage instant_reading_storage_template_;
    fabrix::rcu::storage energy_reading_storage_template_;
    modbus_t * mb_ctx_ = nullptr;
    double last_clock_timestamp_ = 0.0;
    int slave_address_;
};

} // local namespace

int main(int argc, char *argv[]) {
    // Handle arguments
    parse_args(argc, argv);

    // Random seed
    std::srand(std::time(nullptr));

    // Register 'break' handler
    std::signal(SIGINT, [](int) { stop = true; });
    std::signal(SIGTERM, [](int) { stop = true; });

    // Use real-time scheduling policy.
    int const min_priority = sched_get_priority_min(SCHED_RR);
    int const max_priority = sched_get_priority_max(SCHED_RR);
    struct sched_param param;
    int const my_priority = min_priority + 1;
    param.sched_priority = std::min(std::max(my_priority, min_priority), max_priority);
    sched_setscheduler(0, SCHED_RR, &param);

    // Use polymorphic memory system for memory management
    std::pmr::set_default_resource(&memory_pool);

    // Execute component
    ime(&memory_pool, component_name, device_name, slave_address, master_clock_name, read_frequency_hz).run();

    // Cleanup
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);

    return exit_code;
}
