package com.riskengine.api;

import java.time.LocalDate;
import java.util.List;

import com.riskengine.domain.Policy;
import com.riskengine.domain.Portfolio;
import com.riskengine.domain.Property;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotEmpty;
import jakarta.validation.constraints.Positive;

public record SimulationRequest(
        @NotBlank String perilType,
        @NotEmpty @Valid List<PropertyRequest> properties,
        @Positive long masterSeed,
        @Positive long numScenarios
) {

    /** Builds a same-day, one-year policy per submitted property -- there is
     * no persisted Policy/Property source for an ad hoc simulation request
     * like this, so effective/expiration dates are synthetic placeholders,
     * not looked up from real policy terms. */
    public Portfolio toPortfolio() {
        LocalDate effectiveDate = LocalDate.now();
        LocalDate expirationDate = effectiveDate.plusYears(1);

        List<Policy> policies = properties.stream()
                .map(p -> {
                    Property property = new Property(p.lat(), p.lon(), p.insuredValue());
                    double deductible = p.insuredValue() * 0.02;
                    return new Policy(property, perilType, p.insuredValue(), deductible,
                            effectiveDate, expirationDate);
                })
                .toList();

        return new Portfolio("submitted-portfolio", policies);
    }
}
