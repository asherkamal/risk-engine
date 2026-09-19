"""Loads real Atlantic hurricane track data from NOAA's HURDAT2 database
into the historical_events table.

HURDAT2 is plain text, not JSON/CSV-with-header: it's a sequence of storms,
each a header line (storm id, name, number of track records) followed by
that many track-point lines (date, time, status, lat, lon, max wind, min
pressure, ...). Format reference:
https://www.nhc.noaa.gov/data/hurdat/hurdat2-format-atl-1851-2021.pdf
"""
import re
from datetime import datetime, timezone

import requests

INDEX_URL = "https://www.nhc.noaa.gov/data/hurdat/"

# NOAA renames the "current" Atlantic file every time they publish an
# update (the filename embeds a coverage year and build date, e.g.
# hurdat2-1851-2025-091226.txt), and keeps every prior version linked on
# the same index page -- hardcoding either a filename or "take the first/
# last link" would be fragile. Instead we scrape the index page for every
# link matching the un-suffixed Atlantic naming pattern (distinct from the
# older archived "-atl-" variants and the separate Pacific "-nepac-" file)
# and pick the one with the highest (coverage_year, build_date).
ATLANTIC_FILE_PATTERN = re.compile(r'href="(hurdat2-1851-(\d{4})-(\d+)\.txt)"')


def _parse_build_date(build_str: str) -> tuple:
    """Build-date suffixes are inconsistently either 6 digits (MMDDYY) or
    8 digits (MMDDYYYY); normalize both to a comparable (year, month, day)."""
    month, day = int(build_str[0:2]), int(build_str[2:4])
    year = 2000 + int(build_str[4:6]) if len(build_str) == 6 else int(build_str[4:8])
    return (year, month, day)


def find_latest_atlantic_file_url() -> str:
    resp = requests.get(INDEX_URL, timeout=30)
    resp.raise_for_status()
    matches = ATLANTIC_FILE_PATTERN.findall(resp.text)
    if not matches:
        raise RuntimeError(
            f"Could not find an Atlantic HURDAT2 file link on {INDEX_URL} "
            "(NOAA may have changed their page layout)"
        )
    filename, _coverage_year, _build_str = max(
        matches, key=lambda m: (int(m[1]), _parse_build_date(m[2]))
    )
    return INDEX_URL + filename


def _parse_lat(raw: str) -> float:
    value = float(raw[:-1])
    return -value if raw[-1] == "S" else value


def _parse_lon(raw: str) -> float:
    value = float(raw[:-1])
    return -value if raw[-1] == "W" else value


def parse_hurdat2(text: str, min_year: int):
    """Yields one dict per track point for storms in `min_year` or later.

    Fields: external_id, name, event_date (UTC datetime), lat, lon,
    max_wind_kt, min_pressure_mb.
    """
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    i = 0
    while i < len(lines):
        header = [f.strip() for f in lines[i].split(",")]
        storm_id, name, num_records = header[0], header[1], int(header[2])
        i += 1

        storm_year = int(storm_id[4:8])
        for _ in range(num_records):
            if storm_year >= min_year:
                fields = [f.strip() for f in lines[i].split(",")]
                date_str, time_str = fields[0], fields[1]
                event_date = datetime.strptime(date_str + time_str, "%Y%m%d%H%M").replace(
                    tzinfo=timezone.utc
                )
                yield {
                    "external_id": storm_id,
                    "name": name,
                    "event_date": event_date,
                    "lat": _parse_lat(fields[4]),
                    "lon": _parse_lon(fields[5]),
                    "max_wind_kt": float(fields[6]),
                    "min_pressure_mb": float(fields[7]) if fields[7] not in ("", "-999") else None,
                }
            i += 1


def load_hurdat2(conn, min_year: int = 2015) -> int:
    """Downloads the current Atlantic HURDAT2 file, parses track points from
    `min_year` onward (kept recent to bound row count for a demo dataset --
    full history since 1851 is tens of thousands of points), and inserts
    them into historical_events. Returns the number of rows inserted."""
    url = find_latest_atlantic_file_url()
    resp = requests.get(url, timeout=60)
    resp.raise_for_status()

    records = list(parse_hurdat2(resp.text, min_year))

    with conn.cursor() as cur:
        cur.executemany(
            """
            INSERT INTO historical_events
                (peril_type, external_id, event_date, epicenter, intensity, source, raw_data)
            VALUES
                ('hurricane', %(external_id)s, %(event_date)s,
                 ST_SetSRID(ST_MakePoint(%(lon)s::float8, %(lat)s::float8), 4326)::geography,
                 %(max_wind_kt)s, 'NOAA_HURDAT2',
                 jsonb_build_object('name', %(name)s::text, 'min_pressure_mb', %(min_pressure_mb)s::numeric))
            """,
            records,
        )
    conn.commit()
    return len(records)
