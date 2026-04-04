#include <atomic>
#include <chrono>
#include <stop_token>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "archerfish/runtime/spsc_queue.hpp"

using namespace archerfish::runtime;

TEST_CASE("Producer-consumer: 1000 items via push_notify/pop_wait", "[runtime][spsc][concurrent]") {
    SpscQueue<int> q(64);
    constexpr int N = 1000;
    std::vector<int> consumed(N, -1);

    std::stop_source ss;

    std::thread producer([&] {
        auto token = ss.get_token();
        for (int i = 0; i < N; ++i) {
            REQUIRE(q.push_wait(std::move(i), token));
        }
    });

    std::thread consumer([&] {
        auto token = ss.get_token();
        for (int i = 0; i < N; ++i) {
            auto val = q.pop_wait(token);
            REQUIRE(val.has_value());
            consumed[val.value()] = val.value();
        }
    });

    producer.join();
    consumer.join();

    for (int i = 0; i < N; ++i) {
        REQUIRE(consumed[i] == i);
    }
}

TEST_CASE("Stop token causes clean exit from pop_wait within 50ms", "[runtime][spsc][concurrent]") {
    SpscQueue<int> q(16);
    std::stop_source ss;

    std::atomic<bool> exited{false};

    std::thread consumer([&] {
        auto val = q.pop_wait(ss.get_token());
        exited.store(!val.has_value());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE_FALSE(exited.load());

    ss.request_stop();
    q.notify_all();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (!exited.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    consumer.join();
    REQUIRE(exited.load());
}

TEST_CASE("Backpressure: push_wait blocks when full, unblocks on pop", "[runtime][spsc][concurrent]") {
    SpscQueue<int> q(4);
    std::stop_source ss;
    auto token = ss.get_token();

    std::atomic<int> pushed{0};
    std::atomic<bool> producer_blocked{false};

    std::thread producer([&] {
        for (int i = 0; i < 20; ++i) {
            if (q.full()) producer_blocked.store(true);
            REQUIRE(q.push_wait(std::move(i), token));
            pushed.fetch_add(1);
        }
    });

    std::vector<int> consumed;
    consumed.reserve(20);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(producer_blocked.load());

    auto consume_token = ss.get_token();
    while (consumed.size() < 20u) {
        auto val = q.pop_wait(consume_token);
        REQUIRE(val.has_value());
        consumed.push_back(val.value());
    }

    producer.join();

    REQUIRE(pushed.load() == 20);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(consumed[static_cast<size_t>(i)] == i);
    }
}

TEST_CASE("push_wait returns false on stop requested", "[runtime][spsc][concurrent]") {
    SpscQueue<int> q(2);
    std::stop_source ss;

    REQUIRE(q.push_notify(1));
    REQUIRE(q.push_notify(2));
    REQUIRE(q.full());

    std::atomic<bool> result{true};

    std::thread producer([&] {
        int val = 99;
        result.store(q.push_wait(std::move(val), ss.get_token()));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ss.request_stop();
    q.notify_all();
    producer.join();

    REQUIRE_FALSE(result.load());
}

TEST_CASE("notify_all wakes multiple consumers", "[runtime][spsc][concurrent]") {
    SpscQueue<int> q(16);
    std::stop_source ss;
    auto token = ss.get_token();

    std::atomic<int> wake_count{0};

    auto consumer_fn = [&] {
        auto val = q.pop_wait(token);
        if (!val.has_value()) wake_count.fetch_add(1);
    };

    std::thread c1(consumer_fn);
    std::thread c2(consumer_fn);
    std::thread c3(consumer_fn);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ss.request_stop();
    q.notify_all();

    c1.join();
    c2.join();
    c3.join();

    REQUIRE(wake_count.load() == 3);
}
