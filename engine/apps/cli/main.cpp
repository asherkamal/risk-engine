#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "engine/spatial/kdtree.hpp"

using engine::spatial::IndexedPoint;
using engine::spatial::KDTree;
using engine::spatial::Point2D;
using engine::spatial::SquaredDistance;

namespace {

std::vector<IndexedPoint> RandomPoints(std::size_t n, unsigned seed, double range) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-range, range);
    std::vector<IndexedPoint> points;
    points.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        points.push_back(IndexedPoint{Point2D{dist(rng), dist(rng)}, i});
    }
    return points;
}

std::size_t BruteForceCount(const std::vector<IndexedPoint>& points, Point2D center,
                             double radius) {
    const double radius_sq = radius * radius;
    std::size_t count = 0;
    for (const auto& p : points) {
        if (SquaredDistance(p.pos, center) <= radius_sq) {
            ++count;
        }
    }
    return count;
}

void RunComparison(std::size_t n, double range, double radius, int num_queries) {
    auto points = RandomPoints(n, static_cast<unsigned>(n), range);
    auto points_for_tree = points;

    KDTree tree(std::move(points_for_tree));

    std::mt19937 rng(9999);
    std::uniform_real_distribution<double> center_dist(-range, range);
    std::vector<Point2D> query_centers;
    query_centers.reserve(num_queries);
    for (int i = 0; i < num_queries; ++i) {
        query_centers.push_back(Point2D{center_dist(rng), center_dist(rng)});
    }

    std::size_t kdtree_total = 0;
    auto kd_start = std::chrono::steady_clock::now();
    for (auto center : query_centers) {
        kdtree_total += tree.RangeQuery(center, radius).size();
    }
    auto kd_end = std::chrono::steady_clock::now();

    std::size_t brute_total = 0;
    auto brute_start = std::chrono::steady_clock::now();
    for (auto center : query_centers) {
        brute_total += BruteForceCount(points, center, radius);
    }
    auto brute_end = std::chrono::steady_clock::now();

    const double kd_ms =
        std::chrono::duration<double, std::milli>(kd_end - kd_start).count();
    const double brute_ms =
        std::chrono::duration<double, std::milli>(brute_end - brute_start).count();

    std::printf("N=%zu, %d queries, radius=%.1f\n", n, num_queries, radius);
    std::printf("  KDTree:      %.3f ms total (%zu matches summed)\n", kd_ms, kdtree_total);
    std::printf("  Brute force: %.3f ms total (%zu matches summed)\n", brute_ms, brute_total);
    std::printf("  Speedup:     %.1fx\n\n", brute_ms / kd_ms);

    if (kdtree_total != brute_total) {
        std::printf("  !! MISMATCH: kdtree and brute force disagree on match count !!\n");
    }
}

}  // namespace

int main() {
    std::printf("Catastrophic Risk Simulation Engine - Step 1 benchmark\n");
    std::printf("KDTree RangeQuery vs brute-force linear scan\n\n");

    RunComparison(10000, 500.0, 10.0, 200);
    RunComparison(100000, 500.0, 10.0, 200);

    return 0;
}
