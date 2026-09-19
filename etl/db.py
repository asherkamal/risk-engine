"""Postgres connection helper. Reads connection params from environment
variables with defaults matching db/docker-compose.yml, so the ETL scripts
work against the local Docker setup with zero configuration."""
import os

import psycopg


def get_connection():
    return psycopg.connect(
        host=os.environ.get("PGHOST", "localhost"),
        port=os.environ.get("PGPORT", "5432"),
        dbname=os.environ.get("PGDATABASE", "riskengine"),
        user=os.environ.get("PGUSER", "riskengine"),
        password=os.environ.get("PGPASSWORD", "riskengine"),
    )
