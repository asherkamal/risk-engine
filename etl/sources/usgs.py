"""Loads real earthquake events from the USGS earthquake catalog (FDSN
event web service) into the historical_events table.

API docs: https://earthquake.usgs.gov/fdsnws/event/1/
"""
from datetime import datetime, timezone

import requests

API_URL = "https://earthquake.usgs.gov/fdsnws/event/1/query"

# West Coast bounding box: where the synthetic portfolio's earthquake-zone
# properties are clustered (see generators/synthetic_portfolio.py), so the
# real historical events actually overlap the synthetic portfolio's
# footprint rather than being an unrelated global sample.
DEFAULT_BBOX = {"minlatitude": 32.0, "maxlatitude": 49.0, "minlongitude": -125.0, "maxlongitude": -114.0}


def fetch_earthquakes(start_date: str, end_date: str, min_magnitude: float = 4.0, bbox=None):
    params = {
        "format": "geojson",
        "starttime": start_date,
        "endtime": end_date,
        "minmagnitude": min_magnitude,
        **(bbox or DEFAULT_BBOX),
    }
    resp = requests.get(API_URL, params=params, timeout=60)
    resp.raise_for_status()
    return resp.json()["features"]


def load_usgs_earthquakes(conn, start_date: str = "2015-01-01", end_date: str = "2025-01-01",
                            min_magnitude: float = 4.0) -> int:
    """Fetches real USGS earthquake events in the given window/magnitude
    threshold/region and inserts them into historical_events. Returns the
    number of rows inserted."""
    features = fetch_earthquakes(start_date, end_date, min_magnitude)

    records = []
    for feature in features:
        props = feature["properties"]
        lon, lat, depth_km = feature["geometry"]["coordinates"]
        event_date = datetime.fromtimestamp(props["time"] / 1000.0, tz=timezone.utc)
        records.append(
            {
                "external_id": feature["id"],
                "event_date": event_date,
                "lat": lat,
                "lon": lon,
                "magnitude": props["mag"],
                "place": props.get("place"),
                "depth_km": depth_km,
            }
        )

    with conn.cursor() as cur:
        cur.executemany(
            """
            INSERT INTO historical_events
                (peril_type, external_id, event_date, epicenter, intensity, source, raw_data)
            VALUES
                ('earthquake', %(external_id)s, %(event_date)s,
                 ST_SetSRID(ST_MakePoint(%(lon)s::float8, %(lat)s::float8), 4326)::geography,
                 %(magnitude)s, 'USGS',
                 jsonb_build_object('place', %(place)s::text, 'depth_km', %(depth_km)s::numeric))
            """,
            records,
        )
    conn.commit()
    return len(records)
