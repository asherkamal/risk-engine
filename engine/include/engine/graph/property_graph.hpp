#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "engine/spatial/kdtree.hpp"
#include "engine/spatial/point.hpp"

namespace engine::graph {

// A spatial-proximity graph over properties, stored as CSR (compressed
// sparse row) adjacency: built once per portfolio, then read thousands of
// times (once per Monte Carlo scenario), so a cache-friendly flat layout
// matters far more here than build-time convenience.
//
// Node ids are dense: node `i` corresponds to `points[i]` as passed to the
// constructor (the IndexedPoint::id field is ignored — positions are
// re-indexed 0..N-1 by array position).
class PropertyGraph {
public:
    // Links every point to every other point within `link_radius` (a
    // physically meaningful hazard-correlation radius — e.g. a wind-field
    // or shaking-attenuation range — not an arbitrary fixed fan-out),
    // reusing a KDTree range query per node. To bound degree explosion in
    // dense clusters, only the nearest `max_neighbors` within that radius
    // are kept per node.
    PropertyGraph(const std::vector<spatial::Point2D>& points, double link_radius,
                  std::size_t max_neighbors = 32);

    std::size_t NodeCount() const noexcept { return positions_.size(); }
    spatial::Point2D Position(std::uint32_t node_id) const { return positions_[node_id]; }

    std::span<const std::uint32_t> NeighborIds(std::uint32_t node_id) const;
    std::span<const double> NeighborDistances(std::uint32_t node_id) const;

    // Snaps a continuous point (e.g. an event epicenter) onto the id of the
    // nearest node in this graph.
    std::optional<std::uint32_t> NearestNode(spatial::Point2D query) const;

private:
    std::vector<spatial::Point2D> positions_;   // indexed directly by node id
    std::vector<std::uint32_t> neighbor_ids_;   // CSR flattened
    std::vector<double> neighbor_distances_;    // CSR flattened, parallel to neighbor_ids_
    std::vector<std::size_t> offsets_;          // size NodeCount() + 1
    spatial::KDTree nearest_index_;             // reused for NearestNode lookups
};

}  // namespace engine::graph
