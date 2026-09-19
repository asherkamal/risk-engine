package com.riskengine.domain;

/**
 * Seismic shaking attenuates sharply with distance from the epicenter --
 * the highest decay rate of the three perils, over a moderate link radius.
 */
public class EarthquakePeril extends Peril {

    @Override
    public String type() {
        return "earthquake";
    }

    @Override
    protected double minIntensity() {
        return 0.5;
    }

    @Override
    protected double maxIntensity() {
        return 1.0;
    }

    @Override
    protected double minDecayRate() {
        return 0.3;
    }

    @Override
    protected double maxDecayRate() {
        return 0.6;
    }

    @Override
    protected double linkRadiusKm() {
        return 30.0;
    }
}
