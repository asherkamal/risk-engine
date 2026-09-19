#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/graph/property_graph.hpp"

using engine::graph::PropertyGraph;
using engine::spatial::Point2D;

namespace {

double Distance(Point2D a, Point2D b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

}  // namespace

TEST_CASE("PropertyGraph links exactly the points within radius (no cap)", "[graph]") {
    // A simple grid: 0,0 / 10,0 / 20,0 / 0,10 -- radius 15 should link
    // (0,0)-(10,0) [10], (10,0)-(20,0) [10], (0,0)-(0,10) [10], (10,0)-(0,10)
    // [~14.14], but not (0,0)-(20,0) [20] or (20,0)-(0,10) [~22.36].
    std::vector<Point2D> points = {{0, 0}, {10, 0}, {20, 0}, {0, 10}};
    PropertyGraph graph(points, /*link_radius=*/15.0, /*max_neighbors=*/10);

    REQUIRE(graph.NodeCount() == 4);

    auto Has = [&](std::uint32_t node, std::uint32_t neighbor) {
        auto ids = graph.NeighborIds(node);
        return std::find(ids.begin(), ids.end(), neighbor) != ids.end();
    };

    REQUIRE(Has(0, 1));
    REQUIRE(Has(0, 3));
    REQUIRE_FALSE(Has(0, 2));  // distance 20 > radius 15
    REQUIRE(Has(1, 0));
    REQUIRE(Has(1, 2));
    REQUIRE(Has(1, 3));
    REQUIRE_FALSE(Has(2, 3));  // distance ~22.36 > radius 15
}

TEST_CASE("PropertyGraph has no self-loops", "[graph]") {
    std::vector<Point2D> points = {{0, 0}, {1, 1}, {2, 2}, {5, 5}};
    PropertyGraph graph(points, /*link_radius=*/100.0);

    for (std::uint32_t i = 0; i < graph.NodeCount(); ++i) {
        auto ids = graph.NeighborIds(i);
        REQUIRE(std::find(ids.begin(), ids.end(), i) == ids.end());
    }
}

TEST_CASE("PropertyGraph caps out-degree to nearest max_neighbors", "[graph]") {
    // A dense cluster of 20 points all within 1.0 of the origin plus the
    // origin itself; capped to 5 neighbors, the origin should keep only the
    // 5 closest.
    std::vector<Point2D> points = {{0, 0}};
    for (int i = 1; i <= 20; ++i) {
        points.push_back(Point2D{static_cast<double>(i) * 0.01, 0.0});
    }
    PropertyGraph graph(points, /*link_radius=*/100.0, /*max_neighbors=*/5);

    auto ids = graph.NeighborIds(0);
    REQUIRE(ids.size() == 5);
    // The 5 closest to the origin are points 1..5 (distances 0.01..0.05).
    for (std::uint32_t id : ids) {
        REQUIRE(id >= 1);
        REQUIRE(id <= 5);
    }
}

TEST_CASE("PropertyGraph neighbor distances match actual Euclidean distance", "[graph]") {
    std::vector<Point2D> points = {{0, 0}, {3, 4}, {6, 8}};
    PropertyGraph graph(points, /*link_radius=*/100.0);

    auto ids = graph.NeighborIds(0);
    auto dists = graph.NeighborDistances(0);
    REQUIRE(ids.size() == dists.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        REQUIRE(dists[i] == Catch::Approx(Distance(points[0], points[ids[i]])));
    }
}

TEST_CASE("PropertyGraph::NearestNode finds the closest node", "[graph]") {
    std::vector<Point2D> points = {{0, 0}, {10, 0}, {20, 0}, {30, 0}};
    PropertyGraph graph(points, /*link_radius=*/5.0);  // radius doesn't affect NearestNode

    REQUIRE(graph.NearestNode(Point2D{1, 0}) == 0);
    REQUIRE(graph.NearestNode(Point2D{9, 0}) == 1);
    REQUIRE(graph.NearestNode(Point2D{22, 1}) == 2);
    REQUIRE(graph.NearestNode(Point2D{1000, 1000}) == 3);
}
