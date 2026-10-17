# How to Use dhlrc

dhlrc (also called Minecraft Structure Modifier) reads Minecraft NBT and region
files, shows what is inside them, changes a region's base data, and writes the
result out again. This article covers only the windows you will actually use.

## The Main Window and the Sidebar

The left side lists the program's pages, in this order:

| Entry | What it does |
| --- | --- |
| NBT File Reader | A popup listing the open NBT readers (named after the file each read) with an option to open a new reader tab or a new reader window. |
| Manage Region | Opens the list of regions loaded in memory. |
| Region Reader/Modifier | Picks one region and opens a reader for it. |
| Settings | Opens the settings dialog. |
| Program Directories | Lists the directories the program uses. |

Every entry opens with a single click (Enter does the same from the keyboard);
the "Search..." box above the list filters the list as you type. An entry opens
as a tab on the right. Tabs can be closed and dragged to reorder, and
double-clicking a tab title tears the page off into a window of its own (the
"Return to tabs" button in that window puts the page back). Closing the last tab
brings the "Welcome!" page back; it offers "Quick Start", "Program Directories",
"NBT Reader" and "Add Files to Regions".

## Reading an NBT File

1. Click "NBT File Reader", or click "NBT Reader" on the welcome page. The
   popup lists the readers already open; "New Reader Tab" and "New Reader
   Window" open another one, in a tab or in a window of its own. A reader's
   tab and window are named after the file it reads.
2. Drag one NBT file onto the window. Dropping several files at once is refused
   with "Multiple files dropped!".
3. The file is read in the background: a progress bar runs while it is, and
   the window stays usable. The NBT tree is shown when it is ready. A file that
   cannot be read shows the codec's own explanation in an error row above the
   window instead of a broken tree.

The reader uses the Java NBT codec that ships with the program. Without it, the
window says "The NBT codec is not available.".

## Managing Regions

The "Manage Region" tab holds the regions loaded in memory. Nothing is written
back to your files until you save.

- Add files with "Add", with "Add Files to Regions" on the welcome page, or by
  dropping files onto the window. The file dialog is "Select Files"; loading
  runs in the background.
- Each row shows a display name, a grey UUID, "Added: ...", and reader, rename,
  remove and save buttons. The reader button opens the region reader (see
  below); asking for it again shows the tab that is already open for that
  region. Rename asks for a new name ("Input a New Name") and changes the
  region's own name.
- A region is locked while the program works on it (for example while saving).
  A locked row shows "Locked!" and its buttons are disabled. It unlocks by
  itself when the work finishes, and while locked it cannot be renamed, removed
  or selected. An editor that is already open for the region pauses as well:
  its fields are disabled and a line at the top says why, so a form that is
  being typed is neither refused nor lost.
- Tick "Select" to show checkboxes and a second toolbar. "Select All" ticks or
  unticks every row, the label counts the selection, and "Remove Selected" and
  "Save Selected" act on it. Locked regions are kept, with a warning.
- Saving asks for the format in "Select Save Format". If a format can write
  several regions into one file and more than one region is selected, a "Use
  MultiFunc?" question appears: yes writes one file, no writes one file per
  region. Then pick a file ("Select File") or a directory ("Select Directory").

## The Region Reader/Modifier

1. Click "Region Reader/Modifier" for a popup listing the regions loaded in
   memory, or press the reader button on a region's row. The popup marks the
   regions whose reader is already open with a check mark; picking one opens
   its reader, or brings the open one forward.
2. The region opens in a tab named after it — "Region Reader/Modifier: <name>" —
   so several readers can be told apart, and the page repeats the name and UUID
   at the top. Asking for the same region again brings that tab forward instead
   of opening a second one. The reader first downloads that version's language
   file.
3. The top shows the region's base data: size and coordinates, DataVersion,
   version name, create and modify time, author, name and description.
4. Type x, y and z under "Please enter the coordination of the block:". When all
   three are valid, "The information is:" lists the block's name, translation
   name, index, palette entry and properties.
5. "Modify Region" opens a "Region Modifier" tab for editing time, description,
   offset, author, base name, region name and DataVersion. "Show All Entities"
   and "Show Entity NBT" open NBT views, "List blocks" and "Show blocks" open
   the block list and the block view, and "3D Preview" opens the 3D view.
   "Modify Property" is available for blocks with properties. Choosing "Every
   block with this palette entry" lists the blocks that would be changed; the
   list is looked up only when that option is selected, because finding it means
   reading every block of the region.
6. Everything opened from a reader belongs to it: closing the reader closes the
   block list, the block view, the entity NBT view, the 3D view, the "Region
   Modifier" tab and the "Change History" tab as well. Keeping a reader open
   does not lock the region — "Locked!" only means a loading or saving job is
   working on it.
7. "Change History" opens a "Change History: <name>" tab listing the changes
   made to this region, newest first: base data fields, the region's own name,
   and block or palette properties. Selecting one shows its details underneath:
   the time and the old and new values ("facing: east -> west"). "Undo Selected"
   takes one change back, "Undo All" reverts from the newest one down; both are
   disabled while a job holds the region. A change that cannot be undone stays
   in the list, so nothing disappears silently.

## Settings

Click "Settings" for a page-based dialog with the pages "General",
"Default", "Game", "Reader", "Notifications", "3D Preview" and "Manage". The
last page holds options provided by the loaded modules. Each module is one
collapsed fold; opening it shows its "reading" and "writing" folds, which open
to the options themselves, so a module with many options does not push the next
one off the page. Every page shows the configuration file path and a button that
opens its folder.

The buttons are "Restore Defaults", "Reload", "Apply", "OK" and "Cancel".
"Reload" re-reads the file and discards your edits; "Apply" saves without
closing the dialog. The settings you are most likely to want:

- Application Memory Limit (with its unit), and Cache Directory.
- Try Other Modules when Loading Fails: a file is read by the module its
  extension points at, and by the others when that fails. A file that loads as
  something other than what its name says is reported at the end instead of
  being refused.
- Cute Language Pack: use the playful catalogue (喵, 捏, kaomoji, emoji) instead
  of the plain one. It is read when the program starts, so tick it, restart,
  and the whole window speaks that way.
- Multi-Region Display Name, whose "Preview..." button shows a live example.
- Block Name Language and Default Show Option, for block names.
- Download the Client Jar Automatically, which the 3D preview needs.

## Program Directories

"Program Directories" opens a window of its own, not a tab. It lists the three
places the program uses, and the files inside each of them, in a tree with
"Name", "Size" and "Path" columns:

- "Program" — the program's folder, with `region_module/`, `load_module/` and
  `locale/` inside it.
- "Configuration" — the folder holding `config.toml`, which holds every setting.
- "Cache" — downloaded client jars (`client_jar/`), resource packs and the
  resources read from them.

Double-click a row to open it in the file manager; "Open", "Refresh" and "Close"
do the same by button, and right-clicking a row offers "Open" and "Copy Path".
Where those folders actually are:

| What | Where |
| --- | --- |
| Configuration | `~/.config/dhlrc/config.toml`, or `$XDG_CONFIG_HOME/dhlrc/config.toml` when that variable is set; on Windows `%APPDATA%\dhlrc\config.toml`. |
| Cache | `~/.cache/dhlrc`, or `$XDG_CACHE_HOME/dhlrc`; on Windows `%LOCALAPPDATA%\dhlrc`; on macOS `~/Library/Caches/dhlrc`. |
| Program and modules | The folder the window calls "Program". |

The configuration file is plain text, and the program reloads it when the file
changes. Uninstalling does not remove the configuration or the cache, so check
these directories before you uninstall, and delete them yourself if you want to
start clean.

## The Config and Help Menus

- "Config" changes the look: a colour-scheme submenu and an "Icon Theme" submenu
  listing the icon themes installed on the system.
- "Help" has "Help", which opens the help window (a filterable topic list on the
  left, with "Quick Start" and "Create Region"), and "About", which shows the
  version and the region and codec modules that loaded.
