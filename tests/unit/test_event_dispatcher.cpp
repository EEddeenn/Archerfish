#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <mutex>

#include "archerfish/runtime/event_dispatcher.hpp"

using namespace archerfish::runtime;

TEST_CASE("EventDispatcher single event fires after delay", "[runtime][event]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};

    dispatcher.schedule(0.02, [&] { counter.fetch_add(1); });
    REQUIRE(dispatcher.total_events() == 1);

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(counter.load() == 1);
    REQUIRE(dispatcher.dispatched_count() == 1);
}

TEST_CASE("EventDispatcher multiple events fire in order", "[runtime][event]") {
    EventDispatcher dispatcher;
    std::vector<int> order;
    std::mutex mtx;

    dispatcher.schedule(0.03, [&] {
        std::lock_guard lock(mtx);
        order.push_back(2);
    });
    dispatcher.schedule(0.01, [&] {
        std::lock_guard lock(mtx);
        order.push_back(1);
    });
    dispatcher.schedule(0.05, [&] {
        std::lock_guard lock(mtx);
        order.push_back(3);
    });

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
}

TEST_CASE("EventDispatcher events at time 0 fire immediately", "[runtime][event]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};

    dispatcher.schedule(0.0, [&] { counter.fetch_add(1); });
    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(counter.load() == 1);
    REQUIRE(dispatcher.dispatched_count() == 1);
}

TEST_CASE("EventDispatcher dispatched_count tracks correctly", "[runtime][event]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};

    for (int i = 0; i < 5; ++i) {
        dispatcher.schedule(0.0, [&] { counter.fetch_add(1); });
    }

    REQUIRE(dispatcher.total_events() == 5);

    dispatcher.start();
    dispatcher.wait_complete();

    REQUIRE(dispatcher.dispatched_count() == 5);
    REQUIRE(counter.load() == 5);
}

TEST_CASE("EventDispatcher cancel prevents further dispatching", "[runtime][event]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};

    dispatcher.schedule(0.0, [&] { counter.fetch_add(1); });
    dispatcher.schedule(0.5, [&] { counter.fetch_add(10); });

    dispatcher.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    dispatcher.cancel();

    REQUIRE(counter.load() == 1);
    REQUIRE(dispatcher.dispatched_count() == 1);
}
