package com.riskengine.api;

import java.util.List;

import com.riskengine.domain.LossEvent;
import com.riskengine.grpc.RiskEngineClient;
import com.riskengine.persistence.SimulationRunRepository;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;
import org.testcontainers.utility.DockerImageName;
import riskengine.RiskEngineOuterClass.AggregateStats;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.Mockito.when;

/**
 * Real Postgres (via Testcontainers, running the actual Step 4 Flyway
 * migrations) exercises the DB integration end-to-end. The gRPC call to the
 * C++ engine is boundary-mocked here: a real call would require the
 * separately built risk_engine_server.exe running, which Maven doesn't
 * control and CI can't assume -- that real end-to-end path is verified
 * manually (see the Step 5 check-in) with the actual server process up.
 */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class SimulationControllerIntegrationTest {

    @Container
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>(
            DockerImageName.parse("postgis/postgis:16-3.4").asCompatibleSubstituteFor("postgres"))
            .withDatabaseName("riskengine")
            .withUsername("riskengine")
            .withPassword("riskengine");

    @DynamicPropertySource
    static void configureDatasource(DynamicPropertyRegistry registry) {
        registry.add("spring.datasource.url", postgres::getJdbcUrl);
        registry.add("spring.datasource.username", postgres::getUsername);
        registry.add("spring.datasource.password", postgres::getPassword);
    }

    @Autowired
    private TestRestTemplate restTemplate;

    @Autowired
    private SimulationRunRepository repository;

    @MockBean
    private RiskEngineClient riskEngineClient;

    @Test
    void submittingPortfolioRunsSimulationAndPersistsTheRun() {
        AggregateStats fakeStats = AggregateStats.newBuilder()
                .setScenarioCount(5000)
                .setMeanLoss(123456.78)
                .setP99Loss(987654.32)
                .setMinLoss(1000.0)
                .setMaxLoss(2_000_000.0)
                .setWallClockMs(12.3)
                .setThreadsUsed(8)
                .build();
        when(riskEngineClient.runSimulation(anyList(), any())).thenReturn(fakeStats);

        SimulationRequest request = new SimulationRequest("hurricane", List.of(
                new PropertyRequest(25.76, -80.19, 500000.0),
                new PropertyRequest(27.95, -82.45, 750000.0)
        ), 42L, 5000L);

        ResponseEntity<LossEvent> response =
                restTemplate.postForEntity("/api/v1/simulations", request, LossEvent.class);

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.OK);
        assertThat(response.getBody()).isNotNull();
        assertThat(response.getBody().scenarioCount()).isEqualTo(5000);
        assertThat(response.getBody().perilType()).isEqualTo("hurricane");

        assertThat(repository.count()).isEqualTo(1);
        assertThat(repository.findAll().get(0).getId()).isNotNull();
    }

    @Test
    void rejectsUnknownPerilTypeWithBadRequest() {
        SimulationRequest request = new SimulationRequest("tornado", List.of(
                new PropertyRequest(25.76, -80.19, 500000.0)
        ), 42L, 1000L);

        ResponseEntity<String> response =
                restTemplate.postForEntity("/api/v1/simulations", request, String.class);

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
    }
}
