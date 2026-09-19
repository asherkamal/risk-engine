#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/graph/property_graph.hpp"
#include "engine/sim/monte_carlo.hpp"

using engine::graph::PropertyGraph;
using engine::sim::AggregateStats;
using engine::sim::MonteCarloConfig;
using engine::sim::PropagateDamage;
using engine::sim::RunMonteCarlo;
using engine::spatial::Point2D;

namespace {

PropertyGraph BuildChainGraph(int n, double spacing = 10.0) {
    std::vector<Point2D> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) {
        points.push_back(Point2D{static_cast<double>(i) * spacing, 0.0});
    }
    // Radius just over one spacing so each node links only to its immediate
    // neighbors, forming a simple chain -- cumulative distance from node 0
    // to node k is exactly k * spacing.
    return PropertyGraph(points, spacing * 1.5);
}

std::vector<double> UniformInsuredValues(std::size_t n, double value = 1.0) {
    return std::vector<double>(n, value);
}

}  // namespace

TEST_CASE("PropagateDamage decays monotonically with distance and prunes", "[monte_carlo]") {
    PropertyGraph graph = BuildChainGraph(10, 10.0);
    std::vector<std::uint64_t> last_seen(graph.NodeCount(), 0);

    struct Visit {
        std::uint32_t node;
        double distance;
        double probability;
    };
    std::vector<Visit> visits;

    const double intensity = 1.0;
    const double decay_rate = 0.1;
    const double threshold = 0.1;  // exp(-0.1*30) ~= 0.0498 < 0.1, so node 3 (dist 30) is pruned

    std::size_t visited_count =
        PropagateDamage(graph, /*epicenter_node=*/0, intensity, decay_rate, threshold, last_seen,
                         /*generation=*/1, [&](std::uint32_t node, double distance, double probability) {
                             visits.push_back({node, distance, probability});
                         });

    REQUIRE(visited_count == visits.size());
    REQUIRE(visited_count < graph.NodeCount());  // pruning actually engaged
    REQUIRE(visited_count == 3);                 // nodes at distance 0, 10, 20

    std::sort(visits.begin(), visits.end(),
              [](const Visit& a, const Visit& b) { return a.distance < b.distance; });

    for (std::size_t i = 1; i < visits.size(); ++i) {
        REQUIRE(visits[i].probability <= visits[i - 1].probability);  // monotonic decay
    }

    // Threshold invariant: nothing returned below the configured threshold.
    for (const auto& v : visits) {
        REQUIRE(v.probability >= threshold);
    }
}

TEST_CASE("RunMonteCarlo is deterministic given a fixed seed", "[monte_carlo]") {
    PropertyGraph graph = BuildChainGraph(50, 10.0);
    auto values = UniformInsuredValues(graph.NodeCount(), 100000.0);

    MonteCarloConfig config;
    config.master_seed = 777;
    config.num_scenarios = 2000;
    config.num_threads = 4;

    AggregateStats a = RunMonteCarlo(graph, values, config);
    AggregateStats b = RunMonteCarlo(graph, values, config);

    REQUIRE(a.scenario_count == b.scenario_count);
    REQUIRE(a.mean_loss == b.mean_loss);
    REQUIRE(a.p99_loss == b.p99_loss);
    REQUIRE(a.min_loss == b.min_loss);
    REQUIRE(a.max_loss == b.max_loss);
}

TEST_CASE("RunMonteCarlo results are independent of thread count", "[monte_carlo]") {
    PropertyGraph graph = BuildChainGraph(50, 10.0);
    auto values = UniformInsuredValues(graph.NodeCount(), 100000.0);

    MonteCarloConfig config;
    config.master_seed = 12345;
    config.num_scenarios = 3000;

    config.num_threads = 1;
    AggregateStats single = RunMonteCarlo(graph, values, config);

    config.num_threads = 8;
    AggregateStats multi = RunMonteCarlo(graph, values, config);

    REQUIRE(single.scenario_count == multi.scenario_count);
    REQUIRE(single.mean_loss == multi.mean_loss);
    REQUIRE(single.p99_loss == multi.p99_loss);
    REQUIRE(single.min_loss == multi.min_loss);
    REQUIRE(single.max_loss == multi.max_loss);
}

TEST_CASE("RunMonteCarlo processes every scenario exactly once under thread oversubscription",
          "[monte_carlo]") {
    PropertyGraph graph = BuildChainGraph(30, 10.0);
    auto values = UniformInsuredValues(graph.NodeCount(), 50000.0);

    MonteCarloConfig config;
    config.master_seed = 99;
    config.num_scenarios = 5000;
    config.num_threads = 64;  // deliberately oversubscribed

    AggregateStats stats = RunMonteCarlo(graph, values, config);
    REQUIRE(stats.scenario_count == config.num_scenarios);
    REQUIRE(stats.threads_used == 64);

    // Cross-check against a single-threaded run with the same seed.
    config.num_threads = 1;
    AggregateStats reference = RunMonteCarlo(graph, values, config);
    REQUIRE(stats.mean_loss == reference.mean_loss);
    REQUIRE(stats.p99_loss == reference.p99_loss);
}
