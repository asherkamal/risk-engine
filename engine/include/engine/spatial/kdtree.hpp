#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "engine/spatial/point.hpp"

namespace engine::spatial {

// A 2D k-d tree over point data, built from scratch (no third-party spatial
// index library). Chosen over an R-tree because the indexed data here is
// zero-extent points and the query shape is "points within radius X of a
// center point" — a bounded range query k-d trees prune natively via
// splitting-plane distance checks. R-trees earn their bounding-box/overlap
// machinery for extent data (polygons, footprints) overlapping a query
// region, which doesn't apply to point properties.
class KDTree {
public:
    // Takes ownership of `points` and reorders them in place during the
    // build (array-of-structs layout, no per-node heap allocation).
    // `leaf_bucket_size` bounds how far the tree recurses: subtrees at or
    // below this many points become brute-force-scanned leaves instead of
    // splitting further. This guards against pathological recursion depth
    // on heavily duplicated/near-duplicate coordinates (e.g. condo towers,
    // resurveyed parcels), which is expected in coastline-clustered data.
    explicit KDTree(std::vector<IndexedPoint> points, std::size_t leaf_bucket_size = 16);

    // Returns the ids of every point within `radius` (inclusive) of
    // `center`. Internally compares squared distances only; sqrt is never
    // called on this hot path.
    std::vector<std::uint32_t> RangeQuery(Point2D center, double radius) const;

    // Returns the id of the single closest point to `center`, or
    // std::nullopt if the tree is empty. Used to snap a continuous event
    // epicenter onto the nearest node in the property graph.
    std::optional<std::uint32_t> FindNearest(Point2D center) const;

    std::size_t size() const noexcept { return points_.size(); }

private:
    struct Node {
        bool is_leaf;

        // Leaf: brute-force-scanned range within points_.
        std::size_t leaf_begin = 0;
        std::size_t leaf_end = 0;

        // Internal: split axis (0 = x, 1 = y) and value, plus child indices
        // into nodes_. Children are indices, not pointers, so the tree is a
        // flat, cache-friendly array rather than a pointer-linked structure.
        int axis = 0;
        double split_value = 0.0;
        std::size_t left = 0;
        std::size_t right = 0;
    };

    // Recursively partitions points_[begin, end) via std::nth_element on the
    // median (O(n) per level, O(n log n) total build), alternating split
    // axis by depth so the tree rebalances regardless of input ordering.
    // Returns the index of the constructed node in nodes_.
    std::size_t Build(std::size_t begin, std::size_t end, int depth);

    void RangeQueryRecursive(std::size_t node_idx, Point2D center, double radius_sq,
                              std::vector<std::uint32_t>& out) const;

    // Best-so-far nearest-neighbor descent: visits the child on the query's
    // side first, then only crosses the splitting plane if it could still
    // hold something closer than the current best (standard k-d tree NN
    // pruning, same idea as RangeQuery's pruning but bounded by the running
    // best distance instead of a fixed radius).
    void FindNearestRecursive(std::size_t node_idx, Point2D center, double& best_dist_sq,
                                std::optional<std::uint32_t>& best_id) const;

    std::vector<IndexedPoint> points_;
    std::vector<Node> nodes_;
    std::size_t leaf_bucket_size_;
    std::size_t root_ = 0;
};

}  // namespace engine::spatial
