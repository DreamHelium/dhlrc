#!/usr/bin/env python3
"""Validate and expand `dhlrc-resource/data/handle_block.json`.

The table teaches the material list how a block becomes the item(s) a player has
to prepare. Three kinds of rule live in it:

  * which item a block turns into (exact tables, then ordered patterns);
  * how many items one block stands for, when the block state says so
    (`units_rules`: a double slab is two slabs, snow with 8 layers is 8 snow);
  * which items put down two block positions (`pair_items`: a door, a bed, a
    tall plant).

A client jar is the authority for what exists in a version:

  * block ids        -> assets/minecraft/blockstates/<id>.json
  * item ids         -> assets/minecraft/items/<id>.json        (1.21.4+)
                        assets/minecraft/models/item/<id>.json   (older)
  * block properties -> the `variants` keys / `multipart` `when` of that
                        blockstates file

Usage:
  scripts/gen_handle_block.py check  [client.jar ...]
  scripts/gen_handle_block.py expand <client.jar>
  scripts/gen_handle_block.py fmt

`check` is the mode to run after editing the table: it reports blocks that have
no item form and no rule, rules pointing at an item the version does not have,
`units_rules` naming a property the block state does not carry, and entries that
name a block or item no supplied version knows. Exit status is non-zero when
something is wrong.

With no jars given, the ones in dhlrc's cache are used when present.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TABLE = REPO / "dhlrc-resource/data/handle_block.json"

# Tried in order when no jar is passed on the command line.
DEFAULT_JARS = [
    Path.home() / ".cache/dhlrc/client_jar/1.21.5.jar",
    Path.home() / ".cache/dhlrc/client_jar/1.21.jar",
    REPO / "temp/cache_auto/client_jar/1.21.4.jar",
]

EXACT_TABLES = ("ignore_block", "replace_single_block", "replace_multi_block")
REQUIRED = EXACT_TABLES + ("pattern_rules", "units_rules", "pair_items")


# --------------------------------------------------------------------------
# the table


def load_table() -> dict:
    data = json.loads(TABLE.read_text(encoding="utf-8"))
    if data.get("schema") != 3:
        die(f"{TABLE}: expected \"schema\": 3, found {data.get('schema')!r}")
    for key in REQUIRED:
        if key not in data:
            die(f"{TABLE}: missing {key!r}")
    for i, rule in enumerate(data["pattern_rules"]):
        for key in ("match", "action", "to"):
            if key not in rule:
                die(f"{TABLE}: pattern_rules[{i}] is missing {key!r}")
        if rule["action"] not in ("ignore", "single", "multi"):
            die(f"{TABLE}: pattern_rules[{i}] has unknown action {rule['action']!r}")
        if rule["match"].count("*") != 1:
            die(f"{TABLE}: pattern_rules[{i}] match must contain exactly one '*': {rule['match']!r}")
        targets = rule["to"] if isinstance(rule["to"], list) else [rule["to"]]
        if sum(t.count("*") for t in targets) > 1:
            die(f"{TABLE}: pattern_rules[{i}] 'to' may use '*' at most once")
        if rule["action"] == "multi" and not isinstance(rule["to"], list):
            die(f"{TABLE}: pattern_rules[{i}] action 'multi' needs a list in 'to'")
    for i, rule in enumerate(data["units_rules"]):
        for key in ("match", "property"):
            if key not in rule:
                die(f"{TABLE}: units_rules[{i}] is missing {key!r}")
        if rule["match"].count("*") > 1:
            die(f"{TABLE}: units_rules[{i}] match may use at most one '*'")
        if "units" in rule and rule["units"] < 1:
            die(f"{TABLE}: units_rules[{i}] units must be positive")
    for entry in data["pair_items"]:
        if not isinstance(entry, str) or entry.count("*") > 1:
            die(f"{TABLE}: pair_items entry may use at most one '*': {entry!r}")
    return data


def split_pattern(pattern: str) -> tuple[str, str]:
    """`*` matches one or more characters; at most one `*` is allowed."""
    if "*" not in pattern:
        return pattern, ""
    head, tail = pattern.split("*", 1)
    if "*" in tail:
        die(f"pattern may use at most one '*': {pattern!r}")
    return head, tail


def match(pattern: str, value: str) -> bool:
    if "*" not in pattern:
        return value == pattern
    prefix, suffix = split_pattern(pattern)
    if not (value.startswith(prefix) and value.endswith(suffix)):
        return False
    return len(value) > len(prefix) + len(suffix)


def resolve(block: str, table: dict) -> tuple[str, list[str] | None]:
    """Returns (kind, targets); kind is ignore/single/multi/gap.

    Blocks that are their own item are *not* mentioned in the table: this
    reports them as 'gap', and the caller accepts them when the item exists.
    """
    if block in table["ignore_block"]:
        return "ignore", None
    if block in table["replace_single_block"]:
        return "single", [table["replace_single_block"][block]]
    if block in table["replace_multi_block"]:
        return "multi", list(table["replace_multi_block"][block])
    for rule in table["pattern_rules"]:
        if not match(rule["match"], block):
            continue
        if rule["action"] == "ignore":
            return "ignore", None
        prefix, suffix = (
            split_pattern(rule["match"]) if "*" in rule["match"] else (rule["match"], "")
        )
        captured = block[len(prefix) : len(block) - len(suffix)] if suffix else block[len(prefix) :]
        targets = rule["to"] if isinstance(rule["to"], list) else [rule["to"]]
        return rule["action"], [t.replace("*", captured, 1) for t in targets]
    return "gap", None


# --------------------------------------------------------------------------
# the jars


def read_jar(path: Path) -> tuple[set[str], set[str], str]:
    blocks: set[str] = set()
    models_item: set[str] = set()
    item_defs: set[str] = set()
    with zipfile.ZipFile(path) as jar:
        for name in jar.namelist():
            if name.startswith("assets/minecraft/blockstates/") and name.endswith(".json"):
                blocks.add(fmt_id(name.rsplit("/", 1)[-1][:-5]))
            elif name.startswith("assets/minecraft/models/item/") and name.endswith(".json"):
                models_item.add(fmt_id(name.rsplit("/", 1)[-1][:-5]))
            elif name.startswith("assets/minecraft/items/") and name.endswith(".json"):
                item_defs.add(fmt_id(name.rsplit("/", 1)[-1][:-5]))
    # 1.21.4+ moved item model definitions to `items/`; that directory is then
    # the complete item id list. Before that, `models/item/` is.
    if item_defs:
        return blocks, item_defs, "items/"
    return blocks, models_item, "models/item/"


def blockstate_properties(jar_path: Path, blocks: list[str]) -> dict[str, dict[str, set[str]]]:
    """The block state properties each block carries, from its blockstates file."""
    wanted = {short(block): block for block in blocks}
    found: dict[str, dict[str, set[str]]] = {}
    with zipfile.ZipFile(jar_path) as jar:
        for name in jar.namelist():
            if not name.startswith("assets/minecraft/blockstates/") or not name.endswith(".json"):
                continue
            stem = name.rsplit("/", 1)[-1][:-5]
            if stem not in wanted:
                continue
            data = json.loads(jar.read(name))
            properties: dict[str, set[str]] = {}
            for key in data.get("variants", {}):
                for part in key.split(","):
                    if "=" in part:
                        prop, value = part.split("=", 1)
                        properties.setdefault(prop, set()).add(value)
            for part in data.get("multipart", []):
                when = part.get("when")
                if isinstance(when, dict):
                    for prop, value in when.items():
                        values = value if isinstance(value, list) else [value]
                        properties.setdefault(prop, set()).update(str(v) for v in values)
            found[wanted[stem]] = properties
    return found


def fmt_id(name: str) -> str:
    return f"minecraft:{name}"


def short(value: str) -> str:
    return value.split(":", 1)[-1]


def discover_jars(explicit: list[str]) -> list[Path]:
    if explicit:
        jars = [Path(p) for p in explicit]
        for jar in jars:
            if not jar.exists():
                die(f"no such jar: {jar}")
        return jars
    jars = [p for p in DEFAULT_JARS if p.exists()]
    if not jars:
        die(
            "no client jar to check against; pass one explicitly, e.g.\n"
            "  scripts/gen_handle_block.py check ~/.cache/dhlrc/client_jar/1.21.5.jar"
        )
    return jars


# --------------------------------------------------------------------------
# modes


def check(jars: list[Path]) -> int:
    table = load_table()
    failures = 0

    versions = [(p, *read_jar(p)) for p in jars]
    all_blocks = set().union(*(b for _, b, _, _ in versions))
    all_items = set().union(*(i for _, _, i, _ in versions))

    # Entries naming a block / item that no supplied version knows at all.
    stale: list[str] = []
    for block in table["ignore_block"]:
        if block not in all_blocks:
            stale.append(f"ignore_block: {block}")
    for block, target in table["replace_single_block"].items():
        if block not in all_blocks:
            stale.append(f"replace_single_block key: {block}")
        if target not in all_items:
            stale.append(f"replace_single_block target: {block} -> {target}")
    for block, targets in table["replace_multi_block"].items():
        if block not in all_blocks:
            stale.append(f"replace_multi_block key: {block}")
        for target in targets:
            if target not in all_items:
                stale.append(f"replace_multi_block target: {block} -> {target}")

    # Patterns that never fire for any supplied version are probably typos.
    unused_patterns: list[str] = []
    for rule in table["pattern_rules"]:
        if not any(match(rule["match"], block) for block in all_blocks):
            unused_patterns.append(rule["match"])

    # A pair item must be matched by at least one existing item id.
    pair_problems: list[str] = []
    for entry in table["pair_items"]:
        if not any(match(entry, item) for item in all_items):
            pair_problems.append(f"pair_items: {entry} matches no item")

    # A units rule must fire on a block somewhere, and its property has to be
    # real in every version that has one of those blocks (a rule for a block added
    # in a newer version simply does not apply to an older jar).
    units_problems: list[str] = []
    for rule in table["units_rules"]:
        matched_anywhere = False
        for jar, blocks, _, _ in versions:
            matched = sorted(block for block in blocks if match(rule["match"], block))
            if not matched:
                continue
            matched_anywhere = True
            properties = blockstate_properties(jar, matched)
            declared = {
                block: props for block, props in properties.items() if rule["property"] in props
            }
            if not declared:
                units_problems.append(
                    f"{jar.name}: units rule {rule['match']} names property "
                    f"{rule['property']!r}, which none of its blocks carries"
                )
                continue
            if "value" in rule:
                wanted = str(rule["value"])
                if not any(
                    wanted in props[rule["property"]] for props in declared.values()
                ):
                    units_problems.append(
                        f"{jar.name}: units rule {rule['match']} expects "
                        f"{rule['property']}={wanted!r}, which no matched block state has"
                    )
        if not matched_anywhere:
            units_problems.append(f"units rule {rule['match']} matches no block")

    for jar, blocks, items, source in versions:
        print(f"=== {jar.name}: {len(blocks)} blocks, {len(items)} items ({source}) ===")

        gaps: list[str] = []
        bad_targets: list[str] = []
        overlaps: list[tuple[str, list[str]]] = []
        counts = {"identity": 0, "single": 0, "multi": 0, "ignore": 0}

        for block in sorted(blocks):
            kind, targets = resolve(block, table)
            if kind == "gap":
                if block in items:
                    counts["identity"] += 1
                else:
                    gaps.append(block)
                continue
            counts[kind] += 1
            for target in targets or []:
                if target not in items:
                    bad_targets.append(f"{block} -> {target}")

            if kind != "ignore":
                hits = [
                    rule["match"]
                    for rule in table["pattern_rules"]
                    if match(rule["match"], block)
                ]
                if len(hits) > 1:
                    overlaps.append((block, hits))

        print(
            "  resolve: identity={identity} single={single} multi={multi} ignore={ignore}".format(
                **counts
            )
        )
        for label, values in (
            ("gaps (no item form, no rule)", gaps),
            ("rules pointing at a missing item", bad_targets),
        ):
            if values:
                failures += len(values)
                print(f"  ERROR {label}: {len(values)}")
                for value in values:
                    print(f"    {value}")
        for block, hits in overlaps:
            print(f"  note: {block} matches several patterns (first wins): {hits}")

    for label, values in (
        ("entries no supplied version knows", stale),
        ("pair_items entries that match nothing", pair_problems),
        ("units_rules that do not fit a block state", units_problems),
    ):
        if values:
            failures += len(values)
            print(f"ERROR {label}: {len(values)}")
            for value in values:
                print(f"  {value}")
    if unused_patterns:
        print(f"note: patterns that match nothing here: {unused_patterns}")

    print()
    if failures:
        print(f"FAILED: {failures} problem(s)")
        return 1
    print("OK: no gaps, no dangling targets, no misfit units rules")
    return 0


def expand(jar_path: Path) -> int:
    table = load_table()
    blocks, items, source = read_jar(jar_path)
    resolved: dict[str, list[str]] = {}
    ignored: list[str] = []
    gaps: list[str] = []
    for block in sorted(blocks):
        kind, targets = resolve(block, table)
        if kind == "ignore":
            ignored.append(block)
        elif kind == "gap":
            if block in items:
                resolved[block] = [block]
            else:
                gaps.append(block)
        else:
            resolved[block] = targets
    print(
        json.dumps(
            {
                "version_jar": jar_path.name,
                "item_source": source,
                "block_to_items": resolved,
                "ignored": ignored,
                "pair_items": table["pair_items"],
                "units_rules": table["units_rules"],
                "gaps": gaps,
            },
            indent=4,
            sort_keys=True,
        )
    )
    return 0 if not gaps else 1


def fmt() -> int:
    """Rewrite the table with stable ordering, so diffs stay readable."""
    data = load_table()
    ordered = {k: data[k] for k in ("_note", "schema") if k in data}
    ordered["ignore_block"] = sorted(data["ignore_block"])
    ordered["replace_single_block"] = dict(sorted(data["replace_single_block"].items()))
    ordered["replace_multi_block"] = dict(sorted(data["replace_multi_block"].items()))
    ordered["units_rules"] = data["units_rules"]
    ordered["pair_items"] = sorted(data["pair_items"])
    ordered["pattern_rules"] = data["pattern_rules"]
    TABLE.write_text(json.dumps(ordered, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"rewrote {TABLE.relative_to(REPO)}")
    return 0


def die(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    sys.exit(2)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", nargs="?", default="check", choices=("check", "expand", "fmt"))
    parser.add_argument("jars", nargs="*", help="client jars to check against")
    args = parser.parse_args(argv)

    if args.mode == "fmt":
        return fmt()
    if args.mode == "expand":
        if len(args.jars) != 1:
            die("expand needs exactly one client jar")
        return expand(discover_jars(args.jars)[0])
    return check(discover_jars(args.jars))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
