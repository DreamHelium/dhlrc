#!/usr/bin/env python3
"""Builds the playful catalogue (`po/dhlrc-cute/zh_CN.po`) from the plain one.

The cute pack is the whole Chinese catalogue with a playful ending added to
each translated message (喵, 捏, kaomoji, emoji), so the entire window speaks
that way rather than only a handful of labels. Short technical values and a
few literals that other code compares or parses are left alone.

Run it after the plain catalogue changes; `make_po.sh` does that for you when
python3 is available.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "dhlrc" / "zh_CN.po"
TARGET = ROOT / "dhlrc-cute" / "zh_CN.po"

# Rotated so neighbouring messages do not all end the same way.
SUFFIXES = [
    "喵~",
    "捏~",
    "喵呜~",
    "(๑•̀ㅂ•́)و✧",
    "😺",
    "喵♪",
    "(=^･ω･^=)",
    "✨",
]

# Values that name something rather than say something; they stay as they are.
KEEP = {
    "id",
    "x",
    "y",
    "z",
    "NBT",
    "Nbt",
    "cpu",
    "gpu",
    "true",
    "false",
    "Unknown",
    "Palette",
    "DataVersion",
    "Compound",
    "List",
    "BigEndian",
    "LittleEndian",
    "NetworkLittleEndian",
    "GiB",
    "MiB",
    "KiB",
    "Bytes",
    "yyyy/MM/dd HH:mm:ss.zzz",
    "BlockShowUI",
    "BlockListUI",
    "NbtReaderUI",
    "RegionModifyUI",
    "PropertyModifyUI",
    "PaletteAddUI",
    "PaletteListUI",
    "Form",
    "PushButton",
    "Debug",
}

QUOTED = re.compile(r'"((?:[^"\\]|\\.)*)"')

# A file dialog's name filter is syntax, not prose: "Sponge Schematic
# (*.schem)". A cute ending after the closing bracket would turn the pattern
# into something Qt cannot match, so these stay as they are.
FILTER = re.compile(r"\(\*[^)]*\)\s*$")


def piece_text(lines):
    """The message text of a run of `msgstr "..."` lines, as po-escaped text."""
    return "".join(QUOTED.findall("\n".join(lines)))


# Punctuation that reads oddly in front of the ending ("Cancelled.喵~"), so
# it is dropped: the ending takes the place of the full stop.
DROP_TRAILING = ".。．!！?？:：,，;；、·"


def cute(text, suffix):
    """Appends `suffix` to a message, keeping the hotkey and any ellipsis.

    A message like "Save As... (&S)" becomes "Save As喵~ (&S)...": the ending
    goes before the hotkey marker and any ellipsis, so the marker stays at the
    end of the label where Qt and the user expect it, and a menu entry keeps
    the "..." that says it opens a dialog. A trailing full stop is dropped
    instead of being left in the middle of the sentence.
    """
    tail = ""
    while text.endswith("\\n"):
        text = text[:-2]
        tail = "\\n" + tail

    # The ellipsis of a menu entry that opens a dialog stays at the very end.
    ellipsis = ""
    while True:
        stripped = text.rstrip(" ")
        if stripped.endswith("..."):
            text = stripped[:-3]
            ellipsis = "..." + ellipsis
        elif stripped.endswith("…"):
            text = stripped[:-1]
            ellipsis = "…" + ellipsis
        else:
            break
    text = text.rstrip(" ")

    # A mnemonic "( &X )" is the last thing in the label; the ending goes in
    # front of it.
    mnemonic = ""
    match = re.search(r"\s*\(&.\)$", text)
    if match:
        mnemonic = match.group(0)
        text = text[: match.start()]

    text = text.rstrip(DROP_TRAILING + " ")
    if not text:
        return text + mnemonic + ellipsis + tail
    last = text[-1]
    # A Latin word or a placeholder needs a space before the ending; CJK text
    # runs straight into it.
    ascii_word = last.isascii() and (last.isalnum() or last == "%")
    return text + (" " if ascii_word else "") + suffix + mnemonic + ellipsis + tail


def main():
    source = SOURCE.read_text(encoding="utf-8")
    blocks = source.split("\n\n")
    out = []
    added = 0
    for index, block in enumerate(blocks):
        lines = block.split("\n")
        msgstr_at = next(
            (i for i, line in enumerate(lines) if line.startswith("msgstr ")),
            None,
        )
        if msgstr_at is None:
            out.append(block)
            continue
        # The msgstr may continue on following lines.
        end = msgstr_at + 1
        while end < len(lines) and lines[end].startswith('"'):
            end += 1
        msgid_text = piece_text(lines[:msgstr_at])
        msgstr_text = piece_text(lines[msgstr_at:end])
        if (
            msgid_text
            and msgstr_text
            and msgid_text not in KEEP
            and msgstr_text not in KEEP
            and len(msgstr_text.strip("\n ")) > 1
            and not msgid_text.startswith("Project-Id")
            and not FILTER.search(msgid_text)
            and not FILTER.search(msgstr_text)
        ):
            msgstr_text = cute(msgstr_text, SUFFIXES[added % len(SUFFIXES)])
            added += 1
        lines[msgstr_at:end] = ['msgstr "%s"' % msgstr_text]
        out.append("\n".join(lines))
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_text(
        "\n\n".join(out)
        + "\n# Playful catalogue: generated from dhlrc/zh_CN.po by make_cute.py.\n",
        encoding="utf-8",
    )
    print("cute catalogue: %d messages, %d made cute" % (len(blocks), added))


if __name__ == "__main__":
    main()
