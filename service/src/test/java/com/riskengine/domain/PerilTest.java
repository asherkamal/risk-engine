package com.riskengine.domain;

import java.util.List;

import org.junit.jupiter.api.Test;
import riskengine.RiskEngineOuterClass.SimulationConfig;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class PerilTest {

    @Test
    void eachPerilProducesDistinctSimulationCharacteristics() {
        SimulationConfig hurricane = new HurricanePeril().buildSimulationConfig(42, 1000);
        SimulationConfig wildfire = new WildfirePeril().buildSimulationConfig(42, 1000);
        SimulationConfig earthquake = new EarthquakePeril().buildSimulationConfig(42, 1000);

        // Hurricane wind fields correlate damage over the widest area.
        assertThat(hurricane.getLinkRadiusKm())
                .isGreaterThan(earthquake.getLinkRadiusKm())
                .isGreaterThan(wildfire.getLinkRadiusKm());

        // Earthquake shaking attenuates fastest with distance.
        assertThat(earthquake.getDecayMax())
                .isGreaterThan(hurricane.getDecayMax());
    }

    @Test
    void dispatchGoesThroughTheBaseClassNotAPerPerilTypeCheck() {
        // Iterating as List<Peril> and calling the base-class method is the
        // actual proof of polymorphism: nothing here ever asks "which
        // subtype is this" or branches on peril name.
        List<Peril> perils = List.of(new HurricanePeril(), new WildfirePeril(), new EarthquakePeril());
        for (Peril peril : perils) {
            SimulationConfig config = peril.buildSimulationConfig(1, 100);
            assertThat(config.getMasterSeed()).isEqualTo(1);
            assertThat(config.getNumScenarios()).isEqualTo(100);
            assertThat(config.getIntensityMin()).isPositive();
            assertThat(config.getIntensityMax()).isGreaterThan(config.getIntensityMin());
        }
    }

    @Test
    void registryResolvesKnownTypesAndRejectsUnknownOnes() {
        assertThat(PerilRegistry.require("hurricane")).isInstanceOf(HurricanePeril.class);
        assertThat(PerilRegistry.require("wildfire")).isInstanceOf(WildfirePeril.class);
        assertThat(PerilRegistry.require("earthquake")).isInstanceOf(EarthquakePeril.class);

        assertThatThrownBy(() -> PerilRegistry.require("tornado"))
                .isInstanceOf(IllegalArgumentException.class);
    }
}
