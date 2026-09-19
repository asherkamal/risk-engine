#include <cstdio>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "engine/graph/property_graph.hpp"
#include "engine/sim/monte_carlo.hpp"
#include "engine/spatial/point.hpp"
#include "risk_engine.grpc.pb.h"

using engine::graph::PropertyGraph;
using engine::sim::MonteCarloConfig;
using engine::sim::RunMonteCarlo;
using engine::spatial::GeoPoint;
using engine::spatial::ProjectToPlane;

namespace {

// engine_core stays free of any gRPC/protobuf dependency by design (see
// the build-plan notes for Step 3): this file is the only place that
// translates between wire messages and engine types.
class RiskEngineServiceImpl final : public riskengine::RiskEngine::Service {
public:
    grpc::Status RunSimulation(grpc::ServerContext* /*context*/,
                                const riskengine::RunSimulationRequest* request,
                                riskengine::RunSimulationResponse* response) override {
        if (request->properties_size() == 0) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "properties must not be empty");
        }

        std::vector<GeoPoint> geo_points;
        std::vector<double> insured_values;
        geo_points.reserve(request->properties_size());
        insured_values.reserve(request->properties_size());
        for (const auto& p : request->properties()) {
            geo_points.push_back(GeoPoint{p.lat(), p.lon()});
            insured_values.push_back(p.insured_value());
        }
        std::vector<engine::spatial::Point2D> positions = ProjectToPlane(geo_points);

        const auto& cfg = request->config();
        if (cfg.link_radius_km() <= 0.0) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                 "config.link_radius_km must be positive");
        }

        PropertyGraph graph(positions, cfg.link_radius_km(),
                             cfg.max_neighbors() != 0 ? cfg.max_neighbors() : 32);

        MonteCarloConfig mc_config;
        mc_config.master_seed = cfg.master_seed();
        mc_config.num_scenarios = cfg.num_scenarios() != 0 ? cfg.num_scenarios() : 10000;
        mc_config.num_threads = cfg.num_threads();
        mc_config.probability_threshold =
            cfg.probability_threshold() != 0.0 ? cfg.probability_threshold() : 0.01;
        mc_config.intensity_min = cfg.intensity_min() != 0.0 ? cfg.intensity_min() : 0.5;
        mc_config.intensity_max = cfg.intensity_max() != 0.0 ? cfg.intensity_max() : 1.0;
        mc_config.decay_min = cfg.decay_min() != 0.0 ? cfg.decay_min() : 0.05;
        mc_config.decay_max = cfg.decay_max() != 0.0 ? cfg.decay_max() : 0.3;

        auto stats = RunMonteCarlo(graph, insured_values, mc_config);

        auto* out = response->mutable_stats();
        out->set_scenario_count(stats.scenario_count);
        out->set_mean_loss(stats.mean_loss);
        out->set_p99_loss(stats.p99_loss);
        out->set_min_loss(stats.min_loss);
        out->set_max_loss(stats.max_loss);
        out->set_wall_clock_ms(stats.wall_clock_ms);
        out->set_threads_used(stats.threads_used);

        return grpc::Status::OK;
    }
};

}  // namespace

int main() {
    // Insecure credentials + no auth: fine for local dev / this portfolio
    // demo, not how this would run in a real deployment (Step 7 would put
    // this behind TLS and/or a service mesh, not something to fake now).
    const std::string address = "0.0.0.0:50051";
    RiskEngineServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::printf("Risk engine gRPC server listening on %s\n", address.c_str());
    server->Wait();
    return 0;
}
