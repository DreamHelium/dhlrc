# Region Loading Order

A file that holds several regions has no meaningful order of its own: the
program that wrote it may keep them in a hash table, so the order can change
from one load to the next. The **Region Loading Order** setting decides how
that file's regions are presented (the "add region" chooser, the region list
and the loading progress) and the order they are loaded in.

| Setting | Meaning |
| --- | --- |
| File Order | Keep the order the file reports. It may differ between runs; this is the old behaviour. |
| Natural Order | Sort by name, with embedded numbers compared as numbers: `2` before `10`. |
| Custom Rule | Sort by the wildcard patterns below. |

## Custom rule syntax

The rule is a list of wildcard patterns separated by commas or semicolons.
Each region name is compared against the patterns **in order**, and the first
one it matches decides its group. Regions in an earlier group come first.
Inside a group the names keep their natural order.

- `*` matches any run of characters (including none).
- `?` matches exactly one character.
- Matching is case-sensitive and applies to the region name only, without the
  file extension.
- Names that match no pattern are kept, in natural order, after the matched
  ones, so a rule never hides a region.
- An empty rule, or one with no usable pattern, falls back to Natural Order.

## Examples

For regions named `1a`, `2b`, `10c`, `part2`, `part10`:

| Rule | Result |
| --- | --- |
| `2*, 1*` | `2b`, `1a`, `10c`, `part2`, `part10` |
| `part*` | `part2`, `part10`, `1a`, `2b`, `10c` |
| `part1*, part2*` | `part10`, `part2`, `1a`, `2b`, `10c` |
| `*rail*` | all names containing "rail" first, then the rest |
| `?-?` | names like `a-1`; everything else follows |

## Trying a rule

The settings row has a **Preview...** button. It lists a sample of region
names (the regions of the last multi-region file that was opened, or a small
example) and shows the sorted result while the rule is edited, so a rule can be
checked before any file is loaded. The "add region" chooser shows the same
order controls and re-sorts its list as soon as they change.
