-- Initial schema for the Catastrophic Risk Simulation Engine.

CREATE EXTENSION IF NOT EXISTS postgis;

CREATE TABLE properties (
    id BIGSERIAL PRIMARY KEY,
    location GEOGRAPHY(POINT, 4326) NOT NULL,
    address TEXT,
    property_type TEXT NOT NULL,
    insured_value NUMERIC(14, 2) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Serves "which properties fall within radius X of point Y" queries at the
-- DB layer -- e.g. loading all properties in a region for a portfolio, or
-- validating portfolio coverage against a coastline band -- the same query
-- shape the C++ engine's in-memory k-d tree answers for live simulation,
-- needed here too since not every caller pulls the full portfolio into
-- memory first.
CREATE INDEX idx_properties_location_gist ON properties USING GIST (location);

CREATE TABLE policies (
    id BIGSERIAL PRIMARY KEY,
    property_id BIGINT NOT NULL REFERENCES properties(id),
    peril_type TEXT NOT NULL CHECK (peril_type IN ('hurricane', 'wildfire', 'earthquake')),
    coverage_limit NUMERIC(14, 2) NOT NULL,
    deductible NUMERIC(14, 2) NOT NULL DEFAULT 0,
    effective_date DATE NOT NULL,
    expiration_date DATE NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CHECK (expiration_date > effective_date)
);

CREATE INDEX idx_policies_property_id ON policies (property_id);

CREATE TABLE historical_events (
    id BIGSERIAL PRIMARY KEY,
    peril_type TEXT NOT NULL CHECK (peril_type IN ('hurricane', 'wildfire', 'earthquake')),
    external_id TEXT,
    event_date TIMESTAMPTZ NOT NULL,
    epicenter GEOGRAPHY(POINT, 4326) NOT NULL,
    intensity NUMERIC(6, 2),
    source TEXT NOT NULL,
    raw_data JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Same rationale as idx_properties_location_gist, applied to historical
-- storm/quake epicenters -- serves "which past events passed within radius
-- X of this portfolio" queries (e.g. plotting a coastal portfolio's actual
-- hurricane exposure history).
CREATE INDEX idx_historical_events_epicenter_gist ON historical_events USING GIST (epicenter);

CREATE TABLE simulation_runs (
    id BIGSERIAL PRIMARY KEY,
    peril_type TEXT NOT NULL CHECK (peril_type IN ('hurricane', 'wildfire', 'earthquake')),
    num_scenarios BIGINT NOT NULL,
    master_seed BIGINT NOT NULL,
    config JSONB NOT NULL,
    mean_loss NUMERIC(14, 2),
    p99_loss NUMERIC(14, 2),
    min_loss NUMERIC(14, 2),
    max_loss NUMERIC(14, 2),
    wall_clock_ms DOUBLE PRECISION,
    status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'running', 'completed', 'failed')),
    started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    completed_at TIMESTAMPTZ
);

CREATE TABLE results (
    id BIGSERIAL PRIMARY KEY,
    simulation_run_id BIGINT NOT NULL REFERENCES simulation_runs(id),
    policy_id BIGINT NOT NULL REFERENCES policies(id),
    event_date DATE NOT NULL,
    loss_amount NUMERIC(14, 2) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Serves "get this policy's simulated loss history in chronological order"
-- and "which policies were affected in this date range" -- the underwriter
-- lookup pattern of reviewing one policy's loss experience over time, or
-- auditing a specific historical period across the book.
CREATE INDEX idx_results_policy_id_event_date ON results (policy_id, event_date);
