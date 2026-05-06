#include <catch2/catch_test_macros.hpp>

#include "archerfish/runtime/spsc_queue.hpp"

#include <limits>
#include <stdexcept>

using namespace archerfish::runtime;

TEST_CASE("SpscQueue push and pop single item", "[runtime][spsc]") {
    SpscQueue<int> q(4);
    REQUIRE(q.push(42));
    auto val = q.pop();
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 42);
}

TEST_CASE("SpscQueue FIFO order preserved", "[runtime][spsc]") {
    SpscQueue<int> q(8);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    REQUIRE(q.push(3));
    REQUIRE(q.pop().value() == 1);
    REQUIRE(q.pop().value() == 2);
    REQUIRE(q.pop().value() == 3);
}

TEST_CASE("SpscQueue full queue returns false on push", "[runtime][spsc]") {
    SpscQueue<int> q(2);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    REQUIRE_FALSE(q.push(3));
    REQUIRE(q.full());
}

TEST_CASE("SpscQueue empty queue returns nullopt on pop", "[runtime][spsc]") {
    SpscQueue<int> q(4);
    REQUIRE(q.empty());
    REQUIRE_FALSE(q.pop().has_value());
}

TEST_CASE("SpscQueue capacity is respected", "[runtime][spsc]") {
    SpscQueue<int> q(4);
    REQUIRE(q.capacity() == 4);
}

TEST_CASE("SpscQueue rejects invalid capacities", "[runtime][spsc]") {
    CHECK_THROWS_AS(SpscQueue<int>(0), std::invalid_argument);
    CHECK_THROWS_AS(SpscQueue<int>(std::numeric_limits<size_t>::max()), std::overflow_error);
}

TEST_CASE("SpscQueue multiple push/pop cycles work correctly", "[runtime][spsc]") {
    SpscQueue<int> q(4);
    for (int cycle = 0; cycle < 10; ++cycle) {
        REQUIRE(q.push(cycle * 2));
        REQUIRE(q.push(cycle * 2 + 1));
        auto v1 = q.pop();
        auto v2 = q.pop();
        REQUIRE(v1.has_value());
        REQUIRE(v2.has_value());
        REQUIRE(v1.value() == cycle * 2);
        REQUIRE(v2.value() == cycle * 2 + 1);
    }
}

TEST_CASE("SpscQueue size reports correctly", "[runtime][spsc]") {
    SpscQueue<int> q(8);
    REQUIRE(q.size() == 0);
    REQUIRE(q.push(1));
    REQUIRE(q.size() == 1);
    REQUIRE(q.push(2));
    REQUIRE(q.size() == 2);
    REQUIRE(q.pop().has_value());
    REQUIRE(q.size() == 1);
    REQUIRE(q.pop().has_value());
    REQUIRE(q.size() == 0);
}

TEST_CASE("SampleBlock default construction", "[runtime][spsc]") {
    SampleBlock block;
    REQUIRE(block.samples.empty());
    REQUIRE_FALSE(block.start_of_burst);
    REQUIRE_FALSE(block.end_of_burst);
}
