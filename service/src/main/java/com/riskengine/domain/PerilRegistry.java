package com.riskengine.domain;

import java.util.Map;
import java.util.Optional;

/**
 * Resolves a peril type string (as stored on a Policy row) to its {@link
 * Peril} instance. This lookup is the only place that deals with type
 * strings directly; once resolved, all peril-specific behavior flows
 * through {@link Peril}'s virtual methods, not further branching here.
 */
public final class PerilRegistry {

    private static final Map<String, Peril> BY_TYPE = Map.of(
            "hurricane", new HurricanePeril(),
            "wildfire", new WildfirePeril(),
            "earthquake", new EarthquakePeril()
    );

    private PerilRegistry() {
    }

    public static Optional<Peril> find(String type) {
        return Optional.ofNullable(BY_TYPE.get(type));
    }

    public static Peril require(String type) {
        return find(type).orElseThrow(
                () -> new IllegalArgumentException("Unknown peril type: " + type));
    }
}
