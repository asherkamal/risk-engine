// Minimal real gRPC client used to verify the server end-to-end: builds a
// synthetic portfolio, sends it over the wire, and prints whatever the
// engine actually computed. Not a unit test -- this is the Step 3
// "real output, not mocked" check-in artifact, run against a live
// risk_engine_server process.
#include <cstdio>
#include <random>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "risk_engine.grpc.pb.h"

int main(int argc, char** argv) {
    std::string target = argc > 1 ? argv[1] : "localhost:50051";

    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = riskengine::RiskEngine::NewStub(channel);

    riskengine::RunSimulationRequest request;

    // Synthetic properties loosely clustered along a stretch of coastline
    // (fixed longitude band, latitude spread), matching the kind of layout
    // the real NOAA/USGS-driven portfolio will have from Step 4 onward.
    std::mt19937 rng(2024);
    std::uniform_real_distribution<double> lat_dist(25.0, 35.0);   // ~FL to ~NC
    std::uniform_real_distribution<double> lon_dist(-81.0, -79.0);
    std::lognormal_distribution<double> value_dist(12.0, 0.6);

    constexpr int kNumProperties = 500;
    for (int i = 0; i < kNumProperties; ++i) {
        auto* p = request.add_properties();
        p->set_lat(lat_dist(rng));
        p->set_lon(lon_dist(rng));
        p->set_insured_value(value_dist(rng));
    }

    auto* config = request.mutable_config();
    config->set_link_radius_km(25.0);
    config->set_master_seed(42);
    config->set_num_scenarios(10000);
    config->set_probability_threshold(0.01);
    config->set_intensity_min(0.5);
    config->set_intensity_max(1.0);
    config->set_decay_min(0.05);
    config->set_decay_max(0.3);

    riskengine::RunSimulationResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub->RunSimulation(&context, request, &response);

    if (!status.ok()) {
        std::fprintf(stderr, "RPC failed: %s\n", status.error_message().c_str());
        return 1;
    }

    const auto& stats = response.stats();
    std::printf("gRPC RunSimulation response (from live server at %s):\n", target.c_str());
    std::printf("  Properties submitted: %d\n", kNumProperties);
    std::printf("  Scenarios executed:   %llu\n",
                 static_cast<unsigned long long>(stats.scenario_count()));
    std::printf("  Threads used:         %u\n", stats.threads_used());
    std::printf("  Wall clock:           %.2f ms\n", stats.wall_clock_ms());
    std::printf("  Mean loss:            $%.2f\n", stats.mean_loss());
    std::printf("  P99 loss:             $%.2f\n", stats.p99_loss());
    std::printf("  Min loss:             $%.2f\n", stats.min_loss());
    std::printf("  Max loss:             $%.2f\n", stats.max_loss());
    return 0;
}
