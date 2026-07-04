#!/usr/bin/env python3
"""Group ctest JUnit results by assignment ID (test names look like
"m02a03.Suite.Case") and print a per-assignment progress table in
GitHub-flavored markdown for the workflow summary."""
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

def main(path: str) -> None:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as e:
        print(f"No test results to summarize ({e}).")
        return

    stats = defaultdict(lambda: [0, 0])  # id -> [passed, total]
    for case in root.iter("testcase"):
        name = case.get("name", "")
        assignment = name.split(".", 1)[0] if "." in name else "(other)"
        failed = case.find("failure") is not None or case.find("error") is not None
        skipped = case.find("skipped") is not None
        if skipped:
            continue
        stats[assignment][1] += 1
        if not failed:
            stats[assignment][0] += 1

    print("## Assignment progress\n")
    if not stats:
        print("No tests discovered yet.")
        return
    print("| Assignment | Tests passing | Status |")
    print("|------------|--------------:|--------|")
    for assignment in sorted(stats):
        passed, total = stats[assignment]
        icon = "✅" if passed == total else ("🚧" if passed else "⬜")
        print(f"| `{assignment}` | {passed}/{total} | {icon} |")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "test-results.xml")
