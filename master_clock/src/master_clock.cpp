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
#include <schema/master_clock_generated.h>

namespace  {

bool stop = false;
int exit_code = EXIT_SUCCESS;

std::pmr::unsynchronized_pool_resource memory_pool;
std::pmr::string component_name(&memory_pool);
double clock_frequency = 1.0;
double clock_period = 1.0;
int thread_priority = 0;

void print_help() {
    std::cout << "Usage:\n";
    std::cout << "  master_clock [-n|--name <component_name>] [-p|--priority <priority>] [-f|--frequency <frequency>]\n";
    std::cout << "Options:\n";
    std::cout << "  -n, --name <component_name>\n";
    std::cout << "      Specify the component name [mandatory]\n";
    std::cout << "  -p, --priority <priority>\n";
    std::cout << "      Specify the real-time thread priority [optional, default: min+1]\n";
    std::cout << "  -f, --frequency <frequency>\n";
    std::cout << "      Specify the clock frequency in Hz [optional, default: 1.0]\n";
    std::cout << "  -h, --help\n";
    std::cout << "      Display this help and exit\n";
    std::cout.flush();
}

void parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"name", required_argument, nullptr, 'n'},
        {"priority", required_argument, nullptr, 'p'},
        {"frequency", required_argument, nullptr, 'f'},
        {"help", no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "n:p:f:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'n':
                component_name = optarg;
                break;
            case 'p':
                thread_priority = std::stoi(optarg);
                break;
            case 'f': {
                clock_frequency = std::stod(optarg);
                if (clock_frequency <= 0.0) {
                    std::cerr << "Error: Frequency must be positive.\n";
                    exit(1);
                }
                clock_period = 1.0 / clock_frequency;
                break;
            }
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

class master_clock final : public fabrix::component {

    char const * const TOPIC_NAME_CLOCK_TICK = "ClockTick";

public:

    master_clock(std::pmr::memory_resource * const memory_resource, std::string_view name, std::string_view realm = "cems", std::size_t const size = 65536)
    : fabrix::component(memory_resource, name, realm, size)
    {
        // Create area
        if ((clock_tick_area = fabrix::rcu::create_area(*this, TOPIC_NAME_CLOCK_TICK))) {
            // Set grace to 5 seconds; we consider after that after that period data may be reclaimed.
            clock_tick_area.grace_period(5);
            // Create Flatbuffers object template
            clock_tick_storage_template = clock_tick_area.create_storage(sizeof(CEMS::MasterClock::ClockTick), [](flatbuffers::FlatBufferBuilder & builder) {
                builder.Finish(builder.CreateStruct(CEMS::MasterClock::ClockTick()));
            });
        } else {
            std::cerr << "Error: Failed to create RCU area for '" << TOPIC_NAME_CLOCK_TICK << "'.\n";
        }
    }

    ~master_clock() noexcept override {
        clock_tick_area.reset();
    }

    void run(std::chrono::duration<double> period) {
        using namespace std::chrono;
        auto const period_ns = duration_cast<nanoseconds>(period).count();

        // Initial phase alignment
        auto const wall_now = system_clock::now().time_since_epoch();
        auto const initial_wait_ns = period_ns - (duration_cast<nanoseconds>(wall_now).count() % period_ns);

        auto next_steady = steady_clock::now() + nanoseconds(initial_wait_ns);
        // Logical wall time starts at the next grid point
        auto next_logical_wall_ns = ((duration_cast<nanoseconds>(wall_now).count() / period_ns) + 1) * period_ns;

        while (not stop) {
            process_until(next_steady);
            double logical_timestamp = next_logical_wall_ns / 1000000000.0;
            act(logical_timestamp);
            next_steady += nanoseconds(period_ns);
            next_logical_wall_ns += period_ns;
        }
    }

protected:

    void on_start() override {
        std::clog << "Component " << identifier().name()
                  << " is online with pid " << getpid()
                  << " running at priority " << thread_priority
                  << " and frequency " << clock_frequency << " Hz"
                  << "."
                  << std::endl;
    }

    bool on_subscribe_response(endpoint_type sender_endpoint, MAYBE_UNUSED endpoint_type delivery_endpoint, std::string_view topic_name, error_type error_code) override {
        if (error_code == error_type::OK) {
            std::clog << "Request 'subscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << '\'';
            if (!(delivery_endpoint == sender_endpoint)) std::clog << " with delivery point " << delivery_endpoint.identifier().name();
            std::clog << " accepted." << std::endl;
            return true;
        }
        return false;
    }

    bool on_unsubscribe_response(endpoint_type sender_endpoint, MAYBE_UNUSED endpoint_type delivery_endpoint, std::string_view topic_name, MAYBE_UNUSED error_type error_code) override {
        if (error_code == error_type::OK) {
            if (topic_name == TOPIC_NAME_CLOCK_TICK) {
                std::clog << "Request 'unsubscribe' from sender '" << sender_endpoint.identifier().name() << "' on topic '" << topic_name.data() << '\'';
                if (delivery_endpoint) std::clog << " with delivery point " << delivery_endpoint.identifier().name();
                std::clog << " accepted." << std::endl;
                return true;
            }
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
        topics.emplace_back(TOPIC_NAME_CLOCK_TICK);
    }

private:

    void act(double logical_now) {
        // Create clock tick data
        CEMS::MasterClock::ClockTick clock_tick { logical_now, clock_frequency };

        // Update RCU area 'clock_tick'
        {
            auto storage = clock_tick_area.try_reuse(clock_tick_storage_template.size(), true);
            if (not storage) storage = clock_tick_storage_template.clone();
            CEMS::MasterClock::ClockTick * const field = flatbuffers::GetMutableRoot<CEMS::MasterClock::ClockTick>(storage.get());
            *field = clock_tick;
            clock_tick_area.publish_storage(storage);
        }

        // Broadcast topic
        broadcast_topic(TOPIC_NAME_CLOCK_TICK, &clock_tick, sizeof(clock_tick));

        // Tick grace period; decrease counters
        clock_tick_area.tick();

        // Reclaim old unused storage every time; that's ok as the period is very long (1 second)
        clock_tick_area.reclaim();
    }

    fabrix::rcu::writer clock_tick_area;
    fabrix::rcu::storage clock_tick_storage_template;
};

} // local namespace

int main(int argc, char *argv[]) {
    // Handle arguments
    parse_args(argc, argv);

    if (clock_frequency <= 0.0) {
        std::cerr << "Error: Clock frequency must be positive. Use -h for help.\n";
        exit(EXIT_FAILURE);
    }

    // Random seed
    std::srand(std::time(nullptr));

    // Register 'break' handler
    std::signal(SIGINT, [](int) { stop = true; });
    std::signal(SIGTERM, [](int) { stop = true; });

    // Use real-time scheduling policy.
    int const min_priority = sched_get_priority_min(SCHED_RR);
    int const max_priority = sched_get_priority_max(SCHED_RR);
    thread_priority = std::min(std::max(thread_priority, min_priority), max_priority);
    struct sched_param param;
    param.sched_priority = thread_priority;
    sched_setscheduler(0, SCHED_RR, &param);

    // Execute component
    master_clock(&memory_pool, component_name, "cems", 65536).run(std::chrono::duration<double>(clock_period));

    // Cleanup
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);

    return exit_code;
}
