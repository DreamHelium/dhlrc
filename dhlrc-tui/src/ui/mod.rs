/*! The interactive terminal UI.
 *
 * The page model is deliberately free of terminal I/O, so it can be driven in
 * tests: [`App`] owns the configuration, the cursor and the areas the last
 * frame published for the mouse, and [`Screen`] is a small character grid that
 * both the renderer and the tests read. [`term`] is the only part that talks
 * to a real terminal (raw mode, mouse capture, alternate screen).
 *
 * The first page is the settings screen: the groups on the left, the settings
 * of the chosen group on the right. A click or a tap arrives as a mouse event
 * in a terminal (touching a touchscreen sends the same press the mouse does),
 * so the list is clickable, and clicking the selected row opens its editor.
 */

use crate::cli::Kind;
use crate::i18n::gettext;
use command::{CommandLine, CommandStep, StoredRegion};
use dhlrc_core::config::{Config, ConfigInit, ConfigPaths, save_config};
use dhlrc_core::modules::default_dirs;
use dhlrc_core::schema::{SETTINGS, SettingDef, SettingType, settings_summary};
use unicode_width::UnicodeWidthChar;

mod command;
pub mod term;

/// The message ids for "On" and "Off", so a bool reads as words rather than
/// as `true`/`false`. Written as calls for `xgettext`.
fn on_text() -> String {
    gettext("On")
}

fn off_text() -> String {
    gettext("Off")
}

// ---------------------------------------------------------------------------
// The character grid

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub enum Color {
    #[default]
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Cyan,
    White,
    DarkGrey,
    LightCyan,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct Style {
    pub fg: Color,
    pub bg: Color,
    pub bold: bool,
}

impl Style {
    pub const fn plain() -> Self {
        Self {
            fg: Color::Default,
            bg: Color::Default,
            bold: false,
        }
    }

    #[allow(dead_code)] // used by the tests and kept for future pages
    pub const fn fg(colour: Color) -> Self {
        Self {
            fg: colour,
            bg: Color::Default,
            bold: false,
        }
    }

    pub const fn bold(self) -> Self {
        Self { bold: true, ..self }
    }

    pub const fn on(self, background: Color) -> Self {
        Self {
            bg: background,
            ..self
        }
    }

    pub const fn with_fg(self, colour: Color) -> Self {
        Self { fg: colour, ..self }
    }
}

#[derive(Clone, Copy, Debug)]
struct Cell {
    ch: char,
    style: Style,
}

#[derive(Clone, Debug)]
pub struct Screen {
    pub width: u16,
    pub height: u16,
    cells: Vec<Cell>,
}

impl Screen {
    pub fn new(width: u16, height: u16) -> Self {
        let mut screen = Self {
            width,
            height,
            cells: Vec::new(),
        };
        screen.resize(width, height);
        screen
    }

    pub fn resize(&mut self, width: u16, height: u16) {
        self.width = width;
        self.height = height;
        self.cells = vec![
            Cell {
                ch: ' ',
                style: Style::plain(),
            };
            width as usize * height as usize
        ];
    }

    fn index(&self, x: u16, y: u16) -> Option<usize> {
        (x < self.width && y < self.height).then(|| y as usize * self.width as usize + x as usize)
    }

    pub fn cell(&self, x: u16, y: u16) -> Option<(char, Style)> {
        self.index(x, y)
            .map(|i| (self.cells[i].ch, self.cells[i].style))
    }

    pub fn set(&mut self, x: u16, y: u16, ch: char, style: Style) {
        if let Some(i) = self.index(x, y) {
            self.cells[i] = Cell { ch, style };
        }
    }

    /// Writes `text` from `(x, y)`, one cell per column a character occupies
    /// (a wide character marks the cell after it as a continuation), and
    /// returns the column after the last character. Stops at the right edge.
    pub fn put(&mut self, x: u16, y: u16, text: &str, style: Style) -> u16 {
        let mut column = x;
        for ch in text.chars() {
            let width = ch.width().unwrap_or(0) as u16;
            if width == 0 {
                continue;
            }
            if column + width > self.width {
                break;
            }
            self.set(column, y, ch, style);
            if width == 2 {
                self.set(column + 1, y, '\0', style);
            }
            column += width;
        }
        column
    }

    /// Writes `text` so it ends at `right` (the column after its last cell),
    /// cutting the start when it does not fit.
    pub fn put_right(&mut self, right: u16, y: u16, text: &str, style: Style) -> u16 {
        let width: u16 = text.chars().map(|c| c.width().unwrap_or(0) as u16).sum();
        let start = right.saturating_sub(width);
        self.put(start, y, text, style);
        start
    }

    pub fn fill(&mut self, area: Rect, ch: char, style: Style) {
        for y in area.y..area.y.saturating_add(area.h).min(self.height) {
            for x in area.x..area.x.saturating_add(area.w).min(self.width) {
                self.set(x, y, ch, style);
            }
        }
    }

    /// The rows of the grid as text, for tests and for a `--dump` mode.
    #[allow(dead_code)]
    pub fn text(&self) -> Vec<String> {
        (0..self.height)
            .map(|y| {
                let mut line = String::with_capacity(self.width as usize);
                for x in 0..self.width {
                    if let Some((ch, _)) = self.cell(x, y) {
                        if ch != '\0' {
                            line.push(ch);
                        }
                    }
                }
                line.trim_end().to_string()
            })
            .collect()
    }
}

/// A rectangle on the screen, used for the panes and the mouse hit test.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Rect {
    pub x: u16,
    pub y: u16,
    pub w: u16,
    pub h: u16,
}

impl Rect {
    pub const fn new(x: u16, y: u16, w: u16, h: u16) -> Self {
        Self { x, y, w, h }
    }

    pub fn contains(&self, x: u16, y: u16) -> bool {
        x >= self.x
            && x < self.x.saturating_add(self.w)
            && y >= self.y
            && y < self.y.saturating_add(self.h)
    }
}

// ---------------------------------------------------------------------------
// Input, in the page model's own terms

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Key {
    Up,
    Down,
    Left,
    Right,
    Enter,
    Esc,
    Tab,
    Backspace,
    Home,
    End,
    PageUp,
    PageDown,
    F(u8),
    Char(char),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MouseKind {
    Down,
    Up,
    WheelUp,
    WheelDown,
    Drag,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Mouse {
    pub kind: MouseKind,
    pub x: u16,
    pub y: u16,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Event {
    Key(Key),
    Mouse(Mouse),
    Resize(u16, u16),
}

/// The top-level pages the function bar switches between.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Page {
    Settings,
    Modules,
    Render,
    Regions,
    Help,
}

impl Page {
    const ALL: [Page; 5] = [
        Page::Settings,
        Page::Modules,
        Page::Render,
        Page::Regions,
        Page::Help,
    ];

    fn title(self) -> String {
        match self {
            Page::Settings => gettext("Settings"),
            Page::Modules => gettext("Modules"),
            Page::Render => gettext("Render"),
            Page::Regions => gettext("Regions"),
            Page::Help => gettext("Help"),
        }
    }

    /// What the page shows for now: a short note, the equivalent command line
    /// and, for Help, the keys. The pages that only say what they will do are
    /// deliberately thin; they are the entry points for the next pieces.
    fn lines(self) -> Vec<String> {
        match self {
            Page::Settings => Vec::new(),
            Page::Modules => vec![
                gettext(
                    "List the region modules and object codecs installed next to the executable.",
                ),
                gettext("The same report the `dhlrc-tui modules` command prints."),
            ],
            Page::Render => vec![
                gettext("Load a structure file and write a rendered PNG."),
                gettext("The command line version already works:"),
                String::from("    dhlrc-tui render <file> -o <image.png>"),
                gettext("Choosing the file and the view in this page will come later."),
            ],
            Page::Regions => vec![
                gettext("Open, inspect and modify a region file."),
                gettext(
                    "This page is an entry point for now; the interactive editor will come later.",
                ),
            ],
            Page::Help => vec![
                gettext(
                    "F1-F5 switch pages, F10 quits. The function bar at the bottom can be clicked.",
                ),
                gettext(
                    "The other pages are entry points for now; Modules lists what is installed.",
                ),
                gettext("↑↓ move · PageUp/PageDown scroll · Home/End start or end"),
                gettext("Tab switches panes · ←→ step through a list setting"),
                gettext("Enter/Space edit, flip or open the selected row · Esc cancels an editor"),
                gettext(
                    ": opens the command line (help, page, load, list, select, close, info, get, set, reload, save, quit)",
                ),
                gettext("In the command line: Enter runs · Tab completes · ↑↓ history · Esc cancels"),
                gettext("s saves the configuration · r reloads it from disk · q quits"),
                gettext(
                    "Mouse: click a group or row, click again to edit, wheel scrolls the pane under the pointer.",
                ),
            ],
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Focus {
    Sidebar,
    Groups,
    Settings,
}

#[derive(Clone)]
struct Edit {
    def: &'static SettingDef,
    buffer: String,
}

/// A clickable entry of the bottom function bar.
#[derive(Clone, Debug)]
struct ActionArea {
    x: u16,
    width: u16,
    action: Action,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Action {
    Page(Page),
    Quit,
}

#[derive(Clone, Debug, Default)]
struct Layout {
    sidebar: Rect,
    groups: Rect,
    settings: Rect,
    setting_first: u16,
    setting_rows: u16,
}

// ---------------------------------------------------------------------------
// The settings page

pub struct App {
    paths: ConfigPaths,
    config: Config,
    groups: Vec<&'static str>,
    rows: Vec<Vec<&'static SettingDef>>,
    group: usize,
    row: usize,
    focus: Focus,
    scroll: usize,
    edit: Option<Edit>,
    status: Option<(String, bool)>,
    quit: bool,
    page: Page,
    layout: Layout,
    actions: Vec<ActionArea>,
    action_y: u16,
    /// The text a page draws, built when the page is first opened: the module
    /// list comes from the same code the command line uses.
    content: Option<(Page, Vec<String>)>,
    entry_scroll: usize,
    /// The `:` line, the regions it can keep in memory, and the output it
    /// leaves on the screen.
    cmd: CommandLine,
    regions: Vec<StoredRegion>,
    selected: Option<usize>,
    transcript: Vec<String>,
}

impl App {
    pub fn new(init: ConfigInit) -> Self {
        Self::from_parts(init.paths, init.config)
    }

    /// Builds the page over an already loaded configuration; the tests use it
    /// so they never touch the user's own file.
    pub fn from_parts(paths: ConfigPaths, config: Config) -> Self {
        let mut groups: Vec<&'static str> = Vec::new();
        for def in SETTINGS {
            if !groups.contains(&def.group) {
                groups.push(def.group);
            }
        }
        let rows = groups
            .iter()
            .map(|group| {
                SETTINGS
                    .iter()
                    .filter(|def| def.group == *group)
                    .collect::<Vec<_>>()
            })
            .collect();
        Self {
            paths,
            config,
            groups,
            rows,
            group: 0,
            row: 0,
            focus: Focus::Groups,
            scroll: 0,
            edit: None,
            status: None,
            quit: false,
            page: Page::Settings,
            layout: Layout::default(),
            actions: Vec::new(),
            action_y: 0,
            content: None,
            entry_scroll: 0,
            cmd: CommandLine::new(),
            regions: Vec::new(),
            selected: None,
            transcript: Vec::new(),
        }
    }

    pub fn quit(&self) -> bool {
        self.quit
    }

    fn current_rows(&self) -> &[&'static SettingDef] {
        &self.rows[self.group]
    }

    fn current_def(&self) -> Option<&'static SettingDef> {
        self.current_rows().get(self.row).copied()
    }

    // -- values ------------------------------------------------------------

    fn value_text(&self, def: &SettingDef) -> String {
        let raw = (def.get)(&self.config);
        match def.kind {
            SettingType::Bool => {
                if raw == "true" {
                    on_text()
                } else {
                    off_text()
                }
            }
            SettingType::Enum => def
                .choices
                .get(raw.parse::<usize>().unwrap_or(usize::MAX))
                .map(|choice| gettext(*choice))
                .unwrap_or(raw),
            _ => raw,
        }
    }

    fn save(&mut self) {
        let _ = self.save_result();
    }

    /// The same save, with the error kept for a caller (the command line) that
    /// has somewhere to report it.
    fn save_result(&mut self) -> Result<(), String> {
        match save_config(&self.paths, &self.config) {
            Ok(()) => {
                self.set_status(gettext("Saved"), false);
                Ok(())
            }
            Err(error) => {
                let message = format!("{}: {error}", gettext("Could not save the configuration"));
                self.set_status(message.clone(), true);
                Err(message)
            }
        }
    }

    fn set_status(&mut self, text: String, error: bool) {
        self.status = Some((text, error));
    }

    // -- moving and activating --------------------------------------------

    fn move_row(&mut self, delta: isize) {
        let count = self.current_rows().len();
        if count == 0 {
            return;
        }
        let next = self.row as isize + delta;
        self.row = next.clamp(0, count as isize - 1) as usize;
        self.ensure_visible();
    }

    fn wheel(&mut self, mouse: Mouse, step: isize) {
        if self.layout.groups.contains(mouse.x, mouse.y) {
            self.focus = Focus::Groups;
            self.move_group(step);
        } else if self.layout.settings.contains(mouse.x, mouse.y) {
            self.focus = Focus::Settings;
            self.move_row(step * 3);
        }
    }

    fn move_group(&mut self, delta: isize) {
        let count = self.groups.len();
        if count == 0 {
            return;
        }
        let next = self.group as isize + delta;
        self.group = next.clamp(0, count as isize - 1) as usize;
        self.row = 0;
        self.scroll = 0;
    }

    fn ensure_visible(&mut self) {
        let visible = self.layout.setting_rows.max(1) as usize;
        if self.row < self.scroll {
            self.scroll = self.row;
        } else if self.row >= self.scroll + visible {
            self.scroll = self.row + 1 - visible;
        }
    }

    /// Enter, a click on the selected row, or space: edit a text value, or
    /// flip a switch, or step to the next choice of a list.
    fn activate(&mut self) {
        let Some(def) = self.current_def() else {
            return;
        };
        match def.kind {
            SettingType::Bool => {
                let value = (def.get)(&self.config) == "true";
                self.apply(def, if value { "false" } else { "true" });
            }
            SettingType::Enum => self.cycle(def, 1),
            _ => self.begin_edit(def),
        }
    }

    fn cycle(&mut self, def: &SettingDef, direction: isize) {
        if def.choices.is_empty() {
            return;
        }
        let current = (def.get)(&self.config)
            .parse::<isize>()
            .unwrap_or(0)
            .clamp(0, def.choices.len() as isize - 1);
        let count = def.choices.len() as isize;
        let next = (current + direction).rem_euclid(count);
        self.apply(def, &next.to_string());
    }

    fn begin_edit(&mut self, def: &'static SettingDef) {
        self.edit = Some(Edit {
            def,
            buffer: (def.get)(&self.config),
        });
        self.set_status(
            format!("{} {}", gettext("Editing"), gettext(def.label)),
            false,
        );
    }

    fn commit_edit(&mut self) {
        /* The editor stays open when the text is refused, so a typo is not
         * lost and the value on disk keeps its old shape. */
        let Some(edit) = self.edit.as_ref() else {
            return;
        };
        let def = edit.def;
        let text = edit.buffer.trim().to_owned();
        match normalise(def.kind, &text) {
            Some(value) => {
                self.edit = None;
                self.apply(def, &value);
            }
            None => self.set_status(format!("{}: {text}", gettext("Not a valid value")), true),
        }
    }

    fn apply(&mut self, def: &SettingDef, value: &str) {
        self.config.set_setting(def.key, value);
        self.save();
    }

    // -- input -------------------------------------------------------------

    pub fn handle(&mut self, event: &Event) {
        match event {
            Event::Resize(_, _) => {}
            Event::Key(key) => self.handle_key(*key),
            Event::Mouse(mouse) => self.handle_mouse(*mouse),
        }
    }

    fn handle_key(&mut self, key: Key) {
        if let Some(edit) = self.edit.as_mut() {
            match key {
                Key::Esc => {
                    self.edit = None;
                    self.set_status(gettext("Edit cancelled"), false);
                }
                Key::Enter => self.commit_edit(),
                Key::Backspace => {
                    edit.buffer.pop();
                }
                Key::Char(ch) => edit.buffer.push(ch),
                _ => {}
            }
            return;
        }

        /* The command line takes every key while it is open; it is the one
         * place where typing a plain letter does not mean a shortcut. */
        if self.cmd.is_active() {
            match self.cmd.handle_key(key, &self.region_names()) {
                CommandStep::Run => {
                    let line = self.cmd.take_line();
                    self.run_command(&line);
                }
                CommandStep::Cancel => {}
                CommandStep::None => {}
            }
            return;
        }

        /* An earlier command's output stays up until it is dismissed; `Esc`
         * closes it before it reaches its old meaning of quitting. */
        if key == Key::Esc && !self.transcript.is_empty() {
            self.transcript.clear();
            self.status = None;
            return;
        }
        if key == Key::Char(':') {
            self.open_command();
            return;
        }

        /* The function bar works from every page, including the ones that have
         * nothing else on them yet. */
        if let Key::F(number) = key {
            self.function_key(number);
            return;
        }
        if key == Key::Char('q') || key == Key::Esc {
            self.quit = true;
            return;
        }
        if self.page != Page::Settings {
            if self.focus == Focus::Sidebar {
                match key {
                    Key::Up => self.move_page(-1),
                    Key::Down => self.move_page(1),
                    Key::Home => self.switch_page(Page::ALL[0]),
                    Key::End => self.switch_page(Page::ALL[Page::ALL.len() - 1]),
                    Key::Enter | Key::Char(' ') | Key::Tab => self.focus = Focus::Settings,
                    _ => {}
                }
                return;
            }
            let step = match key {
                Key::PageUp | Key::PageDown => 10,
                _ => 1,
            };
            match key {
                Key::Up => self.scroll_entry(-(step as isize)),
                Key::Down => self.scroll_entry(step as isize),
                Key::PageUp => self.scroll_entry(-(step as isize)),
                Key::PageDown => self.scroll_entry(step as isize),
                Key::Home => self.entry_scroll = 0,
                Key::End => self.entry_scroll = usize::MAX / 2,
                Key::Tab => self.focus = Focus::Sidebar,
                _ => {}
            }
            return;
        }

        if self.focus == Focus::Sidebar {
            match key {
                Key::Up => self.move_page(-1),
                Key::Down => self.move_page(1),
                Key::Home => self.switch_page(Page::ALL[0]),
                Key::End => self.switch_page(Page::ALL[Page::ALL.len() - 1]),
                Key::Enter | Key::Char(' ') | Key::Tab => self.focus = Focus::Groups,
                Key::Char('s') => self.save(),
                Key::Char('r') => self.reload(),
                _ => {}
            }
            return;
        }

        match key {
            Key::Char('s') => self.save(),
            Key::Char('r') => self.reload(),
            Key::Tab => {
                self.focus = match self.focus {
                    Focus::Sidebar => Focus::Groups,
                    Focus::Groups => Focus::Settings,
                    Focus::Settings => Focus::Sidebar,
                }
            }
            Key::Up => match self.focus {
                Focus::Sidebar => self.move_page(-1),
                Focus::Groups => self.move_group(-1),
                Focus::Settings => self.move_row(-1),
            },
            Key::Down => match self.focus {
                Focus::Sidebar => self.move_page(1),
                Focus::Groups => self.move_group(1),
                Focus::Settings => self.move_row(1),
            },
            Key::PageUp => self.move_row(-(self.layout.setting_rows.max(1) as isize)),
            Key::PageDown => self.move_row(self.layout.setting_rows.max(1) as isize),
            Key::Home => self.move_row(isize::MIN / 2),
            Key::End => self.move_row(isize::MAX / 2),
            Key::Left => {
                if self.focus == Focus::Settings {
                    if let Some(def) = self.current_def() {
                        self.cycle(def, -1);
                    }
                } else {
                    self.move_group(-1);
                }
            }
            Key::Right => {
                if self.focus == Focus::Settings {
                    if let Some(def) = self.current_def() {
                        self.cycle(def, 1);
                    }
                } else {
                    self.move_group(1);
                }
            }
            Key::Enter | Key::Char(' ') => self.activate(),
            _ => {}
        }
    }

    fn function_key(&mut self, number: u8) {
        if number == 10 {
            self.quit = true;
            return;
        }
        if let Some(page) = number
            .checked_sub(1)
            .and_then(|index| Page::ALL.get(index as usize))
        {
            self.switch_page(*page);
        }
    }

    fn switch_page(&mut self, page: Page) {
        self.page = page;
        self.status = None;
        self.entry_scroll = 0;
        self.scroll = 0;
        self.focus = if page == Page::Settings {
            Focus::Groups
        } else {
            Focus::Settings
        };
        if page == Page::Modules && self.content.as_ref().map(|(_, lines)| lines).is_none() {
            let (region_dir, load_dir) = default_dirs();
            let text = crate::modules::report(&region_dir, &load_dir, Kind::All, false);
            let lines = text.lines().map(str::to_owned).collect();
            self.content = Some((Page::Modules, lines));
        }
    }

    fn handle_mouse(&mut self, mouse: Mouse) {
        if self.edit.is_some() || self.cmd.is_active() {
            return;
        }
        if mouse.kind == MouseKind::Down && self.layout.sidebar.contains(mouse.x, mouse.y) {
            let index = (mouse.y - self.layout.sidebar.y) as usize;
            if let Some(page) = Page::ALL.get(index).copied() {
                self.switch_page(page);
            }
            return;
        }
        if mouse.kind == MouseKind::Down && mouse.y == self.action_y {
            if let Some(area) = self
                .actions
                .iter()
                .find(|area| mouse.x >= area.x && mouse.x < area.x + area.width)
            {
                match area.action {
                    Action::Page(page) => self.switch_page(page),
                    Action::Quit => self.quit = true,
                }
            }
            return;
        }
        if mouse.kind != MouseKind::Down && self.layout.sidebar.contains(mouse.x, mouse.y) {
            match mouse.kind {
                MouseKind::WheelUp => self.move_page(-1),
                MouseKind::WheelDown => self.move_page(1),
                _ => {}
            }
            return;
        }
        if self.page != Page::Settings {
            match mouse.kind {
                MouseKind::WheelUp => self.scroll_entry(-3),
                MouseKind::WheelDown => self.scroll_entry(3),
                _ => {}
            }
            return;
        }
        match mouse.kind {
            MouseKind::Down => {
                if self.layout.groups.contains(mouse.x, mouse.y) {
                    let index = (mouse.y - self.layout.groups.y) as usize;
                    if index < self.groups.len() {
                        self.focus = Focus::Groups;
                        if index != self.group {
                            self.group = index;
                            self.row = 0;
                            self.scroll = 0;
                        }
                    }
                    return;
                }
                if self.layout.settings.contains(mouse.x, mouse.y) {
                    let offset = (mouse.y - self.layout.setting_first) as usize;
                    let index = offset + self.scroll;
                    if index < self.current_rows().len() {
                        let was_selected = index == self.row;
                        self.focus = Focus::Settings;
                        self.row = index;
                        self.ensure_visible();
                        /* Clicking the row that is already selected opens it,
                         * so one click selects and the next one edits; a
                         * double-click does the same in one go. */
                        if was_selected {
                            self.activate();
                        }
                    }
                }
            }
            /* The wheel scrolls the pane the pointer is over: one entry per
             * notch on the group list, three rows on the settings page, which
             * is the usual step for a page of text. */
            MouseKind::WheelUp => self.wheel(mouse, -1),
            MouseKind::WheelDown => self.wheel(mouse, 1),
            _ => {}
        }
    }

    fn move_page(&mut self, delta: isize) {
        let index = Page::ALL
            .iter()
            .position(|page| *page == self.page)
            .unwrap_or(0) as isize;
        let next = (index + delta).clamp(0, Page::ALL.len() as isize - 1) as usize;
        let page = Page::ALL[next];
        if page != self.page {
            self.switch_page(page);
        }
    }

    fn scroll_entry(&mut self, delta: isize) {
        let lines = self.entry_lines().len();
        let max = lines.saturating_sub(1);
        let next = (self.entry_scroll as isize).saturating_add(delta);
        self.entry_scroll = next.clamp(0, max as isize) as usize;
    }

    fn reload(&mut self) {
        let _ = self.reload_result();
    }

    /// The same reload, with the error kept for the command line.
    fn reload_result(&mut self) -> Result<(), String> {
        match dhlrc_core::load_or_create(&self.paths) {
            Ok(init) => {
                self.config = init.config;
                self.set_status(gettext("Configuration reloaded"), false);
                Ok(())
            }
            Err(error) => {
                let message = format!(
                    "{}: {error}",
                    gettext("Could not reload the configuration")
                );
                self.set_status(message.clone(), true);
                Err(message)
            }
        }
    }

    // -- drawing -----------------------------------------------------------

    pub fn render(&mut self, screen: &mut Screen) {
        screen.resize(screen.width, screen.height);
        let width = screen.width;
        let height = screen.height;
        if width == 0 || height == 0 {
            return;
        }

        let header = Style::plain().bold().on(Color::DarkGrey);
        for x in 0..width {
            screen.set(x, 0, ' ', header);
        }
        let title = format!("dhlrc  {}", self.page.title());
        screen.put(1, 0, &title, header.with_fg(Color::White).bold());
        if self.page == Page::Settings {
            let path = self.paths.file.display().to_string();
            screen.put_right(width.saturating_sub(1), 0, &path, header);
        }

        self.render_sidebar(screen);
        if self.page == Page::Settings {
            self.render_settings(screen);
        } else {
            self.render_entry_page(screen);
        }

        /* The three footer lines: the status (or the page's key hints), the
         * editor or the tooltip, and the clickable function bar. */
        let status_y = height.saturating_sub(3);
        let detail_y = height.saturating_sub(2);
        if let Some((text, error)) = &self.status {
            let style = Style::plain().with_fg(if *error { Color::Red } else { Color::Green });
            screen.put(
                1,
                status_y,
                &truncate(text, width.saturating_sub(2) as usize),
                style,
            );
        } else if self.edit.is_none() {
            screen.put(
                1,
                status_y,
                &truncate(&self.hint_text(), width.saturating_sub(2) as usize),
                Style::plain().with_fg(Color::DarkGrey),
            );
        }
        if let Some(edit) = &self.edit {
            let prompt = format!("{} {}: ", gettext("Editing"), gettext(edit.def.label));
            let mut x = screen.put(1, detail_y, &prompt, Style::plain().bold());
            let room = width.saturating_sub(x + 1) as usize;
            let value = tail(&edit.buffer, room);
            x = screen.put(x, detail_y, &value, Style::plain().with_fg(Color::Yellow));
            screen.set(x.min(width - 1), detail_y, '▏', Style::plain());
            screen.put(
                1,
                status_y,
                &gettext("Enter saves, Esc cancels"),
                Style::plain().with_fg(Color::DarkGrey),
            );
        } else if self.page == Page::Settings {
            if let Some(def) = self.current_def() {
                let tooltip = gettext(def.tooltip);
                if !tooltip.is_empty() {
                    screen.put(
                        1,
                        detail_y,
                        &truncate(&tooltip, width.saturating_sub(2) as usize),
                        Style::plain().with_fg(Color::DarkGrey),
                    );
                }
            }
        }

        self.render_actions(screen);
        /* The command line and its output sit over the bottom of the frame, so
         * they are painted after the page and the function bar. */
        self.render_console(screen);
    }

    fn render_settings(&mut self, screen: &mut Screen) {
        let width = screen.width;
        let height = screen.height;
        /* One row at the bottom is kept for the group's one-line summary. */
        let body_y = 1u16;
        let body_h = height.saturating_sub(5);
        let content_x = self.layout.sidebar.x + self.layout.sidebar.w + 1;
        let left_width = self.left_width(screen);
        let left = Rect::new(content_x, body_y, left_width, body_h);
        let settings_x = content_x.saturating_add(left_width).saturating_add(1);
        let settings = Rect::new(settings_x, body_y, width.saturating_sub(settings_x), body_h);

        /* The groups, with the selected one highlighted. */
        for (index, group) in self.groups.iter().enumerate() {
            if index as u16 >= body_h {
                break;
            }
            let y = body_y + index as u16;
            let selected = index == self.group;
            let style = if selected {
                Style::plain().with_fg(Color::Black).on(Color::Cyan).bold()
            } else {
                Style::plain()
            };
            screen.fill(Rect::new(content_x, y, left_width, 1), ' ', style);
            let label = gettext(*group);
            screen.put(content_x + 2, y, &label, style);
        }
        /* The separator between the panes. */
        for y in body_y..body_y + body_h {
            screen.set(
                content_x + left_width,
                y,
                '│',
                Style::plain().with_fg(Color::DarkGrey),
            );
        }

        /* The settings of the chosen group. */
        let rows = &self.rows[self.group];
        let visible = body_h.saturating_sub(1) as usize;
        self.scroll = self.scroll.min(rows.len().saturating_sub(visible.max(1)));
        let value_x = width.saturating_sub(4);
        let label_width = settings.w.saturating_sub(6).min(settings.w);

        for (offset, def) in rows.iter().enumerate().skip(self.scroll).take(visible) {
            let index = offset;
            let y = body_y + 1 + (index - self.scroll) as u16;
            let selected = index == self.row && self.focus == Focus::Settings;
            let style = if selected {
                Style::plain().with_fg(Color::Black).on(Color::Cyan)
            } else {
                Style::plain()
            };
            if selected {
                screen.fill(
                    Rect::new(settings_x + 1, y, width.saturating_sub(settings_x + 2), 1),
                    ' ',
                    style,
                );
            }
            let label = gettext(def.label);
            let label = truncate(&label, label_width as usize);
            screen.put(settings_x + 2, y, &label, style);
            let value = self.value_text(def);
            let value_style = if selected {
                style
            } else {
                Style::plain().with_fg(Color::LightCyan)
            };
            screen.put_right(value_x, y, &value, value_style);
        }

        let sidebar = self.layout.sidebar;
        self.layout = Layout {
            sidebar,
            groups: left,
            settings,
            setting_first: body_y + 1,
            setting_rows: visible as u16,
        };

        /* A one-line digest of the selected group, so the whole page reads at
         * a glance even when a row is scrolled out of view. */
        let summary = settings_summary(self.groups[self.group]);
        if !summary.is_empty() {
            let y = height.saturating_sub(4);
            let style = Style::plain().with_fg(Color::White).on(Color::DarkGrey);
            screen.fill(Rect::new(0, y, width, 1), ' ', style);
            screen.put(
                1,
                y,
                &truncate(&summary, width.saturating_sub(2) as usize),
                style,
            );
        }
    }

    fn entry_lines(&self) -> Vec<String> {
        match &self.content {
            Some((page, lines)) if *page == self.page => lines.clone(),
            _ => self.page.lines(),
        }
    }

    fn render_entry_page(&mut self, screen: &mut Screen) {
        let width = screen.width;
        let body_h = screen.height.saturating_sub(4).max(1);
        let sidebar = self.layout.sidebar;
        self.layout = Layout {
            sidebar,
            ..Layout::default()
        };
        let content_x = sidebar.x + sidebar.w + 1;
        let lines = self.entry_lines();
        let visible = body_h as usize;
        self.entry_scroll = self.entry_scroll.min(lines.len().saturating_sub(visible));
        let room = width.saturating_sub(content_x + 2) as usize;
        let mut y = 1u16;
        for line in lines.iter().skip(self.entry_scroll).take(visible) {
            let style = if line.starts_with("    ") {
                Style::plain().with_fg(Color::LightCyan)
            } else {
                Style::plain()
            };
            screen.put(content_x + 1, y, &truncate(line, room), style);
            y += 1;
        }
        if self.entry_scroll + visible < lines.len() {
            screen.put_right(
                width.saturating_sub(3),
                screen.height.saturating_sub(4),
                "▾",
                Style::plain().with_fg(Color::DarkGrey),
            );
        }
    }

    fn render_actions(&mut self, screen: &mut Screen) {
        let width = screen.width;
        let y = screen.height.saturating_sub(1);
        self.action_y = y;
        self.actions.clear();
        let mut x = 1u16;
        for (index, page) in Page::ALL.iter().enumerate() {
            let label = format!("F{} {}", index + 1, page.title());
            let text = format!(" {label} ");
            let area_width = text_width(&text) as u16;
            if x + area_width >= width {
                break;
            }
            let style = if *page == self.page {
                Style::plain()
                    .with_fg(Color::Black)
                    .on(Color::LightCyan)
                    .bold()
            } else {
                Style::plain().with_fg(Color::Black).on(Color::Cyan)
            };
            screen.fill(Rect::new(x, y, area_width, 1), ' ', style);
            screen.put(x + 1, y, &label, style);
            self.actions.push(ActionArea {
                x,
                width: area_width,
                action: Action::Page(*page),
            });
            x += area_width + 1;
        }
        let label = format!("F10 {}", gettext("Quit"));
        let text = format!(" {label} ");
        let area_width = text_width(&text) as u16;
        if x + area_width <= width {
            let style = Style::plain().with_fg(Color::White).on(Color::Red).bold();
            screen.fill(Rect::new(x, y, area_width, 1), ' ', style);
            screen.put(x + 1, y, &label, style);
            self.actions.push(ActionArea {
                x,
                width: area_width,
                action: Action::Quit,
            });
        }
    }

    fn sidebar_width(&self, screen: &Screen) -> u16 {
        let widest = Page::ALL
            .iter()
            .map(|page| text_width(&page.title()) as u16)
            .max()
            .unwrap_or(0);
        widest
            .saturating_add(4)
            .clamp(10, 22)
            .min(screen.width.saturating_sub(30))
            .max(10)
    }

    fn render_sidebar(&mut self, screen: &mut Screen) {
        let width = self.sidebar_width(screen);
        let body_y = 1u16;
        let body_h = screen.height.saturating_sub(4);
        let sidebar = Rect::new(0, body_y, width, body_h);
        for (index, page) in Page::ALL.iter().enumerate() {
            if index as u16 >= body_h {
                break;
            }
            let y = body_y + index as u16;
            let style = if *page == self.page {
                Style::plain().with_fg(Color::Black).on(Color::Cyan).bold()
            } else if self.focus == Focus::Sidebar {
                Style::plain().with_fg(Color::LightCyan)
            } else {
                Style::plain()
            };
            screen.fill(Rect::new(0, y, width, 1), ' ', style);
            screen.put(2, y, &page.title(), style);
        }
        for y in body_y..body_y + body_h {
            screen.set(width, y, '│', Style::plain().with_fg(Color::DarkGrey));
        }
        self.layout.sidebar = sidebar;
    }

    fn hint_text(&self) -> String {
        if self.page == Page::Settings {
            gettext(
                "↑↓ move · Tab switches pane · ←→ changes a list value · Enter/Space edits · : commands · s saves · r reloads · q quits",
            )
        } else {
            gettext(
                "↑↓/PageUp/PageDown scroll · click the sidebar or F1-F5 to switch · : commands · q quits",
            )
        }
    }

    fn left_width(&self, screen: &Screen) -> u16 {
        let widest = self
            .groups
            .iter()
            .map(|group| text_width(&gettext(*group)) as u16)
            .max()
            .unwrap_or(0);
        let wanted = widest.saturating_add(4).clamp(12, 30);
        wanted.min(screen.width.saturating_sub(20)).max(12)
    }
}

/// The value a text editor accepts, normalised, or `None` when it has to be
/// rejected. The setters clamp silently, so the check happens here where a
/// typo can be reported instead of replaced.
fn normalise(kind: SettingType, text: &str) -> Option<String> {
    match kind {
        SettingType::Bool => match text {
            "true" | "false" => Some(text.to_owned()),
            _ => None,
        },
        SettingType::Int => text.parse::<i64>().ok().map(|v| v.to_string()),
        SettingType::Enum => text.parse::<usize>().ok().map(|v| v.to_string()),
        SettingType::Colour => {
            let digits = text.strip_prefix('#').unwrap_or(text);
            let valid = (digits.len() == 6 || digits.len() == 3)
                && digits.chars().all(|c| c.is_ascii_hexdigit());
            valid.then(|| format!("#{}", digits.to_ascii_lowercase()))
        }
        SettingType::Path => (!text.is_empty()).then(|| text.to_owned()),
        SettingType::String => Some(text.to_owned()),
    }
}

fn text_width(text: &str) -> usize {
    text.chars().map(|c| c.width().unwrap_or(0)).sum()
}

/// Cuts `text` so it fits in `width` cells, ending with an ellipsis.
fn truncate(text: &str, width: usize) -> String {
    if text_width(text) <= width {
        return text.to_owned();
    }
    let mut out = String::new();
    let mut used = 0;
    for ch in text.chars() {
        let w = ch.width().unwrap_or(0);
        if used + w + 1 > width {
            break;
        }
        out.push(ch);
        used += w;
    }
    out.push('…');
    out
}

/// The last part of `text` that fits in `width` cells (the editor scrolls
/// sideways so the cursor stays visible in a long value).
fn tail(text: &str, width: usize) -> String {
    let chars: Vec<char> = text.chars().collect();
    let mut used = 0;
    let mut start = chars.len();
    while start > 0 {
        let w = chars[start - 1].width().unwrap_or(0);
        if used + w > width {
            break;
        }
        used += w;
        start -= 1;
    }
    chars[start..].iter().collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn app() -> (App, PathBuf) {
        let dir = std::env::temp_dir().join(format!(
            "dhlrc-tui-test-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        std::fs::create_dir_all(&dir).unwrap();
        let paths = ConfigPaths {
            file: dir.join("config.toml"),
        };
        (App::from_parts(paths, Config::default()), dir)
    }

    fn press(app: &mut App, key: Key) {
        app.handle(&Event::Key(key));
    }

    fn type_text(app: &mut App, text: &str) {
        for ch in text.chars() {
            press(app, Key::Char(ch));
        }
    }

    fn row_of(app: &App, key: &str) -> usize {
        app.current_rows()
            .iter()
            .position(|def| def.key == key)
            .unwrap_or_else(|| panic!("{key} is not on the current page"))
    }

    #[test]
    fn the_pages_come_from_the_setting_schema() {
        let (app, _dir) = app();
        assert!(!app.groups.is_empty());
        assert_eq!(app.groups[0], SETTINGS[0].group);
        let total: usize = app.rows.iter().map(Vec::len).sum();
        assert_eq!(total, SETTINGS.len());
        for def in SETTINGS {
            assert!(app.groups.contains(&def.group));
        }
    }

    #[test]
    fn the_keyboard_moves_the_selection() {
        let (mut app, _dir) = app();
        press(&mut app, Key::Tab);
        assert_eq!(app.focus, Focus::Settings);
        let before = app.row;
        press(&mut app, Key::Down);
        assert_eq!(app.row, before + 1);
        press(&mut app, Key::Up);
        assert_eq!(app.row, before);
        press(&mut app, Key::Down);
        press(&mut app, Key::PageDown);
        assert!(app.row <= app.current_rows().len() - 1);
    }

    #[test]
    fn entering_toggles_a_switch_and_writes_the_file() {
        let (mut app, dir) = app();
        app.group = app
            .groups
            .iter()
            .position(|group| *group == i18n_group("Notifications"))
            .unwrap();
        app.row = row_of(&app, "SuppressWhenActive");
        app.focus = Focus::Settings;
        let before = app.config.setting("SuppressWhenActive").unwrap();
        press(&mut app, Key::Enter);
        let after = app.config.setting("SuppressWhenActive").unwrap();
        assert_ne!(before, after);
        let written = std::fs::read_to_string(dir.join("config.toml")).unwrap();
        assert!(written.contains("suppress_when_active"));
    }

    #[test]
    fn a_typed_number_is_committed_and_invalid_text_is_refused() {
        let (mut app, _dir) = app();
        app.group = app
            .groups
            .iter()
            .position(|group| *group == i18n_group("Notifications"))
            .unwrap();
        app.row = row_of(&app, "NotificationTimeout");
        app.focus = Focus::Settings;
        let original = app.config.setting("NotificationTimeout").unwrap();
        press(&mut app, Key::Enter);
        assert!(app.edit.is_some());
        for _ in 0..8 {
            press(&mut app, Key::Backspace);
        }
        type_text(&mut app, "250");
        press(&mut app, Key::Enter);
        assert!(app.edit.is_none());
        assert_eq!(app.config.setting("NotificationTimeout").unwrap(), "250");

        press(&mut app, Key::Enter);
        for _ in 0..8 {
            press(&mut app, Key::Backspace);
        }
        type_text(&mut app, "abc");
        press(&mut app, Key::Enter);
        /* The editor stays open and nothing was written. */
        assert!(app.edit.is_some());
        assert_eq!(app.config.setting("NotificationTimeout").unwrap(), "250");

        /* Esc leaves the value alone and closes the editor. */
        press(&mut app, Key::Esc);
        assert!(app.edit.is_none());
        assert_eq!(app.config.setting("NotificationTimeout").unwrap(), "250");
        assert_ne!(original, "250");
    }

    #[test]
    fn an_enum_steps_with_the_arrow_keys() {
        let (app, _dir) = app();
        let def = app
            .current_rows()
            .iter()
            .find(|def| def.kind == SettingType::Enum)
            .copied();
        let Some(def) = def else {
            return;
        };
        let mut app = app;
        app.focus = Focus::Settings;
        app.row = row_of(&app, def.key);
        let before = app.config.setting(def.key).unwrap();
        press(&mut app, Key::Right);
        let after = app.config.setting(def.key).unwrap();
        assert_ne!(before, after);
    }

    #[test]
    fn a_click_selects_a_row_and_a_second_click_opens_it() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let first = app.layout.setting_first;
        let x = app.layout.settings.x + 3;
        /* Clicking the second row selects it. */
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x,
            y: first + 1,
        }));
        assert_eq!(app.focus, Focus::Settings);
        assert_eq!(app.row, app.scroll + 1);
        /* Clicking it again opens the editor (its kind decides what that is). */
        let def = app.current_def().unwrap();
        let kind = def.kind;
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x,
            y: first + 1,
        }));
        match kind {
            SettingType::String | SettingType::Int | SettingType::Path | SettingType::Colour => {
                assert!(app.edit.is_some(), "a text setting opens its editor")
            }
            _ => assert!(app.edit.is_none(), "a switch flips instead of editing"),
        }
    }

    #[test]
    fn a_click_on_a_group_switches_the_page() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let (x, y) = (app.layout.groups.x + 2, app.layout.groups.y + 1);
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x,
            y,
        }));
        assert_eq!(app.focus, Focus::Groups);
        assert_eq!(app.group, 1);
        assert_eq!(app.row, 0);
    }

    #[test]
    fn the_sidebar_lists_the_pages_and_switches_them() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        assert_eq!(
            screen.cell(2, 1).map(|cell| cell.0),
            Page::ALL[0].title().chars().next()
        );
        /* The second page entry sits one row below the first. */
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x: 2,
            y: 1 + 1,
        }));
        assert_eq!(app.page, Page::ALL[1]);
        /* Clicking the current page is harmless. */
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x: 2,
            y: 1 + 1,
        }));
        assert_eq!(app.page, Page::ALL[1]);
    }

    #[test]
    fn the_quit_entry_follows_the_pages_without_a_gap() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(140, 30);
        app.render(&mut screen);
        let index = app
            .actions
            .iter()
            .position(|area| area.action == Action::Quit)
            .expect("the quit entry is drawn");
        let previous = &app.actions[index - 1];
        assert_eq!(
            previous.action,
            Action::Page(Page::ALL[Page::ALL.len() - 1])
        );
        assert_eq!(app.actions[index].x, previous.x + previous.width + 1);
    }

    #[test]
    fn the_hint_line_explains_the_keys_of_the_page() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(120, 30);
        app.render(&mut screen);
        let line: String = (1..119)
            .map(|x| screen.cell(x, 27).map(|cell| cell.0).unwrap_or(' '))
            .collect::<String>()
            .trim()
            .to_owned();
        assert!(line.contains("↑↓"), "hint shown: {line:?}");
        app.switch_page(Page::Modules);
        app.render(&mut screen);
        let line: String = (1..119)
            .map(|x| screen.cell(x, 27).map(|cell| cell.0).unwrap_or(' '))
            .collect::<String>()
            .trim()
            .to_owned();
        assert!(
            line.contains("PageUp") || line.contains("滚动"),
            "hint shown: {line:?}"
        );
    }

    #[test]
    fn the_settings_panes_start_to_the_right_of_the_sidebar() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let sidebar_right = app.layout.sidebar.x + app.layout.sidebar.w;
        assert!(app.layout.groups.x > sidebar_right);
        assert!(app.layout.settings.x > app.layout.groups.x + 4);
    }

    #[test]
    fn the_wheel_scrolls_the_pane_under_the_pointer() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        /* Over the group list one notch moves one group. */
        let over_groups = Mouse {
            kind: MouseKind::WheelDown,
            x: app.layout.groups.x + 2,
            y: app.layout.groups.y + 1,
        };
        let group = app.group;
        app.handle(&Event::Mouse(over_groups));
        assert_eq!(app.focus, Focus::Groups);
        assert_eq!(app.group, group + 1);
        assert_eq!(app.row, 0);
        /* Over the page one notch moves three rows, the usual text step. */
        let over_page = Mouse {
            kind: MouseKind::WheelDown,
            x: app.layout.settings.x + 3,
            y: app.layout.setting_first,
        };
        app.handle(&Event::Mouse(over_page));
        assert_eq!(app.focus, Focus::Settings);
        assert_eq!(app.row, 3.min(app.current_rows().len().saturating_sub(1)));
        /* Scrolling up goes back towards the first row and stops there. */
        for _ in 0..4 {
            app.handle(&Event::Mouse(Mouse {
                kind: MouseKind::WheelUp,
                ..over_page
            }));
        }
        assert_eq!(app.row, 0);
        assert_eq!(app.scroll, 0);
    }

    #[test]
    fn the_function_keys_switch_pages_and_f10_quits() {
        let (mut app, _dir) = app();
        press(&mut app, Key::F(2));
        assert_eq!(app.page, Page::Modules);
        press(&mut app, Key::F(5));
        assert_eq!(app.page, Page::Help);
        press(&mut app, Key::F(1));
        assert_eq!(app.page, Page::Settings);
        press(&mut app, Key::F(10));
        assert!(app.quit());
    }

    #[test]
    fn clicking_the_function_bar_switches_pages() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let area = app
            .actions
            .iter()
            .find(|area| area.action == Action::Page(Page::Modules))
            .cloned()
            .expect("the bar has a Modules entry");
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x: area.x + 1,
            y: app.action_y,
        }));
        assert_eq!(app.page, Page::Modules);
        /* A click on an unused part of the bar does nothing. */
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x: screen.width - 1,
            y: app.action_y,
        }));
        assert_eq!(app.page, Page::Modules);
        /* The quit entry quits. */
        let quit = app
            .actions
            .iter()
            .find(|area| area.action == Action::Quit)
            .cloned()
            .expect("the bar has a Quit entry");
        app.handle(&Event::Mouse(Mouse {
            kind: MouseKind::Down,
            x: quit.x + 1,
            y: app.action_y,
        }));
        assert!(app.quit());
    }

    #[test]
    fn every_page_renders_its_title_and_lines() {
        let (mut app, _dir) = app();
        for page in Page::ALL {
            app.page = page;
            let mut screen = Screen::new(100, 30);
            app.render(&mut screen);
            let text = screen.text().join("\n");
            assert!(text.contains(&page.title()), "{page:?} shows its title");
            for line in page.lines() {
                let head: String = line.chars().take(10).collect();
                assert!(text.contains(&head), "{page:?} shows {head:?}");
            }
        }
    }

    #[test]
    fn the_selected_group_is_highlighted() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(80, 24);
        app.render(&mut screen);
        let (_, style) = screen.cell(0, 1).unwrap();
        assert_eq!(style.bg, Color::Cyan);
    }

    #[test]
    fn the_frame_shows_the_group_the_labels_and_the_values() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let lines = screen.text();
        let joined = lines.join("\n");
        assert!(joined.contains(&gettext(SETTINGS[0].group)));
        assert!(joined.contains(&gettext(SETTINGS[0].label)));
        assert!(joined.contains(&(SETTINGS[0].get)(&app.config)));
        assert!(joined.contains(&gettext("Settings")));
    }

    #[test]
    fn the_mouse_hit_areas_do_not_overlap() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(80, 24);
        app.render(&mut screen);
        assert!(app.layout.groups.w < 80);
        assert!(
            !app.layout
                .groups
                .contains(app.layout.settings.x, app.layout.settings.y)
        );
        assert!(app.layout.settings.y > 0);
    }

    #[test]
    fn the_colon_opens_a_command_line_that_runs_and_cancels() {
        let (mut app, _dir) = app();
        press(&mut app, Key::Char(':'));
        assert!(app.cmd.is_active());
        type_text(&mut app, "page modules");
        press(&mut app, Key::Enter);
        assert!(!app.cmd.is_active());
        assert_eq!(app.page, Page::Modules);
        assert!(app.transcript.iter().any(|line| line.contains("Modules")));

        /* Esc closes the line without running what was typed. */
        press(&mut app, Key::Char(':'));
        type_text(&mut app, "quit");
        press(&mut app, Key::Esc);
        assert!(!app.quit());
        assert!(!app.cmd.is_active());
    }

    #[test]
    fn tab_lists_the_command_candidates() {
        let (mut app, _dir) = app();
        press(&mut app, Key::Char(':'));
        type_text(&mut app, "l");
        press(&mut app, Key::Tab);
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let text = screen.text().join("\n");
        assert!(text.contains("list"), "candidates shown: {text}");
        assert!(text.contains("load"), "candidates shown: {text}");
        /* A repeated Tab inserts the first candidate. */
        press(&mut app, Key::Tab);
        assert_eq!(app.cmd.input_text(), "load");
    }

    #[test]
    fn the_settings_page_shows_the_selected_group_summary() {
        let (mut app, _dir) = app();
        let mut screen = Screen::new(200, 30);
        app.render(&mut screen);
        let summary = settings_summary(app.groups[app.group]);
        let head: String = summary.chars().take(16).collect();
        assert!(
            screen.text().join("\n").contains(&head),
            "summary {summary:?} is on the page"
        );
        /* It follows the group selection. */
        app.move_group(1);
        app.render(&mut screen);
        let summary = settings_summary(app.groups[app.group]);
        let head: String = summary.chars().take(16).collect();
        assert!(screen.text().join("\n").contains(&head));
    }

    fn i18n_group(text: &str) -> &'static str {
        SETTINGS
            .iter()
            .map(|def| def.group)
            .find(|group| *group == text)
            .unwrap_or_else(|| panic!("no group {text}"))
    }
}
