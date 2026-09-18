#include "engine/spatial/point.hpp"

#include <cmath>
#include <numeric>

namespace engine::spatial {

namespace {
constexpr double kEarthRadiusKm = 6371.0;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
}  // namespace

std::vector<Point2D> ProjectToPlane(const std::vector<GeoPoint>& geo_points) {
    std::vector<Point2D> result;
    result.reserve(geo_points.size());
    if (geo_points.empty()) {
        return result;
    }

    double lat_sum = 0.0;
    for (const auto& p : geo_points) {
        lat_sum += p.lat;
    }
    const double mean_lat_rad = (lat_sum / static_cast<double>(geo_points.size())) * kDegToRad;
    const double cos_mean_lat = std::cos(mean_lat_rad);

    for (const auto& p : geo_points) {
        const double x = p.lon * kDegToRad * cos_mean_lat * kEarthRadiusKm;
        const double y = p.lat * kDegToRad * kEarthRadiusKm;
        result.push_back(Point2D{x, y});
    }
    return result;
}

}  // namespace engine::spatial
