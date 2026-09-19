package com.riskengine.domain;

import java.util.List;

public record Portfolio(String name, List<Policy> policies) {

    public Portfolio {
        policies = List.copyOf(policies);
        if (policies.isEmpty()) {
            throw new IllegalArgumentException("portfolio must contain at least one policy");
        }
    }
}
