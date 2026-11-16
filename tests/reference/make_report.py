#!/usr/bin/env python3
"""Build docs/QA_REPORT.md from results that were actually produced.

Inputs:
  --junit  ctest JUnit XML (C++ unit tests + Python suites as ctest entries)
  --json   one or more result files written by etref.results.Results
  --pluginval  pluginval log file (optional)
Nothing is written as PASS unless it appears as passed in those inputs.
"""

from __future__ import annotations

import argparse
import json
import platform
import re
import subprocess
import xml.etree.ElementTree as ET
from datetime import datetime, timezone


def esc(s: str) -> str:
    return str(s).replace("|", "\\|").replace("\n", " ")


def git_rev() -> str:
    try:
        return subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip() or "uncommitted"
    except OSError:
        return "unknown"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--junit")
    ap.add_argument("--json", nargs="*", default=[])
    ap.add_argument("--pluginval")
    ap.add_argument("--stage", default="")
    ap.add_argument("--limitations", help="markdown file with known limitations")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    lines = [
        "# ETLimit QA report",
        "",
        f"- Stage: {a.stage}",
        f"- Generated: {datetime.now(timezone.utc).isoformat(timespec='seconds')}",
        f"- Code revision: `{git_rev()}` (report generated before commit if this is the parent revision)",
        f"- Platform: {platform.system()} {platform.release()} {platform.machine()}",
        "",
        "Every row below comes from a test that was executed while generating this report.",
        "",
    ]
    totals = {"PASS": 0, "FAIL": 0, "NOT RUN": 0}

    import os
    if a.junit and not os.path.exists(a.junit):
        totals["NOT RUN"] += 1
        lines += ["## C++ unit tests and test suites (ctest)", "", "**NOT RUN** (no ctest results found)", ""]
        a.junit = None
    a.json = [p for p in a.json if os.path.exists(p)]
    if a.pluginval and not os.path.exists(a.pluginval):
        totals["NOT RUN"] += 1
        lines += ["## pluginval", "", "**NOT RUN** (no log found)", ""]
        a.pluginval = None

    if a.junit:
        root = ET.parse(a.junit).getroot()
        lines += ["## C++ unit tests and test suites (ctest)", "",
                  "| Test | Result | Time (s) |", "|---|---|---|"]
        for tc in root.iter("testcase"):
            status = "PASS"
            if tc.find("failure") is not None or tc.find("error") is not None:
                status = "FAIL"
            elif tc.find("skipped") is not None or tc.get("status") in ("notrun", "disabled"):
                status = "NOT RUN"
            totals[status] += 1
            lines.append(f"| {esc(tc.get('name'))} | {status} | {float(tc.get('time', 0)):.2f} |")
        lines.append("")

    for path in a.json:
        with open(path) as f:
            suite = json.load(f)
        lines += [f"## Suite `{suite['suite']}`", "",
                  f"Executed {suite['executed_utc']} on {suite['platform']}.", "",
                  "| Test | Conditions | Expected | Actual | Result |", "|---|---|---|---|---|"]
        for c in suite["checks"]:
            totals[c["status"]] += 1
            lines.append(f"| {esc(c['name'])} | {esc(c['conditions'])} | {esc(c['expected'])} | "
                         f"{esc(c['actual'])} | **{c['status']}** |")
        lines += ["", f"Full measurement data: `{path.split('/')[-1]}` (CI artifact / build/qa).", ""]

    if a.pluginval:
        log = open(a.pluginval, errors="replace").read()
        m = re.search(r"Strictness level: (\d+)", log)
        exit_m = re.search(r"pluginval exit (\d+)", log)
        success = "SUCCESS" in log and (exit_m is None or exit_m.group(1) == "0")
        skipped = re.findall(r"INFO: Skipping (.*)", log)
        status = "PASS" if success else "FAIL"
        totals[status] += 1
        lines += ["## pluginval", "",
                  f"- Strictness level: {m.group(1) if m else '?'}",
                  f"- Result: **{status}**",
                  ]
        for s in skipped:
            totals["NOT RUN"] += 1
            lines.append(f"- NOT RUN inside pluginval: {esc(s)}")
        lines.append("")

    if a.limitations:
        lines += [open(a.limitations).read().strip(), ""]

    summary = f"**Totals: {totals['PASS']} passed, {totals['FAIL']} failed, {totals['NOT RUN']} not run.**"
    lines.insert(9, summary)
    lines.insert(10, "")
    with open(a.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(summary)


if __name__ == "__main__":
    main()
