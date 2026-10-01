#!/usr/bin/env python3

from pathlib import Path
from urllib.parse import unquote
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

PRIMARY_DOCS = [
    ROOT / "README.md",
    ROOT / "CURRENT_STATE.md",
    ROOT / "docs/README.md",
    ROOT / "docs/api/README.md",
    ROOT / "docs/architecture/README.md",
    ROOT / "examples/README.md",
]

REQUIRED_PATHS = PRIMARY_DOCS + [
    ROOT / "include/solana/delivery.h",
    ROOT / "include/solana/discovery.h",
    ROOT / "include/solana/ingress.h",
    ROOT / "examples/delivery_local_acceptance.c",
    ROOT / "examples/discovery_refresh.c",
    ROOT / "examples/ingress_decode.c",
]

LINK_RE = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})[ \t]+\S")


def markdown_files():
    for path in ROOT.rglob("*.md"):
        relative = path.relative_to(ROOT)
        if any(part in {".git", "build"} for part in relative.parts):
            continue
        yield path


def add_error(errors, message):
    errors.append(message)


def validate_required(errors):
    for path in REQUIRED_PATHS:
        if not path.is_file():
            add_error(
                errors,
                f"missing required file: {path.relative_to(ROOT)}",
            )


def validate_structure(errors):
    for path in PRIMARY_DOCS:
        if not path.is_file():
            continue

        headings = []
        in_fence = False

        for number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(),
            1,
        ):
            if line.lstrip().startswith("```"):
                in_fence = not in_fence
                continue

            if in_fence:
                continue

            match = HEADING_RE.match(line)
            if match:
                headings.append((number, len(match.group(1))))

        if in_fence:
            add_error(
                errors,
                f"{path.relative_to(ROOT)}: unclosed fenced code block",
            )

        h1_count = sum(level == 1 for _, level in headings)
        if h1_count != 1:
            add_error(
                errors,
                f"{path.relative_to(ROOT)}: expected one H1, found {h1_count}",
            )

        previous = 0
        for number, level in headings:
            if previous and level > previous + 1:
                add_error(
                    errors,
                    f"{path.relative_to(ROOT)}:{number}: "
                    f"heading jumps H{previous} -> H{level}",
                )
            previous = level


def link_target(raw):
    raw = raw.strip()
    if raw.startswith("<") and ">" in raw:
        return raw[1:raw.index(">")]
    return raw.split(None, 1)[0] if raw else ""


def validate_links(errors):
    root = ROOT.resolve()

    for path in markdown_files():
        text = path.read_text(encoding="utf-8")

        for match in LINK_RE.finditer(text):
            raw = link_target(match.group(1))

            if not raw or raw.startswith(
                ("http://", "https://", "mailto:", "#")
            ):
                continue

            target = unquote(raw.split("#", 1)[0])
            if not target:
                continue

            resolved = (path.parent / target).resolve()
            line = text.count("\n", 0, match.start()) + 1

            try:
                resolved.relative_to(root)
            except ValueError:
                add_error(
                    errors,
                    f"{path.relative_to(ROOT)}:{line}: "
                    f"link escapes repository: {raw}",
                )
                continue

            if not resolved.exists():
                add_error(
                    errors,
                    f"{path.relative_to(ROOT)}:{line}: "
                    f"broken relative link: {raw}",
                )


def main():
    errors = []
    validate_required(errors)
    validate_structure(errors)
    validate_links(errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("Documentation validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
