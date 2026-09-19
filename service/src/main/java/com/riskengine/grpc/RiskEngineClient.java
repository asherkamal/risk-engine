package com.riskengine.grpc;

import java.util.List;

import com.riskengine.domain.Property;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import jakarta.annotation.PreDestroy;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import riskengine.RiskEngineGrpc;
import riskengine.RiskEngineOuterClass.AggregateStats;
import riskengine.RiskEngineOuterClass.RunSimulationRequest;
import riskengine.RiskEngineOuterClass.RunSimulationResponse;
import riskengine.RiskEngineOuterClass.SimulationConfig;

/** Thin wrapper around the generated gRPC stub -- the only place in the
 * service that talks to the C++ engine (Step 3) directly. */
@Component
public class RiskEngineClient {

    private final ManagedChannel channel;
    private final RiskEngineGrpc.RiskEngineBlockingStub stub;

    public RiskEngineClient(@Value("${risk-engine.grpc.host:localhost}") String host,
                             @Value("${risk-engine.grpc.port:50051}") int port) {
        this.channel = ManagedChannelBuilder.forAddress(host, port)
                .usePlaintext()  // matches the server's InsecureServerCredentials (Step 3) -- local dev only
                .build();
        this.stub = RiskEngineGrpc.newBlockingStub(channel);
    }

    public AggregateStats runSimulation(List<Property> properties, SimulationConfig config) {
        RunSimulationRequest.Builder request = RunSimulationRequest.newBuilder().setConfig(config);
        for (Property property : properties) {
            request.addProperties(riskengine.RiskEngineOuterClass.Property.newBuilder()
                    .setLat(property.lat())
                    .setLon(property.lon())
                    .setInsuredValue(property.insuredValue()));
        }
        RunSimulationResponse response = stub.runSimulation(request.build());
        return response.getStats();
    }

    @PreDestroy
    public void shutdown() {
        channel.shutdown();
    }
}
