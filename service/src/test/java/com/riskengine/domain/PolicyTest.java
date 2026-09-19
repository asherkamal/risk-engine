package com.riskengine.domain;

import java.time.LocalDate;
import java.util.List;

import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class PolicyTest {

    private final Property property = new Property(25.76, -80.19, 500000.0);

    @Test
    void rejectsExpirationOnOrBeforeEffectiveDate() {
        LocalDate effective = LocalDate.of(2024, 1, 1);

        assertThatThrownBy(() -> new Policy(property, "hurricane", 500000.0, 10000.0,
                effective, effective))
                .isInstanceOf(IllegalArgumentException.class);

        assertThatThrownBy(() -> new Policy(property, "hurricane", 500000.0, 10000.0,
                effective, effective.minusDays(1)))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void rejectsUnknownPerilType() {
        LocalDate effective = LocalDate.of(2024, 1, 1);
        assertThatThrownBy(() -> new Policy(property, "tornado", 500000.0, 10000.0,
                effective, effective.plusYears(1)))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void acceptsValidPolicy() {
        LocalDate effective = LocalDate.of(2024, 1, 1);
        Policy policy = new Policy(property, "hurricane", 500000.0, 10000.0,
                effective, effective.plusYears(1));
        assertThat(policy.property()).isEqualTo(property);
        assertThat(policy.perilType()).isEqualTo("hurricane");
    }

    @Test
    void portfolioRejectsEmptyPolicyList() {
        assertThatThrownBy(() -> new Portfolio("empty", List.of()))
                .isInstanceOf(IllegalArgumentException.class);
    }
}
