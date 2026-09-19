#include "engine/graph/property_graph.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace engine::graph {

namespace {

std::vector<spatial::IndexedPoint> ToIndexedPoints(const std::vector<spatial::Point2D>& points) {
    std::vector<spatial::IndexedPoint> out;
    out.reserve(points.size());
    for (std::uint32_t i = 0; i < points.size(); ++i) {
        out.push_back(spatial::IndexedPoint{points[i], i});
    }
    return out;
}

}  // namespace

PropertyGraph::PropertyGraph(const std::vector<spatial::Point2D>& points, double link_radius,
                              std::size_t max_neighbors)
    : positions_(points), nearest_index_(ToIndexedPoints(points)) {
    const std::size_t n = points.size();
    offsets_.assign(n + 1, 0);

    // Per-node (distance, neighbor_id) lists, trimmed to the nearest
    // max_neighbors, before flattening into the final CSR arrays.
    std::vector<std::vector<std::pair<double, std::uint32_t>>> adjacency(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        auto candidates = nearest_index_.RangeQuery(points[i], link_radius);

        std::vector<std::pair<double, std::uint32_t>> by_distance;
        by_distance.reserve(candidates.size());
        for (auto candidate_id : candidates) {
            if (candidate_id == i) {
                continue;  // exclude self
            }
            const double distance = std::sqrt(spatial::SquaredDistance(points[i], points[candidate_id]));
            by_distance.push_back({distance, candidate_id});
        }
        std::sort(by_distance.begin(), by_distance.end());
        if (by_distance.size() > max_neighbors) {
            by_distance.resize(max_neighbors);
        }
        adjacency[i] = std::move(by_distance);
    }

    std::size_t total_edges = 0;
    for (const auto& list : adjacency) {
        total_edges += list.size();
    }
    neighbor_ids_.reserve(total_edges);
    neighbor_distances_.reserve(total_edges);

    for (std::uint32_t i = 0; i < n; ++i) {
        offsets_[i] = neighbor_ids_.size();
        for (const auto& [distance, id] : adjacency[i]) {
            neighbor_ids_.push_back(id);
            neighbor_distances_.push_back(distance);
        }
    }
    offsets_[n] = neighbor_ids_.size();
}

std::span<const std::uint32_t> PropertyGraph::NeighborIds(std::uint32_t node_id) const {
    return {neighbor_ids_.data() + offsets_[node_id], offsets_[node_id + 1] - offsets_[node_id]};
}

std::span<const double> PropertyGraph::NeighborDistances(std::uint32_t node_id) const {
    return {neighbor_distances_.data() + offsets_[node_id],
            offsets_[node_id + 1] - offsets_[node_id]};
}

std::optional<std::uint32_t> PropertyGraph::NearestNode(spatial::Point2D query) const {
    return nearest_index_.FindNearest(query);
}

}  // namespace engine::graph
