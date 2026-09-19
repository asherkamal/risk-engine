package com.riskengine.domain;

import java.time.LocalDate;

public record Policy(Property property, String perilType, double coverageLimit, double deductible,
                      LocalDate effectiveDate, LocalDate expirationDate) {

    public Policy {
        if (!expirationDate.isAfter(effectiveDate)) {
            throw new IllegalArgumentException("expirationDate must be after effectiveDate");
        }
        if (PerilRegistry.find(perilType).isEmpty()) {
            throw new IllegalArgumentException("Unknown peril type: " + perilType);
        }
    }
}
