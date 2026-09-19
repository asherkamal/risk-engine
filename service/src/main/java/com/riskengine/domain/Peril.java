package com.riskengine.domain;

import riskengine.RiskEngineOuterClass.SimulationConfig;

/**
 * Base type for a catastrophe peril. Each subtype supplies its own
 * intensity/decay/link-radius characteristics by overriding the abstract
 * accessor methods below; {@link #buildSimulationConfig} then dispatches to
 * those overrides through ordinary virtual method calls. Callers (e.g.
 * {@code SimulationService}) invoke this base-class method polymorphically
 * and never branch on peril type themselves -- the type-specific behavior
 * lives entirely in each subclass, not in an if/else or switch at the call
 * site.
 */
public abstract class Peril {

    /** Short identifier stored on Policy rows, e.g. "hurricane". */
    public abstract String type();

    protected abstract double minIntensity();

    protected abstract double maxIntensity();

    protected abstract double minDecayRate();

    protected abstract double maxDecayRate();

    /** Hazard-correlation radius (km) used to link properties into the
     * simulation graph -- how far this peril's damage can propagate between
     * neighboring properties. Wildly different per peril: a hurricane's
     * wind field correlates properties tens of kilometers apart; a wildfire
     * spreads along much shorter, terrain-dependent distances. */
    protected abstract double linkRadiusKm();

    public final SimulationConfig buildSimulationConfig(long masterSeed, long numScenarios) {
        return SimulationConfig.newBuilder()
                .setLinkRadiusKm(linkRadiusKm())
                .setMasterSeed(masterSeed)
                .setNumScenarios(numScenarios)
                .setIntensityMin(minIntensity())
                .setIntensityMax(maxIntensity())
                .setDecayMin(minDecayRate())
                .setDecayMax(maxDecayRate())
                .build();
    }
}
