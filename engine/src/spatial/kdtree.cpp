#include "engine/spatial/kdtree.hpp"

#include <algorithm>
#include <cmath>

namespace engine::spatial {

namespace {
double AxisValue(Point2D p, int axis) { return axis == 0 ? p.x : p.y; }
}  // namespace

KDTree::KDTree(std::vector<IndexedPoint> points, std::size_t leaf_bucket_size)
    : points_(std::move(points)), leaf_bucket_size_(leaf_bucket_size) {
    if (!points_.empty()) {
        nodes_.reserve(2 * (points_.size() / std::max<std::size_t>(leaf_bucket_size_, 1) + 1));
        root_ = Build(0, points_.size(), 0);
    }
}

std::size_t KDTree::Build(std::size_t begin, std::size_t end, int depth) {
    const std::size_t count = end - begin;
    const std::size_t idx = nodes_.size();
    nodes_.emplace_back();

    if (count <= leaf_bucket_size_) {
        nodes_[idx].is_leaf = true;
        nodes_[idx].leaf_begin = begin;
        nodes_[idx].leaf_end = end;
        return idx;
    }

    const int axis = depth % 2;
    const std::size_t mid = begin + count / 2;

    std::nth_element(points_.begin() + static_cast<std::ptrdiff_t>(begin),
                      points_.begin() + static_cast<std::ptrdiff_t>(mid),
                      points_.begin() + static_cast<std::ptrdiff_t>(end),
                      [axis](const IndexedPoint& a, const IndexedPoint& b) {
                          return AxisValue(a.pos, axis) < AxisValue(b.pos, axis);
                      });

    // NOTE: nodes_ may reallocate during recursive Build() calls, so we
    // cannot hold a reference into nodes_ across them — write fields via
    // index after both children are built.
    const double split_value = AxisValue(points_[mid].pos, axis);
    const std::size_t left = Build(begin, mid, depth + 1);
    const std::size_t right = Build(mid, end, depth + 1);

    nodes_[idx].is_leaf = false;
    nodes_[idx].axis = axis;
    nodes_[idx].split_value = split_value;
    nodes_[idx].left = left;
    nodes_[idx].right = right;
    return idx;
}

std::optional<std::uint32_t> KDTree::FindNearest(Point2D center) const {
    if (points_.empty()) {
        return std::nullopt;
    }
    double best_dist_sq = std::numeric_limits<double>::infinity();
    std::optional<std::uint32_t> best_id;
    FindNearestRecursive(root_, center, best_dist_sq, best_id);
    return best_id;
}

void KDTree::FindNearestRecursive(std::size_t node_idx, Point2D center, double& best_dist_sq,
                                    std::optional<std::uint32_t>& best_id) const {
    const Node& node = nodes_[node_idx];

    if (node.is_leaf) {
        for (std::size_t i = node.leaf_begin; i < node.leaf_end; ++i) {
            const double d = SquaredDistance(points_[i].pos, center);
            if (d < best_dist_sq) {
                best_dist_sq = d;
                best_id = points_[i].id;
            }
        }
        return;
    }

    const double center_axis_value = AxisValue(center, node.axis);
    const double diff = center_axis_value - node.split_value;

    if (diff <= 0.0) {
        FindNearestRecursive(node.left, center, best_dist_sq, best_id);
        if (diff * diff < best_dist_sq) {
            FindNearestRecursive(node.right, center, best_dist_sq, best_id);
        }
    } else {
        FindNearestRecursive(node.right, center, best_dist_sq, best_id);
        if (diff * diff < best_dist_sq) {
            FindNearestRecursive(node.left, center, best_dist_sq, best_id);
        }
    }
}

std::vector<std::uint32_t> KDTree::RangeQuery(Point2D center, double radius) const {
    std::vector<std::uint32_t> out;
    if (points_.empty()) {
        return out;
    }
    const double radius_sq = radius * radius;
    RangeQueryRecursive(root_, center, radius_sq, out);
    return out;
}

void KDTree::RangeQueryRecursive(std::size_t node_idx, Point2D center, double radius_sq,
                                   std::vector<std::uint32_t>& out) const {
    const Node& node = nodes_[node_idx];

    if (node.is_leaf) {
        for (std::size_t i = node.leaf_begin; i < node.leaf_end; ++i) {
            if (SquaredDistance(points_[i].pos, center) <= radius_sq) {
                out.push_back(points_[i].id);
            }
        }
        return;
    }

    const double center_axis_value = AxisValue(center, node.axis);
    const double diff = center_axis_value - node.split_value;

    // Always descend the side containing the query center.
    if (diff <= 0.0) {
        RangeQueryRecursive(node.left, center, radius_sq, out);
        // Only cross the splitting plane if the query sphere reaches it.
        if (diff * diff <= radius_sq) {
            RangeQueryRecursive(node.right, center, radius_sq, out);
        }
    } else {
        RangeQueryRecursive(node.right, center, radius_sq, out);
        if (diff * diff <= radius_sq) {
            RangeQueryRecursive(node.left, center, radius_sq, out);
        }
    }
}

}  // namespace engine::spatial
