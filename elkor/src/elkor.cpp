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
#include <util/logger.hpp>
#include <fabrix/fabrix.hpp>
#include <elkor_modbus.hpp>
#include <elkor_check.hpp>
#include <phase_lock_loop/phase_lock_loop.hpp>
#include <schema/elkor_generated.h>
#include <schema/master_clock_generated.h>

namespace  {

bool stop = false;
int exit_code = EXIT_SUCCESS;

std::pmr::unsynchronized_pool_resource memory_pool;
std::pmr::string component_name(&memory_pool);
std::pmr::string component_realm(&memory_pool);
std::pmr::string device_name("/dev/elkor", &memory_pool);
std::pmr::string master_clock_name(&memory_pool);
double read_frequency_hz = 1.0;
std::uint8_t scan_start_address = 3;
std::uint8_t scan_end_address = 3;
int thread_priority = 10;
bool verbose = false;

void print_help() {
    std::cout << "Usage:\n";
                 "  elkor -n|--name <component_name> [-d|--device <device>] [-s|--slave <start_address>:<end_address>] [-c|--clock <component_name>] [-v|--verbose]\n"
                 "Options:\n"
                 "  -n, --name <component_name>\n"
                 "      Specify the component name [mandatory]\n"
                 "  -r, --realm <component_realm>\n"
                 "      Specify the component realm [mandatory]\n"
                 "  -d, --device <device>\n"
                 "      Specify the device name [default: /dev/elkor]\n"
                 "  -s, --slave <start_address>:<end_address>\n"
                 "      Provide start and end range of slave addresses to scan for Elkor device [default: 3:3].\n"
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
        {"device", optional_argument, nullptr, 'd'},
        {"slave", optional_argument, nullptr, 's'},
        {"clock", required_argument, nullptr, 'c'},
        {"frequency", required_argument, nullptr, 'f'},
        {"priority", required_argument, nullptr, 'p'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "n:r:d:s:c:f:p:vh", long_options, nullptr)) != -1) {
        char * token;
        int address;
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
                token = std::strtok(const_cast<char*>(optarg), ":");
                if (token) {
                    address = std::atoi(token);
                    if (address < 0 or address > 255) {
                        std::cerr << "Error: Slave number must be between 0 and 255.\n";
                        exit(EXIT_FAILURE);
                    }
                    scan_start_address = address;
                    token = std::strtok(nullptr, ":");
                    if (token) {
                        address = std::atoi(token);
                        if (address < 0 or address > 255) {
                            std::cerr << "Error: Slave number must be between 0 and 255.\n";
                            exit(EXIT_FAILURE);
                        }
                        scan_end_address = address;
                    } else {
                        std::cerr << "Invalid format of scan address range\n";
                        exit(EXIT_FAILURE);
                    }
                    if (scan_start_address > scan_end_address) {
                        std::cerr << "Error: Scan slave start address must lower or equal then end address.\n";
                        exit(EXIT_FAILURE);
                    }
                } else {
                    std::cerr << "Invalid format of scan address range\n";
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
                exit(1);
        }
    }

    if (component_name.empty()) {
        std::cerr << "Error: Component name is mandatory. Use -h for help.\n";
        exit(EXIT_FAILURE);
    }
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::Elkor::InstantReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"                        ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "frequency"                        ANSI_TOK ":" ANSI_NRM << o.frequency()
        << ANSI_TOK "," ANSI_LBL "total_real_power"                 ANSI_TOK ":" ANSI_NRM << o.total_real_power()
        << ANSI_TOK "," ANSI_LBL "total_system_power_factor"        ANSI_TOK ":" ANSI_NRM << o.total_system_power_factor()
        << ANSI_TOK "," ANSI_LBL "total_average_current"            ANSI_TOK ":" ANSI_NRM << o.total_average_current()
        << ANSI_TOK "," ANSI_LBL "total_current"                    ANSI_TOK ":" ANSI_NRM << o.total_current()
        << ANSI_TOK "," ANSI_LBL "total_average_voltage_to_neutral" ANSI_TOK ":" ANSI_NRM << o.total_average_voltage_to_neutral()
        << ANSI_TOK "," ANSI_LBL "total_average_voltage_to_line"    ANSI_TOK ":" ANSI_NRM << o.total_average_voltage_to_line()
        << ANSI_TOK "," ANSI_LBL "total_reactive_power"             ANSI_TOK ":" ANSI_NRM << o.total_reactive_power()
        << ANSI_TOK "," ANSI_LBL "total_apparent_power"             ANSI_TOK ":" ANSI_NRM << o.total_apparent_power()

        << ANSI_TOK "," ANSI_LBL "phase_a_real_power"               ANSI_TOK ":" ANSI_NRM << o.phase_a_real_power()
        << ANSI_TOK "," ANSI_LBL "phase_a_current"                  ANSI_TOK ":" ANSI_NRM << o.phase_a_current()
        << ANSI_TOK "," ANSI_LBL "phase_a_voltage_to_neutral"       ANSI_TOK ":" ANSI_NRM << o.phase_a_voltage_to_neutral()
        << ANSI_TOK "," ANSI_LBL "phase_a_voltage_to_b"             ANSI_TOK ":" ANSI_NRM << o.phase_a_voltage_to_b()
        << ANSI_TOK "," ANSI_LBL "phase_a_reactive_power"           ANSI_TOK ":" ANSI_NRM << o.phase_a_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase_a_apparent_power"           ANSI_TOK ":" ANSI_NRM << o.phase_a_apparent_power()
        << ANSI_TOK "," ANSI_LBL "phase_a_power_factor"             ANSI_TOK ":" ANSI_NRM << o.phase_a_power_factor()

        << ANSI_TOK "," ANSI_LBL "phase_b_real_power"               ANSI_TOK ":" ANSI_NRM << o.phase_b_real_power()
        << ANSI_TOK "," ANSI_LBL "phase_b_current"                  ANSI_TOK ":" ANSI_NRM << o.phase_b_current()
        << ANSI_TOK "," ANSI_LBL "phase_b_voltage_to_neutral"       ANSI_TOK ":" ANSI_NRM << o.phase_b_voltage_to_neutral()
        << ANSI_TOK "," ANSI_LBL "phase_b_voltage_to_c"             ANSI_TOK ":" ANSI_NRM << o.phase_b_voltage_to_c()
        << ANSI_TOK "," ANSI_LBL "phase_b_reactive_power"           ANSI_TOK ":" ANSI_NRM << o.phase_b_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase_b_apparent_power"           ANSI_TOK ":" ANSI_NRM << o.phase_b_apparent_power()
        << ANSI_TOK "," ANSI_LBL "phase_b_power_factor"             ANSI_TOK ":" ANSI_NRM << o.phase_b_power_factor()

        << ANSI_TOK "," ANSI_LBL "phase_c_real_power"               ANSI_TOK ":" ANSI_NRM << o.phase_c_real_power()
        << ANSI_TOK "," ANSI_LBL "phase_c_current"                  ANSI_TOK ":" ANSI_NRM << o.phase_c_current()
        << ANSI_TOK "," ANSI_LBL "phase_c_voltage_to_neutral"       ANSI_TOK ":" ANSI_NRM << o.phase_c_voltage_to_neutral()
        << ANSI_TOK "," ANSI_LBL "phase_c_voltage_to_a"             ANSI_TOK ":" ANSI_NRM << o.phase_c_voltage_to_a()
        << ANSI_TOK "," ANSI_LBL "phase_c_reactive_power"           ANSI_TOK ":" ANSI_NRM << o.phase_c_reactive_power()
        << ANSI_TOK "," ANSI_LBL "phase_c_apparent_power"           ANSI_TOK ":" ANSI_NRM << o.phase_c_apparent_power()
        << ANSI_TOK "," ANSI_LBL "phase_c_power_factor"             ANSI_TOK ":" ANSI_NRM << o.phase_c_power_factor()
        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

MAYBE_UNUSED std::ostream & operator<<(std::ostream & out, CEMS::Elkor::EnergyReading const & o) {
    std::time_t t = o.timestamp();
    std::tm tm = *std::localtime(&t);
    out << ANSI_TOK "{"
        <<              ANSI_LBL "timestamp"                    ANSI_TOK ":" ANSI_NRM "\"" << std::put_time(&tm, "%c %Z") << "\""
        << ANSI_TOK "," ANSI_LBL "total_net_energy"             ANSI_TOK ":" ANSI_NRM << o.total_net_energy()
        << ANSI_TOK "," ANSI_LBL "total_import_energy"          ANSI_TOK ":" ANSI_NRM << o.total_import_energy()
        << ANSI_TOK "," ANSI_LBL "total_export_energy"          ANSI_TOK ":" ANSI_NRM << o.total_export_energy()
        << ANSI_TOK "," ANSI_LBL "total_inductive_energy"       ANSI_TOK ":" ANSI_NRM << o.total_inductive_energy()
        << ANSI_TOK "," ANSI_LBL "total_capacitive_energy"      ANSI_TOK ":" ANSI_NRM << o.total_capacitive_energy()
        << ANSI_TOK "," ANSI_LBL "total_net_reactive_energy"    ANSI_TOK ":" ANSI_NRM << o.total_net_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "total_apparent_energy"        ANSI_TOK ":" ANSI_NRM << o.total_apparent_energy()

        << ANSI_TOK "," ANSI_LBL "phase_a_net_energy"           ANSI_TOK ":" ANSI_NRM << o.phase_a_net_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_import_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_a_import_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_export_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_a_export_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_inductive_energy"     ANSI_TOK ":" ANSI_NRM << o.phase_a_inductive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_capacitive_energy"    ANSI_TOK ":" ANSI_NRM << o.phase_a_capacitive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_net_reactive_energy"  ANSI_TOK ":" ANSI_NRM << o.phase_a_net_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_a_apparent_energy"      ANSI_TOK ":" ANSI_NRM << o.phase_a_apparent_energy()

        << ANSI_TOK "," ANSI_LBL "phase_b_net_energy"           ANSI_TOK ":" ANSI_NRM << o.phase_b_net_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_import_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_b_import_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_export_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_b_export_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_inductive_energy"     ANSI_TOK ":" ANSI_NRM << o.phase_b_inductive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_capacitive_energy"    ANSI_TOK ":" ANSI_NRM << o.phase_b_capacitive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_net_reactive_energy"  ANSI_TOK ":" ANSI_NRM << o.phase_b_net_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_b_apparent_energy"      ANSI_TOK ":" ANSI_NRM << o.phase_b_apparent_energy()

        << ANSI_TOK "," ANSI_LBL "phase_c_net_energy"           ANSI_TOK ":" ANSI_NRM << o.phase_c_net_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_import_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_c_import_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_export_energy"        ANSI_TOK ":" ANSI_NRM << o.phase_c_export_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_inductive_energy"     ANSI_TOK ":" ANSI_NRM << o.phase_c_inductive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_capacitive_energy"    ANSI_TOK ":" ANSI_NRM << o.phase_c_capacitive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_net_reactive_energy"  ANSI_TOK ":" ANSI_NRM << o.phase_c_net_reactive_energy()
        << ANSI_TOK "," ANSI_LBL "phase_c_apparent_energy"      ANSI_TOK ":" ANSI_NRM << o.phase_c_apparent_energy()

        << ANSI_TOK "}" ANSI_NRM;
    return out;
}

class elkor final : public fabrix::component {
private:

    char const * const TOPIC_NAME_INSTANT_READING = "InstantReading";
    char const * const TOPIC_NAME_ENERGY_READING = "EnergyReading";
    char const * const TOPIC_NAME_CLOCK_TICK = "ClockTick";

public:

    elkor(std::pmr::memory_resource * const memory_resource,
          std::string_view name,
          std::string_view realm,
          std::uint8_t scan_start_address,
          std::uint8_t scan_end_address,
          std::string_view master_clock_name = "",
          double nominal_frequency_hz = 1.0,
          std::size_t const size = 65536)
    : fabrix::component(memory_resource, name, realm, size)
    , pll_(nominal_frequency_hz)
    , device_name_(memory_resource)
    , master_clock_name_(master_clock_name)
    {
        device_name_ = device_name.data();
        if (verbose) std::clog << "Initializing modbus connection " << device_name_ << std::endl;
        device_count_ = elkor_interface_.ScanRTUDevices(device_name_.c_str(), scan_start_address, scan_end_address);
        std::clog << "Device count: " << device_count_ << std::endl;
    }

    ~elkor() noexcept override {
        cleanup();
    }

    void run() {
        try {
            do {
                process_until(pll_.at_tick());
                read_and_publish_instantaneous();
                read_and_publish_energy();
                pll_.advance();
                sync_pll();
                process_until(pll_.at_tick());
                read_and_publish_instantaneous();
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
            instant_reading_storage_template_ = instant_reading_area_.create_storage(sizeof(CEMS::Elkor::InstantReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::Elkor::InstantReading()));
            });
        } else {
            std::cerr << "Error: Failed to create RCU area for '" << TOPIC_NAME_INSTANT_READING << "'.\n";
        }

        // Create area
        if ((energy_reading_area_ = fabrix::rcu::create_area(*this, TOPIC_NAME_ENERGY_READING))) {
            // Set grace to 5 seconds; we consider after that after that period data may be reclaimed.
            energy_reading_area_.grace_period(5);
            // Create Flatbuffers object template
            energy_reading_storage_template_ = energy_reading_area_.create_storage(sizeof(CEMS::Elkor::EnergyReading), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::Elkor::EnergyReading()));
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

    void on_error(endpoint_type endpoint, error_type error_code) override {
        std::cerr << "Error: '" << (endpoint ? endpoint.identifier().name() : "<>") << "' with error code " << error_code << '\n';
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

    void sync_pll() {
        fabrix::rcu::scoped_access access(clock_tick_area_);
        CEMS::MasterClock::ClockTick const * const tick = flatbuffers::GetRoot<CEMS::MasterClock::ClockTick>(access.get());
        if (tick && tick->timestamp() > last_clock_timestamp_) {
            last_clock_timestamp_ = tick->timestamp();
            pll_.synchronize(last_clock_timestamp_);
        }
    }

    void read_and_publish_energy() {
        if (device_count_ == 0) return;
        auto itt = elkor_interface_.GetFirstDevice();
        while (itt) {
            if (itt->ReadWattsOnFloatEnergy()) {
                auto & read_data = itt->wattson_float;
                auto const now = std::max(pll_.at_tick().time_since_epoch().count() / 1000000000.0, std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0);
                CEMS::Elkor::EnergyReading energy_reading {
                    now,
                    read_data.NetTotalEnergy(),
                    read_data.TotalImportEnergy(),
                    read_data.TotalExportEnergy(),
                    read_data.TotalInductiveEnergy(),
                    read_data.TotalCapacitiveEnergy(),
                    read_data.NetTotalReactiveEnergy(),
                    read_data.TotalApparentEnergy(),
                    read_data.NetEnergyPhaseA(),
                    read_data.ImportEnergyPhaseA(),
                    read_data.ExportEnergyPhaseA(),
                    read_data.InductiveEnergyPhaseA(),
                    read_data.CapacitiveEnergyPhaseA(),
                    read_data.NetReactiveEnergyPhaseA(),
                    read_data.ApparentEnergyPhaseA(),
                    read_data.NetEnergyPhaseB(),
                    read_data.ImportEnergyPhaseB(),
                    read_data.ExportEnergyPhaseB(),
                    read_data.InductiveEnergyPhaseB(),
                    read_data.CapacitiveEnergyPhaseB(),
                    read_data.NetReactiveEnergyPhaseB(),
                    read_data.ApparentEnergyPhaseB(),
                    read_data.NetEnergyPhaseC(),
                    read_data.ImportEnergyPhaseC(),
                    read_data.ExportEnergyPhaseC(),
                    read_data.InductiveEnergyPhaseC(),
                    read_data.CapacitiveEnergyPhaseC(),
                    read_data.NetReactiveEnergyPhaseC(),
                    read_data.ApparentEnergyPhaseC()
                };
                if (CEMS::Elkor::Verifier::Check(energy_reading)) {
                    auto storage = energy_reading_area_.try_reuse(energy_reading_storage_template_.size(), true);
                    if (!storage) storage = energy_reading_storage_template_.clone();
                    *flatbuffers::GetMutableRoot<CEMS::Elkor::EnergyReading>(storage.get()) = energy_reading;
                    energy_reading_area_.publish_storage(storage);
                    broadcast_topic(TOPIC_NAME_ENERGY_READING, &energy_reading, sizeof(energy_reading));
                }
                energy_reading_area_.tick();
                energy_reading_area_.reclaim();
            }
            itt = elkor_interface_.GetNextDevice(itt);
        }
    }

    void read_and_publish_instantaneous() {
        if (device_count_ == 0) return;
        auto itt = elkor_interface_.GetFirstDevice();
        while (itt) {
            if (itt->ReadWattsOnFloatInstant()) {
                auto & read_data = itt->wattson_float;
                double const now = std::max(pll_.at_tick().time_since_epoch().count() / 1000000000.0, std::chrono::system_clock::now().time_since_epoch().count() / 1000000000.0);
                CEMS::Elkor::InstantReading msg {
                    now,
                    read_data.Frequency(),
                    read_data.TotalRealPower(),
                    read_data.TotalSystemPowerFactor(),
                    read_data.AverageCurrent(),
                    read_data.CurrentPhaseA() + read_data.CurrentPhaseB() + read_data.CurrentPhaseC(),
                    read_data.AverageVoltageLineNeutral(),
                    read_data.AverageVoltageLineLine(),
                    read_data.TotalReactivePower(),
                    read_data.TotalApparentPower(),
                    read_data.RealPowerPhaseA(),
                    read_data.CurrentPhaseA(),
                    read_data.VoltagePhaseA2N(),
                    read_data.VoltagePhaseA2B(),
                    read_data.ReactivePowerPhaseA(),
                    read_data.ApparentPowerPhaseA(),
                    read_data.PowerFactorPhaseA(),
                    read_data.RealPowerPhaseB(),
                    read_data.CurrentPhaseB(),
                    read_data.VoltagePhaseB2N(),
                    read_data.VoltagePhaseB2C(),
                    read_data.ReactivePowerPhaseB(),
                    read_data.ApparentPowerPhaseB(),
                    read_data.PowerFactorPhaseB(),
                    read_data.RealPowerPhaseC(),
                    read_data.CurrentPhaseC(),
                    read_data.VoltagePhaseC2N(),
                    read_data.VoltagePhaseA2C(),
                    read_data.ReactivePowerPhaseC(),
                    read_data.ApparentPowerPhaseC(),
                    read_data.PowerFactorPhaseC()
                };
                if (CEMS::Elkor::Verifier::Check(msg)) {
                    auto storage = instant_reading_area_.try_reuse(instant_reading_storage_template_.size(), true);
                    if (!storage) storage = instant_reading_storage_template_.clone();
                    *flatbuffers::GetMutableRoot<CEMS::Elkor::InstantReading>(storage.get()) = msg;
                    instant_reading_area_.publish_storage(storage);
                    broadcast_topic(TOPIC_NAME_INSTANT_READING, &msg, sizeof(msg));
                }
                instant_reading_area_.tick();
                instant_reading_area_.reclaim();
            }
            itt = elkor_interface_.GetNextDevice(itt);
        }
    }

    void cleanup() {
        elkor_interface_.ClearDeviceList();
    }

    phase_lock_loop pll_;
    std::pmr::string device_name_;
    std::pmr::string master_clock_name_;
    fabrix::rcu::reader clock_tick_area_;
    fabrix::rcu::writer instant_reading_area_;
    fabrix::rcu::writer energy_reading_area_;
    fabrix::rcu::storage instant_reading_storage_template_;
    fabrix::rcu::storage energy_reading_storage_template_;
    ElkorModbusInterface elkor_interface_;
    double last_clock_timestamp_ = 0.0;
    unsigned int device_count_ = 0;
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

    // Use polymorphic memory system for memory management
    std::pmr::set_default_resource(&memory_pool);

    // Execute component
    elkor(&memory_pool, component_name, component_realm, scan_start_address, scan_end_address, master_clock_name, read_frequency_hz).run();

    // Cleanup
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);

    return exit_code;
}
