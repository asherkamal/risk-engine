#include <algorithm>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/spatial/kdtree.hpp"

using engine::spatial::IndexedPoint;
using engine::spatial::KDTree;
using engine::spatial::Point2D;
using engine::spatial::SquaredDistance;

namespace {

std::vector<std::uint32_t> BruteForceRangeQuery(const std::vector<IndexedPoint>& points,
                                                  Point2D center, double radius) {
    std::vector<std::uint32_t> out;
    const double radius_sq = radius * radius;
    for (const auto& p : points) {
        if (SquaredDistance(p.pos, center) <= radius_sq) {
            out.push_back(p.id);
        }
    }
    return out;
}

std::vector<std::uint32_t> Sorted(std::vector<std::uint32_t> v) {
    std::sort(v.begin(), v.end());
    return v;
}

std::vector<IndexedPoint> RandomPoints(std::size_t n, unsigned seed, double range = 100.0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-range, range);
    std::vector<IndexedPoint> points;
    points.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        points.push_back(IndexedPoint{Point2D{dist(rng), dist(rng)}, i});
    }
    return points;
}

}  // namespace

TEST_CASE("KDTree matches brute force on random point sets", "[kdtree]") {
    for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{50}, std::size_t{1000},
                           std::size_t{10000}}) {
        auto points = RandomPoints(n, static_cast<unsigned>(n) + 1);
        auto points_copy = points;  // KDTree takes ownership and reorders
        KDTree tree(std::move(points_copy));

        std::mt19937 rng(12345 + static_cast<unsigned>(n));
        std::uniform_real_distribution<double> center_dist(-120.0, 120.0);
        std::uniform_real_distribution<double> radius_dist(0.0, 60.0);

        for (int trial = 0; trial < 30; ++trial) {
            Point2D center{center_dist(rng), center_dist(rng)};
            double radius = radius_dist(rng);

            auto expected = Sorted(BruteForceRangeQuery(points, center, radius));
            auto actual = Sorted(tree.RangeQuery(center, radius));
            REQUIRE(actual == expected);
        }
    }
}

TEST_CASE("KDTree handles an empty tree", "[kdtree]") {
    KDTree tree(std::vector<IndexedPoint>{});
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.RangeQuery(Point2D{0.0, 0.0}, 100.0).empty());
}

TEST_CASE("KDTree handles a single point", "[kdtree]") {
    KDTree tree(std::vector<IndexedPoint>{IndexedPoint{Point2D{1.0, 1.0}, 42}});
    REQUIRE(tree.RangeQuery(Point2D{1.0, 1.0}, 0.0) == std::vector<std::uint32_t>{42});
    REQUIRE(tree.RangeQuery(Point2D{10.0, 10.0}, 0.5).empty());
}

TEST_CASE("KDTree radius boundary is inclusive", "[kdtree]") {
    // Point sits exactly `radius` away from center (3-4-5 triangle -> distance 5).
    KDTree tree(std::vector<IndexedPoint>{IndexedPoint{Point2D{3.0, 4.0}, 7}});
    REQUIRE(tree.RangeQuery(Point2D{0.0, 0.0}, 5.0) == std::vector<std::uint32_t>{7});
    REQUIRE(tree.RangeQuery(Point2D{0.0, 0.0}, 4.999).empty());
}

TEST_CASE("KDTree handles heavy duplicate coordinates without pathological recursion",
          "[kdtree]") {
    std::vector<IndexedPoint> points;
    for (std::uint32_t i = 0; i < 5000; ++i) {
        points.push_back(IndexedPoint{Point2D{7.0, 7.0}, i});
    }
    // A few distinct points mixed in so the duplicate cluster isn't the whole tree.
    points.push_back(IndexedPoint{Point2D{500.0, 500.0}, 99999});

    KDTree tree(points);  // must not stack-overflow / infinite-loop on construction
    auto result = Sorted(tree.RangeQuery(Point2D{7.0, 7.0}, 0.001));
    REQUIRE(result.size() == 5000);

    auto far = tree.RangeQuery(Point2D{500.0, 500.0}, 0.001);
    REQUIRE(far == std::vector<std::uint32_t>{99999});
}

TEST_CASE("KDTree degenerate queries", "[kdtree]") {
    auto points = RandomPoints(200, 777);
    KDTree tree(points);

    // Huge radius returns everything.
    auto all = tree.RangeQuery(Point2D{0.0, 0.0}, 1e9);
    REQUIRE(all.size() == 200);

    // Center far outside the point cloud with a small radius returns nothing.
    auto none = tree.RangeQuery(Point2D{1e6, 1e6}, 1.0);
    REQUIRE(none.empty());
}
