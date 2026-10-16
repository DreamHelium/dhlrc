# How to Use dhlrc

dhlrc (also called Minecraft Structure Modifier) reads Minecraft NBT and region
files, shows what is inside them, changes a region's base data, and writes the
result out again. This article covers only the windows you will actually use.

## The Main Window and the Sidebar

The left side lists the program's pages, in this order:

| Entry | What it does |
| --- | --- |
| NBT File Reader | Opens an "NBT Reader" tab for one NBT file. |
| Manage Region | Opens the list of regions loaded in memory. |
| Region Reader/Modifier | Picks one region and opens a reader for it. |
| Settings | Opens the settings dialog. |
| Program Directories | Lists the directories the program uses. |

Every entry is opened with a double click; a single click only selects it, and
the "Search..." box above the list filters the list as you type. An entry opens
as a tab on the right. Tabs can be closed and dragged to reorder, and
double-clicking a tab title tears the page off into a window of its own (the
"Return to tabs" button in that window puts the page back). Closing the last tab
brings the "Welcome!" page back; it offers "Quick Start", "Program Directories",
"NBT Reader" and "Add Files to Regions".

## Reading an NBT File

1. Double-click "NBT File Reader", or click "NBT Reader" on the welcome page.
2. Drag one NBT file onto the window. Dropping several files at once is refused
   with "Multiple files dropped!".
3. A progress bar runs while the file is read, and then the NBT tree is shown;
   errors appear in a warning row above it.

The reader uses the Java NBT codec that ships with the program. Without it, the
window says "The NBT codec is not available.".

## Managing Regions

The "Manage Region" tab holds the regions loaded in memory. Nothing is written
back to your files until you save.

- Add files with "Add", with "Add Files to Regions" on the welcome page, or by
  dropping files onto the window. The file dialog is "Select Files"; loading
  runs in the background.
- Each row shows a display name, a grey UUID, "Added: ...", and rename, remove
  and save buttons. Rename asks for a new name ("Input a New Name") and changes
  the region's own name.
- A region is locked while the program works on it (for example while saving).
  A locked row shows "Locked!" and its buttons are disabled. It unlocks by
  itself when the work finishes, and while locked it cannot be renamed, removed
  or selected.
- Tick "Select" to show checkboxes and a second toolbar. "Select All" ticks or
  unticks every row, the label counts the selection, and "Remove Selected" and
  "Save Selected" act on it. Locked regions are kept, with a warning.
- Saving asks for the format in "Select Save Format". If a format can write
  several regions into one file and more than one region is selected, a "Use
  MultiFunc?" question appears: yes writes one file, no writes one file per
  region. Then pick a file ("Select File") or a directory ("Select Directory").

## The Region Reader/Modifier

1. Double-click "Region Reader/Modifier" and pick a region in "Select
   Region(s)". Locked regions are labelled "Locked" and cannot be picked.
2. The region opens in a "Region Reader/Modifier" tab (window title "Read
   Blocks"). It first downloads that version's language file.
3. The top shows the region's base data: size and coordinates, DataVersion,
   version name, create and modify time, author, name and description.
4. Type x, y and z under "Please enter the coordination of the block:". When all
   three are valid, "The information is:" lists the block's name, translation
   name, index, palette entry and properties.
5. "Modify Region" opens a "Region Modifier" tab for editing time, description,
   offset, author, base name, region name and DataVersion. "Show All Entities"
   and "Show Entity NBT" open NBT views, "List blocks" and "Show blocks" open
   the block list and the block view, and "3D Preview" opens the 3D view.
   "Modify Property" is available for blocks with properties, but its editor is
   not finished yet.

## Settings

Double-click "Settings" for a page-based dialog with the pages "General",
"Default", "Game", "Reader", "Notifications", "3D Preview" and "Manage". The
last page holds options provided by the loaded modules. Every page shows the
configuration file path and a button that opens its folder.

The buttons are "Restore Defaults", "Reload", "Apply", "OK" and "Cancel".
"Reload" re-reads the file and discards your edits; "Apply" saves without
closing the dialog. The settings you are most likely to want:

- Application Memory Limit (with its unit), and Cache Directory.
- Try Other Modules when Loading Fails: a file is read by the module its
  extension points at, and by the others when that fails. A file that loads as
  something other than what its name says is reported at the end instead of
  being refused.
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
