package com.riskengine.domain;

/** The outcome of running a portfolio through the simulation engine for one peril. */
public record LossEvent(String perilType, long scenarioCount, double meanLoss, double p99Loss,
                         double minLoss, double maxLoss, double wallClockMs, int threadsUsed) {
}
