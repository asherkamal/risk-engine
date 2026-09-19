#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "engine/graph/property_graph.hpp"
#include "engine/spatial/point.hpp"

namespace engine::sim {

// Modified Dijkstra propagation from `epicenter_node` across `g`: expands
// nodes in non-decreasing cumulative-distance order (standard Dijkstra,
// non-negative edge weights), computing
//   damage_probability = intensity * exp(-decay_rate * cumulative_distance)
// at each node and invoking `visit(node_id, distance, damage_probability)`
// for every node whose probability is still >= probability_threshold.
// Expansion stops at a node once its probability drops below the
// threshold: this is a valid prune (not an approximation) because
// exp(-decay_rate * x) is monotonically non-increasing in x, so no node
// reachable only through an already-pruned node can have a smaller
// cumulative distance -- and therefore cannot cross back above the
// threshold.
//
// `Visitor` is a template parameter (not std::function) so the Monte Carlo
// hot path below can accumulate a running scalar with no per-call heap
// allocation or virtual dispatch; tests can instead pass a visitor that
// records full per-node results.
//
// `last_seen`/`generation` implement "visited this call" without an O(n)
// reset per call: `last_seen[node] == generation` means "already finalized
// during this call". Callers reuse the same `last_seen` buffer across many
// calls (one per Monte Carlo scenario) and increment `generation` each
// time, which is O(1) amortized instead of O(n) per scenario. `last_seen`
// must be sized to `g.NodeCount()` and must never be re-used with a
// `generation` value it has already seen with meaning "finalized" reset to
// something inconsistent -- callers own one buffer + counter per thread.
//
// Returns the number of nodes visited (probability >= threshold) -- used
// by tests to confirm pruning actually engages (visited_count < NodeCount())
// for fast-decaying scenarios, rather than just checking the math is right.
template <typename Visitor>
std::size_t PropagateDamage(const graph::PropertyGraph& g, std::uint32_t epicenter_node,
                             double intensity, double decay_rate, double probability_threshold,
                             std::vector<std::uint64_t>& last_seen, std::uint64_t generation,
                             Visitor&& visit) {
    using QueueEntry = std::pair<double, std::uint32_t>;  // (cumulative distance, node id)
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;
    pq.push({0.0, epicenter_node});

    std::size_t visited_count = 0;
    while (!pq.empty()) {
        auto [distance, node] = pq.top();
        pq.pop();

        // Lazy deletion: this node may already have been finalized via a
        // shorter earlier queue entry, in which case this one is stale.
        if (last_seen[node] == generation) {
            continue;
        }
        last_seen[node] = generation;

        const double probability = intensity * std::exp(-decay_rate * distance);
        if (probability < probability_threshold) {
            continue;  // pruned: do not expand this node's neighbors
        }
        ++visited_count;
        visit(node, distance, probability);

        auto neighbor_ids = g.NeighborIds(node);
        auto neighbor_distances = g.NeighborDistances(node);
        for (std::size_t i = 0; i < neighbor_ids.size(); ++i) {
            if (last_seen[neighbor_ids[i]] == generation) {
                continue;
            }
            pq.push({distance + neighbor_distances[i], neighbor_ids[i]});
        }
    }
    return visited_count;
}

struct MonteCarloConfig {
    std::uint64_t master_seed = 42;
    std::size_t num_scenarios = 10000;
    unsigned num_threads = 0;  // 0 = use std::thread::hardware_concurrency()
    double probability_threshold = 0.01;
    double intensity_min = 0.5;
    double intensity_max = 1.0;
    double decay_min = 0.05;
    double decay_max = 0.3;
};

struct AggregateStats {
    std::size_t scenario_count = 0;
    double mean_loss = 0.0;
    double p99_loss = 0.0;
    double min_loss = 0.0;
    double max_loss = 0.0;
    double wall_clock_ms = 0.0;
    unsigned threads_used = 0;
};

// Runs `config.num_scenarios` randomized catastrophe scenarios (randomized
// epicenter within the graph's bounding box, intensity, and decay rate)
// against `graph`, in parallel across `config.num_threads` worker threads.
//
// Each scenario's RNG is seeded deterministically from
// (config.master_seed, scenario_index) -- never from thread id or
// completion order -- so the full set of per-scenario results is identical
// regardless of thread count. Work is distributed via a single
// std::atomic<size_t> counter workers pull from (simpler than a generic
// task queue, and sufficient since scenarios are homogeneous, independent,
// and known-count up front).
//
// Each worker owns a private per-scenario-index result slot; no shared
// mutable state exists during the parallel phase itself, only in a single-
// threaded merge after all threads join, which also canonicalizes the
// reduction order by scenario index (not by thread or completion order) --
// necessary because floating-point addition is not associative, so summing
// in a different order can change the mean's last bit even over the exact
// same set of values.
AggregateStats RunMonteCarlo(const graph::PropertyGraph& graph,
                              const std::vector<double>& insured_values,
                              const MonteCarloConfig& config);

}  // namespace engine::sim
