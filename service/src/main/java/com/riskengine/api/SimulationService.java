package com.riskengine.api;

import java.math.BigDecimal;
import java.time.OffsetDateTime;
import java.util.List;
import java.util.Map;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.riskengine.domain.LossEvent;
import com.riskengine.domain.Peril;
import com.riskengine.domain.PerilRegistry;
import com.riskengine.domain.Policy;
import com.riskengine.domain.Portfolio;
import com.riskengine.domain.Property;
import com.riskengine.grpc.RiskEngineClient;
import com.riskengine.persistence.SimulationRunEntity;
import com.riskengine.persistence.SimulationRunRepository;
import org.springframework.stereotype.Service;
import riskengine.RiskEngineOuterClass.AggregateStats;
import riskengine.RiskEngineOuterClass.SimulationConfig;

@Service
public class SimulationService {

    private final RiskEngineClient client;
    private final SimulationRunRepository repository;
    private final ObjectMapper objectMapper;

    public SimulationService(RiskEngineClient client, SimulationRunRepository repository,
                              ObjectMapper objectMapper) {
        this.client = client;
        this.repository = repository;
        this.objectMapper = objectMapper;
    }

    public LossEvent runSimulation(Portfolio portfolio, long masterSeed, long numScenarios) {
        // A submitted portfolio is single-peril: every policy shares the
        // perilType SimulationRequest.toPortfolio() assigned them, and
        // Peril.buildSimulationConfig is dispatched once through the base
        // class -- no branching on peril type happens here or in Peril
        // itself.
        String perilType = portfolio.policies().get(0).perilType();
        Peril peril = PerilRegistry.require(perilType);
        SimulationConfig config = peril.buildSimulationConfig(masterSeed, numScenarios);

        List<Property> properties = portfolio.policies().stream().map(Policy::property).toList();
        AggregateStats stats = client.runSimulation(properties, config);

        LossEvent event = new LossEvent(peril.type(), stats.getScenarioCount(), stats.getMeanLoss(),
                stats.getP99Loss(), stats.getMinLoss(), stats.getMaxLoss(), stats.getWallClockMs(),
                stats.getThreadsUsed());

        persist(peril, masterSeed, numScenarios, config, event);
        return event;
    }

    private void persist(Peril peril, long masterSeed, long numScenarios, SimulationConfig config,
                          LossEvent event) {
        SimulationRunEntity run = new SimulationRunEntity();
        run.setPerilType(peril.type());
        run.setNumScenarios(numScenarios);
        run.setMasterSeed(masterSeed);
        run.setConfig(toJson(config));
        run.setMeanLoss(BigDecimal.valueOf(event.meanLoss()));
        run.setP99Loss(BigDecimal.valueOf(event.p99Loss()));
        run.setMinLoss(BigDecimal.valueOf(event.minLoss()));
        run.setMaxLoss(BigDecimal.valueOf(event.maxLoss()));
        run.setWallClockMs(event.wallClockMs());
        run.setStatus("completed");
        run.setCompletedAt(OffsetDateTime.now());
        repository.save(run);
    }

    private String toJson(SimulationConfig config) {
        try {
            return objectMapper.writeValueAsString(Map.of(
                    "linkRadiusKm", config.getLinkRadiusKm(),
                    "intensityMin", config.getIntensityMin(),
                    "intensityMax", config.getIntensityMax(),
                    "decayMin", config.getDecayMin(),
                    "decayMax", config.getDecayMax()
            ));
        } catch (Exception e) {
            throw new IllegalStateException("Failed to serialize simulation config", e);
        }
    }
}
