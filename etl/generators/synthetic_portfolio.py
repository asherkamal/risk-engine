"""Generates a synthetic property portfolio with realistic lat/lon
clustering along US coastlines, since real policyholder data doesn't exist
for this project. Insured values and coordinates are synthetic; the cluster
anchor points are real coastal/seismic-risk cities so the portfolio's
footprint actually overlaps the real NOAA/USGS historical event data loaded
by sources/hurdat2.py and sources/usgs.py.
"""
import random

# (city, lat, lon, peril_type, relative weight). Hurricane clusters run
# along the Atlantic/Gulf coast (matching HURDAT2 track coverage);
# earthquake clusters sit inside USGS's DEFAULT_BBOX west-coast window.
COASTAL_CLUSTERS = [
    ("Miami, FL", 25.7617, -80.1918, "hurricane", 12),
    ("Tampa, FL", 27.9506, -82.4572, "hurricane", 10),
    ("Charleston, SC", 32.7765, -79.9311, "hurricane", 8),
    ("Myrtle Beach, SC", 33.6891, -78.8867, "hurricane", 6),
    ("Virginia Beach, VA", 36.8529, -75.9780, "hurricane", 6),
    ("New Orleans, LA", 29.9511, -90.0715, "hurricane", 10),
    ("Galveston, TX", 29.3013, -94.7977, "hurricane", 8),
    ("Outer Banks, NC", 35.5585, -75.4665, "hurricane", 5),
    ("Los Angeles, CA", 34.0522, -118.2437, "earthquake", 12),
    ("San Francisco, CA", 37.7749, -122.4194, "earthquake", 10),
    ("Seattle, WA", 47.6062, -122.3321, "earthquake", 7),
    ("Portland, OR", 45.5152, -122.6784, "earthquake", 6),
]

CLUSTER_WEIGHTS = [c[4] for c in COASTAL_CLUSTERS]
JITTER_STDDEV_DEGREES = 0.35  # roughly 30-40km around each anchor city


def generate_and_load_synthetic_portfolio(conn, num_properties: int = 8000, seed: int = 42) -> int:
    """Generates `num_properties` synthetic properties plus one policy each,
    inserts them, and returns the number of properties created."""
    rng = random.Random(seed)

    property_rows = []  # (lon, lat, address, property_type, insured_value)
    peril_types = []
    for _ in range(num_properties):
        city, center_lat, center_lon, peril_type, _ = rng.choices(
            COASTAL_CLUSTERS, weights=CLUSTER_WEIGHTS, k=1
        )[0]
        lat = rng.gauss(center_lat, JITTER_STDDEV_DEGREES)
        lon = rng.gauss(center_lon, JITTER_STDDEV_DEGREES)
        insured_value = round(rng.lognormvariate(12.0, 0.6), 2)
        property_type = rng.choices(["residential", "commercial"], weights=[85, 15])[0]

        property_rows.append((lon, lat, f"Synthetic property near {city}", property_type, insured_value))
        peril_types.append(peril_type)

    with conn.cursor() as cur:
        # psycopg3 has no execute_values helper (that's psycopg2.extras), so
        # a single multi-row INSERT...VALUES(...),(...)  RETURNING id is
        # built by hand instead -- one round trip, ids come back in the
        # same order the rows were given.
        values_sql = ",".join(
            ["(ST_SetSRID(ST_MakePoint(%s, %s), 4326)::geography, %s, %s, %s)"] * len(property_rows)
        )
        flat_params = [field for row in property_rows for field in row]
        cur.execute(
            f"""
            INSERT INTO properties (location, address, property_type, insured_value)
            VALUES {values_sql}
            RETURNING id
            """,
            flat_params,
        )
        property_ids = [row[0] for row in cur.fetchall()]

        policy_rows = []
        for property_id, peril_type, row in zip(property_ids, peril_types, property_rows):
            insured_value = row[4]
            coverage_limit = insured_value
            deductible = round(insured_value * 0.02, 2)
            policy_rows.append(
                (property_id, peril_type, coverage_limit, deductible, "2024-01-01", "2025-01-01")
            )

        policy_values_sql = ",".join(["(%s, %s, %s, %s, %s, %s)"] * len(policy_rows))
        flat_policy_params = [field for row in policy_rows for field in row]
        cur.execute(
            f"""
            INSERT INTO policies
                (property_id, peril_type, coverage_limit, deductible, effective_date, expiration_date)
            VALUES {policy_values_sql}
            """,
            flat_policy_params,
        )

    conn.commit()
    return len(property_ids)
