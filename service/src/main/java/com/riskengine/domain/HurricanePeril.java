package com.riskengine.domain;

/**
 * Hurricane wind fields correlate damage across wide areas and decay slowly
 * with distance from the eye -- a large link radius and low decay rate
 * relative to the other perils.
 */
public class HurricanePeril extends Peril {

    @Override
    public String type() {
        return "hurricane";
    }

    @Override
    protected double minIntensity() {
        return 0.6;
    }

    @Override
    protected double maxIntensity() {
        return 1.0;
    }

    @Override
    protected double minDecayRate() {
        return 0.02;
    }

    @Override
    protected double maxDecayRate() {
        return 0.08;
    }

    @Override
    protected double linkRadiusKm() {
        return 50.0;
    }
}
