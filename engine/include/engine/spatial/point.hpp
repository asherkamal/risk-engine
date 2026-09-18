#pragma once

#include <cstdint>
#include <vector>

namespace engine::spatial {

// Raw geographic coordinate, degrees.
struct GeoPoint {
    double lat;
    double lon;
};

// Projected planar coordinate (kilometers), valid only relative to the
// reference latitude used to produce it. Not a global coordinate system.
struct Point2D {
    double x;
    double y;
};

struct IndexedPoint {
    Point2D pos;
    std::uint32_t id;
};

inline double SquaredDistance(Point2D a, Point2D b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Projects geographic points to a local tangent-plane Cartesian system
// (equirectangular approximation) centered on the mean latitude of the
// input set. Accurate for regional queries (continental-US scale); not
// valid across polar regions or for exact global/great-circle distance —
// a real production system spanning those would need UTM zones or full
// haversine math instead.
std::vector<Point2D> ProjectToPlane(const std::vector<GeoPoint>& geo_points);

}  // namespace engine::spatial
