#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <chrono>
#include <vector>

#include "archerfish/runtime/event_dispatcher.hpp"

using namespace archerfish::runtime;
using Catch::Matchers::WithinAbs;

TEST_CASE("EventDispatcher timing accuracy within tolerance", "[runtime][event_timing]") {
    EventDispatcher dispatcher;
    std::atomic<double> fire_time{0.0};

    dispatcher.schedule(0.05, [&] {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        fire_time.store(std::chrono::duration<double>(now).count());
    });

    auto start = std::chrono::steady_clock::now().time_since_epoch();
    double start_d = std::chrono::duration<double>(start).count();

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(fire_time.load() > 0.0);
    double elapsed = fire_time.load() - start_d;
    CHECK_THAT(elapsed, WithinAbs(0.05, 0.03));
}

TEST_CASE("EventDispatcher preserves event order", "[runtime][event_timing]") {
    EventDispatcher dispatcher;
    std::vector<double> times;
    std::mutex mtx;

    for (int i = 0; i < 5; ++i) {
        double t = 0.01 * (i + 1);
        dispatcher.schedule(t, [&mtx, &times, t] {
            std::lock_guard lock(mtx);
            times.push_back(t);
        });
    }

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(times.size() == 5);
    for (size_t i = 0; i < times.size(); ++i) {
        CHECK(times[i] == 0.01 * (i + 1));
    }
}

TEST_CASE("EventDispatcher zero-delay events dispatch promptly", "[runtime][event_timing]") {
    EventDispatcher dispatcher;
    std::atomic<bool> fired{false};

    dispatcher.schedule(0.0, [&] { fired.store(true); });
    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(fired.load());
    REQUIRE(dispatcher.dispatched_count() == 1);
}

TEST_CASE("EventDispatcher many events all dispatch", "[runtime][event_timing]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};
    const int N = 20;

    for (int i = 0; i < N; ++i) {
        dispatcher.schedule(0.0, [&] { counter.fetch_add(1); });
    }

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(counter.load() == N);
    REQUIRE(dispatcher.dispatched_count() == static_cast<size_t>(N));
}
