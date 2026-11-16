"""Collects test results in a uniform format for the QA report.

Every entry records what was actually executed and measured. A check that
was not run is never written as passed.
"""

from __future__ import annotations

import json
import os
import platform
import sys
from datetime import datetime, timezone


class Results:
    def __init__(self, suite: str):
        self.suite = suite
        self.checks: list[dict] = []

    def add(self, name: str, conditions: str, expected: str, actual: str, passed: bool | None,
            data: dict | None = None):
        """passed=None means the check could not be executed (status NOT RUN)."""
        status = "NOT RUN" if passed is None else ("PASS" if passed else "FAIL")
        self.checks.append({
            "name": name,
            "conditions": conditions,
            "expected": expected,
            "actual": actual,
            "status": status,
            "data": data or {},
        })
        print(f"[{status}] {name} | {conditions} | expected {expected} | actual {actual}")

    @property
    def all_passed(self) -> bool:
        """True if nothing failed and at least one check actually ran.
        NOT RUN checks do not fail the suite but are listed in the report."""
        ran = [c for c in self.checks if c["status"] != "NOT RUN"]
        return bool(ran) and all(c["status"] == "PASS" for c in ran)

    def write(self, path: str):
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w") as f:
            json.dump({
                "suite": self.suite,
                "executed_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                "platform": f"{platform.system()} {platform.machine()} / Python {sys.version.split()[0]}",
                "checks": self.checks,
            }, f, indent=2)
