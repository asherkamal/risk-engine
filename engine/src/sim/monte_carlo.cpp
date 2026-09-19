#include "engine/sim/monte_carlo.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>

namespace engine::sim {

namespace {

struct BoundingBox {
    double min_x, max_x, min_y, max_y;
};

BoundingBox ComputeBoundingBox(const graph::PropertyGraph& graph) {
    BoundingBox box{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (std::uint32_t i = 0; i < graph.NodeCount(); ++i) {
        auto p = graph.Position(i);
        box.min_x = std::min(box.min_x, p.x);
        box.max_x = std::max(box.max_x, p.x);
        box.min_y = std::min(box.min_y, p.y);
        box.max_y = std::max(box.max_y, p.y);
    }
    return box;
}

}  // namespace

AggregateStats RunMonteCarlo(const graph::PropertyGraph& graph,
                              const std::vector<double>& insured_values,
                              const MonteCarloConfig& config) {
    AggregateStats stats;
    if (graph.NodeCount() == 0 || config.num_scenarios == 0) {
        return stats;
    }

    const BoundingBox box = ComputeBoundingBox(graph);
    const unsigned num_threads =
        config.num_threads != 0 ? config.num_threads
                                 : std::max(1u, std::thread::hardware_concurrency());

    // Indexed by scenario index -- each index is written exactly once by
    // whichever worker claims it, so after all threads join this is safe
    // to read single-threaded with no further synchronization.
    std::vector<double> results(config.num_scenarios, 0.0);
    std::atomic<std::size_t> next_index{0};

    auto worker = [&]() {
        std::vector<std::uint64_t> last_seen(graph.NodeCount(), 0);
        std::uint64_t generation = 0;

        while (true) {
            const std::size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
            if (idx >= config.num_scenarios) {
                break;
            }
            ++generation;

            std::seed_seq seed_seq{static_cast<std::uint32_t>(config.master_seed),
                                    static_cast<std::uint32_t>(config.master_seed >> 32),
                                    static_cast<std::uint32_t>(idx),
                                    static_cast<std::uint32_t>(idx >> 32)};
            std::mt19937_64 rng(seed_seq);
            std::uniform_real_distribution<double> x_dist(box.min_x, box.max_x);
            std::uniform_real_distribution<double> y_dist(box.min_y, box.max_y);
            std::uniform_real_distribution<double> intensity_dist(config.intensity_min,
                                                                    config.intensity_max);
            std::uniform_real_distribution<double> decay_dist(config.decay_min, config.decay_max);

            const spatial::Point2D epicenter{x_dist(rng), y_dist(rng)};
            const double intensity = intensity_dist(rng);
            const double decay_rate = decay_dist(rng);

            double total_loss = 0.0;
            if (auto nearest = graph.NearestNode(epicenter)) {
                PropagateDamage(graph, *nearest, intensity, decay_rate,
                                 config.probability_threshold, last_seen, generation,
                                 [&](std::uint32_t node, double /*distance*/, double probability) {
                                     total_loss += probability * insured_values[node];
                                 });
            }
            results[idx] = total_loss;
        }
    };

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }
    const auto end = std::chrono::steady_clock::now();

    // Reduce in canonical scenario-index order (the order `results` is
    // already stored in), not thread-completion order: floating-point
    // addition is not associative, so summing the same values in a
    // different order can change the mean's last bit. Canonicalizing here
    // is what makes results bit-identical across different thread counts.
    double sum = 0.0;
    for (double v : results) {
        sum += v;
    }

    std::vector<double> sorted_results = results;
    std::sort(sorted_results.begin(), sorted_results.end());

    std::size_t p99_index =
        static_cast<std::size_t>(std::ceil(0.99 * static_cast<double>(sorted_results.size())));
    p99_index = std::min(p99_index == 0 ? 0 : p99_index - 1, sorted_results.size() - 1);

    stats.scenario_count = results.size();
    stats.mean_loss = sum / static_cast<double>(results.size());
    stats.p99_loss = sorted_results[p99_index];
    stats.min_loss = sorted_results.front();
    stats.max_loss = sorted_results.back();
    stats.wall_clock_ms = std::chrono::duration<double, std::milli>(end - start).count();
    stats.threads_used = num_threads;
    return stats;
}

}  // namespace engine::sim
