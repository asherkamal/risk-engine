package com.riskengine.persistence;

import java.math.BigDecimal;
import java.time.OffsetDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import org.hibernate.annotations.JdbcTypeCode;
import org.hibernate.type.SqlTypes;

/** Maps to the simulation_runs table created by db/migrations/V1__init_schema.sql
 * (Step 4) -- schema ownership stays with Flyway, this is read/write only. */
@Entity
@Table(name = "simulation_runs")
public class SimulationRunEntity {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "peril_type", nullable = false)
    private String perilType;

    @Column(name = "num_scenarios", nullable = false)
    private long numScenarios;

    @Column(name = "master_seed", nullable = false)
    private long masterSeed;

    @JdbcTypeCode(SqlTypes.JSON)
    @Column(name = "config", nullable = false, columnDefinition = "jsonb")
    private String config;

    // BigDecimal, not Double: these columns are NUMERIC(14,2) in the Step 4
    // migration, and Hibernate's schema validator maps Double to float8,
    // which fails validation against a NUMERIC column. wall_clock_ms below
    // stays Double because that column really is DOUBLE PRECISION.
    @Column(name = "mean_loss")
    private BigDecimal meanLoss;

    @Column(name = "p99_loss")
    private BigDecimal p99Loss;

    @Column(name = "min_loss")
    private BigDecimal minLoss;

    @Column(name = "max_loss")
    private BigDecimal maxLoss;

    @Column(name = "wall_clock_ms")
    private Double wallClockMs;

    @Column(name = "status", nullable = false)
    private String status = "pending";

    @Column(name = "started_at", nullable = false)
    private OffsetDateTime startedAt = OffsetDateTime.now();

    @Column(name = "completed_at")
    private OffsetDateTime completedAt;

    public Long getId() {
        return id;
    }

    public void setPerilType(String perilType) {
        this.perilType = perilType;
    }

    public void setNumScenarios(long numScenarios) {
        this.numScenarios = numScenarios;
    }

    public void setMasterSeed(long masterSeed) {
        this.masterSeed = masterSeed;
    }

    public void setConfig(String config) {
        this.config = config;
    }

    public void setMeanLoss(BigDecimal meanLoss) {
        this.meanLoss = meanLoss;
    }

    public void setP99Loss(BigDecimal p99Loss) {
        this.p99Loss = p99Loss;
    }

    public void setMinLoss(BigDecimal minLoss) {
        this.minLoss = minLoss;
    }

    public void setMaxLoss(BigDecimal maxLoss) {
        this.maxLoss = maxLoss;
    }

    public void setWallClockMs(Double wallClockMs) {
        this.wallClockMs = wallClockMs;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public void setCompletedAt(OffsetDateTime completedAt) {
        this.completedAt = completedAt;
    }
}
