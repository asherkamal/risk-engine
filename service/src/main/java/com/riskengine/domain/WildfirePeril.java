package com.riskengine.domain;

/**
 * Wildfires spread along terrain and wind-driven paths rather than
 * radiating uniformly, so correlated damage falls off much faster with
 * distance than a hurricane's wind field, over a much shorter radius.
 */
public class WildfirePeril extends Peril {

    @Override
    public String type() {
        return "wildfire";
    }

    @Override
    protected double minIntensity() {
        return 0.4;
    }

    @Override
    protected double maxIntensity() {
        return 0.9;
    }

    @Override
    protected double minDecayRate() {
        return 0.15;
    }

    @Override
    protected double maxDecayRate() {
        return 0.4;
    }

    @Override
    protected double linkRadiusKm() {
        return 15.0;
    }
}
