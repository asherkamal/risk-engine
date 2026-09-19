"""Orchestrates the Step 4 ETL: real NOAA/USGS historical events plus a
synthetic property portfolio, loaded into Postgres.

Usage:
    python run_etl.py --all
    python run_etl.py --hurdat2 --min-year 2020
    python run_etl.py --usgs --start-date 2020-01-01 --end-date 2024-01-01
    python run_etl.py --portfolio --num-properties 8000
"""
import argparse
import sys

from db import get_connection
from generators.synthetic_portfolio import generate_and_load_synthetic_portfolio
from sources.hurdat2 import load_hurdat2
from sources.usgs import load_usgs_earthquakes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true", help="run every step")
    parser.add_argument("--hurdat2", action="store_true", help="load real NOAA HURDAT2 hurricane tracks")
    parser.add_argument("--usgs", action="store_true", help="load real USGS earthquake events")
    parser.add_argument("--portfolio", action="store_true", help="generate the synthetic property portfolio")
    parser.add_argument("--min-year", type=int, default=2015, help="HURDAT2: earliest storm year to load")
    parser.add_argument("--start-date", default="2015-01-01", help="USGS: query start date")
    parser.add_argument("--end-date", default="2025-01-01", help="USGS: query end date")
    parser.add_argument("--min-magnitude", type=float, default=4.0, help="USGS: minimum magnitude")
    parser.add_argument("--num-properties", type=int, default=8000, help="synthetic portfolio size")
    args = parser.parse_args()

    if not (args.all or args.hurdat2 or args.usgs or args.portfolio):
        parser.error("specify --all or one of --hurdat2 / --usgs / --portfolio")

    conn = get_connection()
    try:
        if args.all or args.hurdat2:
            print(f"Loading NOAA HURDAT2 hurricane tracks (year >= {args.min_year})...")
            n = load_hurdat2(conn, min_year=args.min_year)
            print(f"  -> inserted {n} track points into historical_events")

        if args.all or args.usgs:
            print(f"Loading USGS earthquakes ({args.start_date} to {args.end_date}, "
                  f"magnitude >= {args.min_magnitude})...")
            n = load_usgs_earthquakes(conn, args.start_date, args.end_date, args.min_magnitude)
            print(f"  -> inserted {n} events into historical_events")

        if args.all or args.portfolio:
            print(f"Generating synthetic portfolio ({args.num_properties} properties)...")
            n = generate_and_load_synthetic_portfolio(conn, num_properties=args.num_properties)
            print(f"  -> inserted {n} properties (+ {n} policies)")
    finally:
        conn.close()


if __name__ == "__main__":
    sys.exit(main())
