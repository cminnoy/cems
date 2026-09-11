#include <csignal>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <array>
#include <thread>
#include <memory_resource>

#include <getopt.h>

#include <modbus/modbus.h>
#include <util/logger.hpp>
#include <fabrix/fabrix.hpp>
#include <circutor_check.hpp>
#include <phase_lock_loop/phase_lock_loop.hpp>
#include <schema/circutor_generated.h>
#include <schema/master_clock_generated.h>

namespace {

bool stop = false;
int exit_code = EXIT_SUCCESS;

std::pmr::unsynchronized_pool_resource memory_pool;
std::pmr::string component_name(&memory_pool);
std::pmr::string component_realm(&memory_pool);
std::pmr::string device_name("/dev/circutor", &memory_pool);
std::pmr::string master_clock_name(&memory_pool);
double read_frequency_hz = 1.0;
int slave_address = 1;
int thread_priority = 10;
bool verbose = false;

void print_help() {
    std::cout << "Usage:\n"
                 "  circutor [-n|--name <component_name>] [-r|--realm <component_realm>] [-d|--device <device>] [-s|--slave <address>] [-c|--clock <component_name>] [-v|--verbose]\n"
                 "Options:\n"
                 "  -n, --name <component_name>\n"
                 "      Specify the component name [mandatory]\n"
                 "  -r, --realm <component_realm>\n"
                 "      Specify the component realm [mandatory]\n"
                 "  -d, --device <device>\n"
                 "      Specify the device name [default: /dev/circutor]\n"
                 "  -s, --slave <address>\n"
                 "      Set the Modbus slave address [default: 1]\n"
                 "  -c, --clock <component_name>\n"
                 "      Specify the master clock component [optional]\n"
                 "  -f, --frequency <Hz>\n"
                 "      Set the frequency [default: 1]\n"
                 "  -p, --priority <priority>\n"
                 "      Set the priority of the real time process [default: 10]\n"
                 "  -v, --verbose\n"
                 "      Enable verbose output\n"
                 "  -h, --help\n"
                 "      Display this help and exit\n";
    std::cout.flush();
}

void parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"name", required_argument, nullptr, 'n'},
        {"realm", required_argument, nullptr, 'r'},
        {"device", required_argument, nullptr, 'd'},
        {"slave", required_argument, nullptr, 's'},
        {"clock", required_argument, nullptr, 'c'},
        {"frequency", required_argument, nullptr, 'f'},
        {"priority", required_argument, nullptr, 'p'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "d:s:n:r:c:f:p:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'n':
                component_name = optarg;
                break;
            case 'r':
                component_realm = optarg;
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
            case 'p':
                thread_priority = std::atoi(optarg);
                if (thread_priority < sched_get_priority_min(SCHED_RR) or thread_priority > sched_get_priority_max(SCHED_RR)) {
                    std::cerr << "Error: Priority must be between " << sched_get_priority_min(SCHED_RR) << " and " << sched_get_priority_max(SCHED_RR) << ".\n";
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
                exit(EXIT_FAILURE);
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

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::Circutor::InstantReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"      ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "phase1_voltage" ANSI_TOK ":" ANSI_NRM << o.phase1_voltage()
        << ANSI_TOK "," ANSI_LBL "phase2_voltage" ANSI_TOK ":" ANSI_NRM << o.phase2_voltage()
        << ANSI_TOK "," ANSI_LBL "phase3_voltage" ANSI_TOK ":" ANSI_NRM << o.phase3_voltage()
        << ANSI_TOK "," ANSI_LBL "phase1_current" ANSI_TOK ":" ANSI_NRM << o.phase1_current()
        << ANSI_TOK "," ANSI_LBL "phase2_current" ANSI_TOK ":" ANSI_NRM << o.phase2_current()
        << ANSI_TOK "," ANSI_LBL "phase3_current" ANSI_TOK ":" ANSI_NRM << o.phase3_current()
        << ANSI_TOK "," ANSI_LBL "phase1_cos_phi" ANSI_TOK ":" ANSI_NRM << o.phase1_cos_phi()
        << ANSI_TOK "," ANSI_LBL "phase2_cos_phi" ANSI_TOK ":" ANSI_NRM << o.phase2_cos_phi()
        << ANSI_TOK "," ANSI_LBL "phase3_cos_phi" ANSI_TOK ":" ANSI_NRM << o.phase3_cos_phi()
        << ANSI_TOK "," ANSI_LBL "phase1_active_power" ANSI_TOK ":" ANSI_NRM << o.phase1_active_power()
        << ANSI_TOK "," ANSI_LBL "phase2_active_power" ANSI_TOK ":" ANSI_NRM << o.phase2_active_power()
        << ANSI_TOK "," ANSI_LBL "phase3_active_power" ANSI_TOK ":" ANSI_NRM << o.phase3_active_power()
        << ANSI_TOK "," ANSI_LBL "total_active_power" ANSI_TOK ":" ANSI_NRM << o.total_active_power()
        << ANSI_TOK "," ANSI_LBL "phase1_reactive_power" ANSI_TOK ":" ANSI_NRM << o.phase1_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase2_reactive_power" ANSI_TOK ":" ANSI_NRM << o.phase2_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase3_reactive_power" ANSI_TOK ":" ANSI_NRM << o.phase3_reactive_power()
        << ANSI_TOK "," ANSI_LBL "total_reactive_power" ANSI_TOK ":" ANSI_NRM << o.total_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase1_apparent_power" ANSI_TOK ":" ANSI_NRM << o.phase1_apparent_power()
        << ANSI_TOK "," ANSI_LBL "phase2_apparent_power" ANSI_TOK ":" ANSI_NRM << o.phase2_apparent_power()
        << ANSI_TOK "," ANSI_LBL "phase3_apparent_power" ANSI_TOK ":" ANSI_NRM << o.phase3_apparent_power()
        << ANSI_TOK "," ANSI_LBL "total_apparent_power" ANSI_TOK ":" ANSI_NRM << o.total_apparent_power()
        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::Circutor::EnergyReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"      ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "imported_active_energy" ANSI_TOK ":" ANSI_NRM << o.imported_active_energy()
        << ANSI_TOK "," ANSI_LBL "exported_active_energy" ANSI_TOK ":" ANSI_NRM << o.exported_active_energy()
        << ANSI_TOK "," ANSI_LBL "q1_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q1_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q2_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q2_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q3_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q3_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q4_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q4_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "partial_imported_active_energy" ANSI_TOK ":" ANSI_NRM << o.imported_active_energy()
        << ANSI_TOK "," ANSI_LBL "partial_exported_active_energy" ANSI_TOK ":" ANSI_NRM << o.exported_active_energy()
        << ANSI_TOK "," ANSI_LBL "q1_partial_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q1_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q2_partial_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q2_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q3_partial_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q3_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "q4_partial_reactive_energy" ANSI_TOK ":" ANSI_NRM << o.q4_reactive_energy()
        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

class circutor : public fabrix::component {
private:

    char const * const TOPIC_NAME_INSTANT_READING = "InstantReading";
    char const * const TOPIC_NAME_ENERGY_READING = "EnergyReading";
    char const * const TOPIC_NAME_CLOCK_TICK = "ClockTick";

    enum CircutorAddress {
        IMPORTED_ACTIVE_ENERGY                          = 0x0000, /* Wh   , 32-bit, scale 1 */
        EXPORTED_ACTIVE_ENERGY                          = 0x0002, /* Wh   , 32-bit, scale 1 */
        Q1_REACTIVE_ENERGY                              = 0x0004, /* varh , 32-bit, scale 1 */
        Q2_REACTIVE_ENERGY                              = 0x0006, /* varh , 32-bit, scale 1 */
        Q3_REACTIVE_ENERGY                              = 0x0008, /* varh , 32-bit, scale 1 */
        Q4_REACTIVE_ENERGY                              = 0x000A, /* varh , 32-bit, scale 1 */

        DIGITAL_INPUT_STATUS                            = 0x0020, /* bitfield, 16-bit */

        PARTIAL_IMPORTED_ACTIVE_ENERGY                  = 0x0030, /* Wh   , 32-bit, scale 1 */
        PARTIAL_EXPORTED_ACTIVE_ENERGY                  = 0x0032, /* Wh   , 32-bit, scale 1 */
        Q1_PARTIAL_REACTIVE_ENERGY                      = 0x0034, /* varh , 32-bit, scale 1 */
        Q2_PARTIAL_REACTIVE_ENERGY                      = 0x0036, /* varh , 32-bit, scale 1 */
        Q3_PARTIAL_REACTIVE_ENERGY                      = 0x0038, /* varh , 32-bit, scale 1 */
        Q4_PARTIAL_REACTIVE_ENERGY                      = 0x003A, /* varh , 32-bit, scale 1 */

        HIGH_FIRMWARE_VERSION_NUMBER                    = 0x0050, /* bitfield, 16-bit */
        LOW_FIRMWARE_VERSION_NUMBER                     = 0x0051, /* bitfield, 16-bit */
        REVISED_FIRMWARE_VERSION_NUMBER                 = 0x0052, /* bitfield, 16-bit */

        SERIAL_NUMBER                                   = 0x0060, /* bitfield, 32-bit */

        IDENTIFIER_ID_NUMBER                            = 0x0068, /* bitfield, 32-bit */

        IMPULSE_OUTPUT_TYPE                             = 0x0080, /* 0: Active energy, 1: Reactive energy, 16-bit, default 0 */
        IMPULSE_OUTPUT_WEIGHT                           = 0x0081, /* Wh/impulse 0..99999, 16-bit */

        COST_OF_PARTIAL_CONSUMPTION                     = 0x00C0, /* currency units, 32-bit, scale 100 */
        ATMOSPHERIC_EMISSIONS_OF_PARTIAL_CONSUMPTION    = 0x00C2, /* KgCO2eq, 32-bit, scale 100 */
        HOURS_OF_PARTIAL_OPERATION_IN_SECONDS           = 0x00C4, /* s, 32-bit, scale 1 */
        HOURS_OF_TOTAL_OPERATION_IN_SECONDS             = 0x00C6, /* s, 32-bit, scale 1 */

        TOTAL_IMPULSE_COUNT                             = 0x0180, /* impulses, 32-bit, scale 1 */
        PARTIAL_IMPULSE_COUNT                           = 0x0182, /* impulses, 32-bit, scale 1 */

        MODBUS_ADDRESS                                  = 0x03E8, /* 1..254, 16-bit, default 1 */
        TRANSMISSION_RATE                               = 0x03E9, /* 0: 9600 bps, 1: 19200 bps, 2: 38400 bps, 16-bit, default 0 */

        COMMUNICATION_CONFIGURATION                     = 0x03EA, /* 0: 8N1, 1: 8E1, 2: 8O1, 3: 8N2, 4: 8E2, 5: 8O2, 16-bit, default 0 */

        DIGITAL_INPUT_TYPE                              = 0x0454, /* 0: Tariff, 1: Impulse counter, 16-bit, default 0 */

        PHASE1_VOLTAGE                                  = 0x0732, /* V    , 32-bit, scale 10 */
        PHASE2_VOLTAGE                                  = 0x0734, /* V    , 32-bit, scale 10 */
        PHASE3_VOLTAGE                                  = 0x0736, /* V    , 32-bit, scale 10 */
        PHASE1_CURRENT                                  = 0x0738, /* A    , 32-bit, scale 100 */
        PHASE2_CURRENT                                  = 0x073A, /* A    , 32-bit, scale 100 */
        PHASE3_CURRENT                                  = 0x073C, /* A    , 32-bit, scale 100 */
        PHASE1_COS_PHI                                  = 0x073E, /*      , 32-bit, scale 100 */
        PHASE2_COS_PHI                                  = 0x0740, /*      , 32-bit, scale 100 */
        PHASE3_COS_PHI                                  = 0x0742, /*      , 32-bit, scale 100 */

        PHASE1_ACTIVE_POWER                             = 0x0746, /* W    , 32-bit, scale 1 */
        PHASE2_ACTIVE_POWER                             = 0x0748, /* W    , 32-bit, scale 1 */
        PHASE3_ACTIVE_POWER                             = 0x074A, /* W    , 32-bit, scale 1 */
        TOTAL_ACTIVE_POWER                              = 0x074C, /* W    , 32-bit, scale 1 */
        PHASE1_REACTIVE_POWER                           = 0x074E, /* var  , 32-bit, scale 1 */
        PHASE2_REACTIVE_POWER                           = 0x0750, /* var  , 32-bit, scale 1 */
        PHASE3_REACTIVE_POWER                           = 0x0752, /* var  , 32-bit, scale 1 */
        TOTAL_REACTIVE_POWER                            = 0x0754, /* var  , 32-bit, scale 1 */
        PHASE1_APPARENT_POWER                           = 0x0756, /* VA   , 32-bit, scale 1 */
        PHASE2_APPARENT_POWER                           = 0x0758, /* VA   , 32-bit, scale 1 */
        PHASE3_APPARENT_POWER                           = 0x075A, /* VA   , 32-bit, scale 1 */
        TOTAL_APPARENT_POWER                            = 0x075C, /* VA   , 32-bit, scale 1 */

        PARTIAL_ENERGY_RESET                            = 0x0800, /* Write 0xFF00 to activate */

        ENERGY_METER_MODEL                              = 0xF010  /* bitfield, 12 registers (24 bytes), ASCII string */
    };

    struct {

        struct {
            std::int32_t phase1_voltage;           /* 0x0732, 0x0733 */
            std::int32_t phase2_voltage;           /* 0x0734, 0x0735 */
            std::int32_t phase3_voltage;           /* 0x0736, 0x0737 */
            std::int32_t phase1_current;           /* 0x0738, 0x0739 */
            std::int32_t phase2_current;           /* 0x073A, 0x073B */
            std::int32_t phase3_current;           /* 0x073C, 0x073D */
            std::int32_t phase1_cos_phi;           /* 0x073E, 0x073F */
            std::int32_t phase2_cos_phi;           /* 0x0740, 0x0741 */
            std::int32_t phase3_cos_phi;           /* 0x0742, 0x0743 */
        } __attribute((packed, aligned(4)));

        struct {
            std::int32_t phase1_active_power;      /* 0x0746, 0x0747 */
            std::int32_t phase2_active_power;      /* 0x0748, 0x0749 */
            std::int32_t phase3_active_power;      /* 0x074A, 0x074B */
            std::int32_t total_active_power;       /* 0x074C, 0x074D */
            std::int32_t phase1_reactive_power;    /* 0x074E, 0x074F */
            std::int32_t phase2_reactive_power;    /* 0x0750, 0x0751 */
            std::int32_t phase3_reactive_power;    /* 0x0752, 0x0753 */
            std::int32_t total_reactive_power;     /* 0x0754, 0x0755 */
            std::int32_t phase1_apparent_power;    /* 0x0756, 0x0757 */
            std::int32_t phase2_apparent_power;    /* 0x0758, 0x0759 */
            std::int32_t phase3_apparent_power;    /* 0x075A, 0x075B */
            std::int32_t total_apparent_power;     /* 0x075C, 0x075D */
        } __attribute__((packed, aligned(4)));

        struct {
            std::int32_t imported_active_energy;   /* 0x0000, 0x0001 */
            std::int32_t exported_active_energy;   /* 0x0002, 0x0003 */
            std::uint32_t q1_reactive_energy;      /* 0x0004, 0x0005 */
            std::uint32_t q2_reactive_energy;      /* 0x0006, 0x0007 */
            std::uint32_t q3_reactive_energy;      /* 0x0008, 0x0009 */
            std::uint32_t q4_reactive_energy;      /* 0x000A, 0x000B */
        } __attribute__((packed, aligned(4)));

        struct {
            std::int32_t partial_imported_active_energy;   /* 0x0030, 0x0031 */
            std::int32_t partial_exported_active_energy;   /* 0x0032, 0x0033 */
            std::int32_t q1_partial_reactive_energy;       /* 0x0034, 0x0035 */
            std::int32_t q2_partial_reactive_energy;       /* 0x0036, 0x0037 */
            std::int32_t q3_partial_reactive_energy;       /* 0x0038, 0x0039 */
            std::int32_t q4_partial_reactive_energy;       /* 0x003A, 0x003B */
        } __attribute__((packed, aligned(4)));

        struct {
            std::uint16_t digital_input_status;     /* 0x0020 */
            std::uint32_t total_impulse_count;      /* 0x0180 */
            std::uint32_t partial_impulse_count;    /* 0x0182 */
        } __attribute__((packed, aligned(4)));

        struct {
            std::int32_t cost_of_partial_consumption;                   /* 0x00C0 */
            std::int32_t atmospheric_emissions_of_partial_consumption;  /* 0x00C2 */
            std::int32_t hours_of_partial_operation_in_seconds;         /* 0x00C4 */
            std::int32_t hours_of_total_operation_in_seconds;           /* 0x00C6 */
        } __attribute__((packed, aligned(4)));

    } __attribute__((packed, aligned(4))) read_data;

public:

    circutor(std::pmr::memory_resource * const memory_resource,
             std::string_view name,
             std::string_view realm,
             std::string_view device_name,
             int slave_address,
             std::string_view master_clock_name = "",
             double nominal_frequency_hz = 1.0,
             std::size_t const size = 65536)
    : fabrix::component(memory_resource, name, realm, size)
    , pll_(nominal_frequency_hz)
    , device_name_(memory_resource)
    , master_clock_name_(master_clock_name)
    , slave_address_(slave_address)
    {
        device_name_ = device_name.data();
        connect();
    }

    ~circutor() noexcept override {
        cleanup();
    }

    void run() {
        auto last_connect_attempt = std::chrono::steady_clock::now();
        try {
            do {
                // If no modbus connection, retry every 15 seconds
                if (!stop and !mb_ctx_) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - last_connect_attempt >= std::chrono::seconds(15)) {
                        last_connect_attempt = now;
                        connect();
                    }
                }
                process_until(pll_.at_tick());
                read_and_publish_instantaneous();
                read_and_publish_energy();
                pll_.advance();
                sync_pll();
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
            instant_reading_storage_template_ = instant_reading_area_.create_storage(sizeof(CEMS::Circutor::InstantReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::Circutor::InstantReading()));
            });
        } else {
            std::cerr << "Error: Failed to create RCU area for '" << TOPIC_NAME_INSTANT_READING << "'.\n";
        }

        // Create area
        if ((energy_reading_area_ = fabrix::rcu::create_area(*this, TOPIC_NAME_ENERGY_READING))) {
            // Set grace to 5 seconds; we consider after that after that period data may be reclaimed.
            energy_reading_area_.grace_period(5);
            // Create Flatbuffers object template
            energy_reading_storage_template_ = energy_reading_area_.create_storage(sizeof(CEMS::Circutor::EnergyReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::Circutor::EnergyReading()));
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

    void on_list_topics_request(MAYBE_UNUSED endpoint_type sender_endpoint, list_type<string_type> & topics) override {
        topics.emplace_back( TOPIC_NAME_INSTANT_READING );
        topics.emplace_back( TOPIC_NAME_ENERGY_READING );
    }

private:

    void connect() {
        if (verbose) std::clog << "Initializing modbus connection " << device_name_ << std::endl;

        // Register values for Circutor 0x03E9: 0=9600, 1=19200, 2=38400
        struct BaudRate { int rate; uint16_t reg_val; };
        constexpr std::array<BaudRate, 3> baud_steps = {{{9600, 0}, {19200, 1}, {38400, 2}}};

        int current_step = -1;

        auto try_connect = [&](int baud) -> bool {
            if (mb_ctx_) {
                modbus_close(mb_ctx_);
                modbus_free(mb_ctx_);
            }
            mb_ctx_ = modbus_new_rtu(device_name_.c_str(), baud, 'N', 8, 1);
            if (!mb_ctx_) return false;

            modbus_set_response_timeout(mb_ctx_, 0, 200000); // 200ms
            modbus_set_slave(mb_ctx_, slave_address_);

            if (modbus_connect(mb_ctx_) == -1) return false;

            // Use modbus_read_registers (0x03) for Holding Registers
            // Verify with the baud rate register
            std::uint16_t val;
            return modbus_read_registers(mb_ctx_, 0x03E9, 1, &val) != -1;
        };

        // 1. Scan to find current meter baud rate
        for (int i = 0; i < (int)baud_steps.size(); ++i) {
            if (verbose) std::clog << "Scanning at " << baud_steps[i].rate << "..." << std::endl;
            if (try_connect(baud_steps[i].rate)) {
                current_step = i;
                break;
            }
        }

        // 2. Try to escalate baud rate step-by-step
        if (current_step != -1) {
            for (int next_step = current_step + 1; next_step < (int)baud_steps.size(); ++next_step) {
                if (verbose) std::clog << "Attempting upgrade to " << baud_steps[next_step].rate << "..." << std::endl;

                uint16_t val = baud_steps[next_step].reg_val;
                // Force Function 0x10 instead of 0x06
                if (modbus_write_registers(mb_ctx_, 0x03E9, 1, &val) != -1) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    if (try_connect(baud_steps[next_step].rate)) {
                        current_step = next_step;
                        continue; 
                    }
                } else {
                    // Log WHY the write failed
                    if (verbose) std::clog << "Write failed: " << modbus_strerror(errno) << std::endl;
                }

                if (verbose) std::clog << "Upgrade failed, reverting to " << baud_steps[current_step].rate << std::endl;
                try_connect(baud_steps[current_step].rate);
                break;
            }

            // Final configuration for production use
            modbus_set_response_timeout(mb_ctx_, 0, 150000); // 150ms
            if (verbose) std::clog << "Connected at " << baud_steps[current_step].rate << " baud with response timeout of 150ms." << std::endl;
        } else {
            std::cerr << "Error: Could not establish communication with meter at any baud rate." << std::endl;
            cleanup();
        }
    }

    void sync_pll() {
        fabrix::rcu::scoped_access access(clock_tick_area_);
        CEMS::MasterClock::ClockTick const * const tick = flatbuffers::GetRoot<CEMS::MasterClock::ClockTick>(access.get());
        if (tick && tick->timestamp() > last_clock_timestamp_) {
            last_clock_timestamp_ = tick->timestamp();
            pll_.synchronize(last_clock_timestamp_);
        }
    }

    void read_and_publish_energy() {
        if (!mb_ctx_) {
            energy_reading_area_.publish_none();
            return;
        }
        if (-1 == modbus_read_registers(mb_ctx_, IMPORTED_ACTIVE_ENERGY, 12, reinterpret_cast<std::uint16_t *>(&read_data.imported_active_energy))) return;
        read_data.imported_active_energy = swap_lr(read_data.imported_active_energy);
        read_data.exported_active_energy = swap_lr(read_data.exported_active_energy);
        read_data.q1_reactive_energy = swap_lr(read_data.q1_reactive_energy);
        read_data.q2_reactive_energy = swap_lr(read_data.q2_reactive_energy);
        read_data.q3_reactive_energy = swap_lr(read_data.q3_reactive_energy);
        read_data.q4_reactive_energy = swap_lr(read_data.q4_reactive_energy);
        auto const now = std::max(pll_.at_tick().time_since_epoch().count() / 1000000000.0, std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0);
        CEMS::Circutor::EnergyReading msg {
            now,
            static_cast<std::uint8_t>(slave_address_),
            read_data.imported_active_energy,
            read_data.exported_active_energy,
            read_data.q1_reactive_energy,
            read_data.q2_reactive_energy,
            read_data.q3_reactive_energy,
            read_data.q4_reactive_energy,
            0, 0, 0, 0, 0, 0
        };
        if (CEMS::Circutor::Verifier::Check(msg)) {
            auto storage = energy_reading_area_.try_reuse(energy_reading_storage_template_.size(), true);
            if (!storage) storage = energy_reading_storage_template_.clone();
            *flatbuffers::GetMutableRoot<CEMS::Circutor::EnergyReading>(storage.get()) = msg;
            energy_reading_area_.publish_storage(storage);
            broadcast_topic(TOPIC_NAME_ENERGY_READING, &msg, sizeof(msg));
        }
        energy_reading_area_.tick();
        energy_reading_area_.reclaim();
    }

    void read_and_publish_instantaneous() {
        if (!mb_ctx_) {
            instant_reading_area_.publish_none();
            return;
        }
        if (-1 == modbus_read_registers(mb_ctx_, PHASE1_VOLTAGE, 18, reinterpret_cast<std::uint16_t *>(&read_data.phase1_voltage))) {
            if (errno == ETIMEDOUT) {
                // Retry once after a short delay
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (-1 == modbus_read_registers(mb_ctx_, PHASE1_VOLTAGE, 18, reinterpret_cast<std::uint16_t *>(&read_data.phase1_voltage))) {
                    if (verbose) std::cerr << "Error reading registers (1): " << modbus_strerror(errno) << std::endl;
                    return;
                }
            } else {
                if (verbose) std::cerr << "Error reading registers (1): " << modbus_strerror(errno) << std::endl;
                return;
            }
        }
        read_data.phase1_voltage = swap_lr(read_data.phase1_voltage);
        read_data.phase2_voltage = swap_lr(read_data.phase2_voltage);
        read_data.phase3_voltage = swap_lr(read_data.phase3_voltage);
        read_data.phase1_current = swap_lr(read_data.phase1_current);
        read_data.phase2_current = swap_lr(read_data.phase2_current);
        read_data.phase3_current = swap_lr(read_data.phase3_current);
        read_data.phase1_cos_phi = swap_lr(read_data.phase1_cos_phi);
        read_data.phase2_cos_phi = swap_lr(read_data.phase2_cos_phi);
        read_data.phase3_cos_phi = swap_lr(read_data.phase3_cos_phi);

        std::this_thread::sleep_for(std::chrono::milliseconds(15));

        if (-1 == modbus_read_registers(mb_ctx_, PHASE1_ACTIVE_POWER, 24, reinterpret_cast<std::uint16_t *>(&read_data.phase1_active_power))) {
            if (errno == ETIMEDOUT) {
                // Retry once after a short delay
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (-1 == modbus_read_registers(mb_ctx_, PHASE1_ACTIVE_POWER, 24, reinterpret_cast<std::uint16_t *>(&read_data.phase1_active_power))) {
                    if (verbose) std::cerr << "Error reading registers (2): " << modbus_strerror(errno) << std::endl;
                    return;
                }
            } else {
                if (verbose) std::cerr << "Error reading registers (2): " << modbus_strerror(errno) << std::endl;
                return;
            }
        }
        read_data.phase1_active_power = swap_lr(read_data.phase1_active_power);
        read_data.phase2_active_power = swap_lr(read_data.phase2_active_power);
        read_data.phase3_active_power = swap_lr(read_data.phase3_active_power);
        read_data.total_active_power = swap_lr(read_data.total_active_power);
        read_data.phase1_reactive_power = swap_lr(read_data.phase1_reactive_power);
        read_data.phase2_reactive_power = swap_lr(read_data.phase2_reactive_power);
        read_data.phase3_reactive_power = swap_lr(read_data.phase3_reactive_power);
        read_data.total_reactive_power = swap_lr(read_data.total_reactive_power);
        read_data.phase1_apparent_power = swap_lr(read_data.phase1_apparent_power);
        read_data.phase2_apparent_power = swap_lr(read_data.phase2_apparent_power);
        read_data.phase3_apparent_power = swap_lr(read_data.phase3_apparent_power);
        read_data.total_apparent_power = swap_lr(read_data.total_apparent_power);

        auto const now = std::max(pll_.at_tick().time_since_epoch().count() / 1000000000.0, std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0);
        CEMS::Circutor::InstantReading msg {
            now,
            static_cast<std::uint8_t>(slave_address_),
            read_data.phase1_voltage / 10.0f,
            read_data.phase2_voltage / 10.0f,
            read_data.phase3_voltage / 10.0f,
            read_data.phase1_current / 100.0f,
            read_data.phase2_current / 100.0f,
            read_data.phase3_current / 100.0f,
            read_data.phase1_cos_phi / 100.0f,
            read_data.phase2_cos_phi / 100.0f,
            read_data.phase3_cos_phi / 100.0f,
            read_data.phase1_active_power,
            read_data.phase2_active_power,
            read_data.phase3_active_power,
            read_data.total_active_power,
            read_data.phase1_reactive_power,
            read_data.phase2_reactive_power,
            read_data.phase3_reactive_power,
            read_data.total_reactive_power,
            read_data.phase1_apparent_power,
            read_data.phase2_apparent_power,
            read_data.phase3_apparent_power,
            read_data.total_apparent_power
        };
        if (CEMS::Circutor::Verifier::Check(msg)) {
            auto storage = instant_reading_area_.try_reuse(instant_reading_storage_template_.size(), true);
            if (!storage) storage = instant_reading_storage_template_.clone();
            *flatbuffers::GetMutableRoot<CEMS::Circutor::InstantReading>(storage.get()) = msg;
            instant_reading_area_.publish_storage(storage);
            broadcast_topic(TOPIC_NAME_INSTANT_READING, &msg, sizeof(msg));
        }
        instant_reading_area_.tick();
        instant_reading_area_.reclaim();
    }

    void cleanup() {
        if (mb_ctx_) {
            modbus_close(mb_ctx_);
            modbus_free(mb_ctx_);
            mb_ctx_ = nullptr;
        }
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
    param.sched_priority = std::min(std::max(thread_priority, min_priority), max_priority);
    sched_setscheduler(0, SCHED_RR, &param);

    // Execute component
    circutor(&memory_pool, component_name, component_realm, device_name, slave_address, master_clock_name, read_frequency_hz).run();

    // Cleanup
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);

    return exit_code;
}
