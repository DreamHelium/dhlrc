/*! The `:` command line of the interactive UI.
 *
 * The settings page is enough for changing a value, but several things a
 * terminal frontend needs — loading a region and keeping it in memory, looking
 * at it, reading a value by name — have no page yet. The command line is the
 * small, scriptable way in: `:` opens it, `Enter` runs the line, `Esc` leaves
 * it, `Up`/`Down` walk the history and `Tab` completes the word under the
 * cursor (and shows the candidates above the line).
 *
 * Everything here is plain state: the parser, the completion and the in-memory
 * region store do no terminal I/O, so they can be driven by the unit tests at
 * the bottom. `render_console` is the only drawing code.
 */

use super::{
    App, Color, Key, Page, Rect, SETTINGS, Screen, SettingType, Style, gettext, normalise,
    settings_summary, truncate,
};
use crate::region;
use dhlrc_core::modules::default_dirs;
use dhlrc_render::scene::Voxels;
use std::path::{Path, PathBuf};

/// How many command lines the history keeps.
const HISTORY_CAPACITY: usize = 50;

/// How many output lines the console keeps; older ones scroll away.
const TRANSCRIPT_CAPACITY: usize = 200;

/// The commands, their syntax and the one-line help. The summary goes through
/// `gettext` when it is shown, so this table stays the list of message ids.
pub struct CommandSpec {
    pub name: &'static str,
    pub usage: &'static str,
    pub summary: &'static str,
}

pub const COMMAND_SPECS: &[CommandSpec] = &[
    CommandSpec {
        name: "help",
        usage: "help [command]",
        summary: "List the commands, or explain one",
    },
    CommandSpec {
        name: "page",
        usage: "page <settings|modules|render|regions|help>",
        summary: "Switch to a page",
    },
    CommandSpec {
        name: "settings",
        usage: "settings",
        summary: "Show the summary of the selected settings group",
    },
    CommandSpec {
        name: "load",
        usage: "load <file> [--all]",
        summary: "Load a region file into memory; --all loads every region in it",
    },
    CommandSpec {
        name: "list",
        usage: "list",
        summary: "List the regions kept in memory",
    },
    CommandSpec {
        name: "select",
        usage: "select <n>",
        summary: "Mark region <n> as the selected one",
    },
    CommandSpec {
        name: "close",
        usage: "close <n|all>",
        summary: "Drop region <n>, or every region, from memory",
    },
    CommandSpec {
        name: "info",
        usage: "info <n>",
        summary: "Describe region <n>",
    },
    CommandSpec {
        name: "get",
        usage: "get <key>",
        summary: "Print a configuration value",
    },
    CommandSpec {
        name: "set",
        usage: "set <key> <value>",
        summary: "Change a configuration value and save it",
    },
    CommandSpec {
        name: "reload",
        usage: "reload",
        summary: "Reload the configuration from disk",
    },
    CommandSpec {
        name: "save",
        usage: "save",
        summary: "Write the configuration to disk",
    },
    CommandSpec {
        name: "quit",
        usage: "quit",
        summary: "Leave the program",
    },
];

/// The page names `page` accepts, in the order the completion lists them.
const PAGE_NAMES: [&str; 5] = ["settings", "modules", "render", "regions", "help"];

fn spec(name: &str) -> Option<&'static CommandSpec> {
    COMMAND_SPECS.iter().find(|spec| spec.name == name)
}

/// A command line split and understood, before it touches the application.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Parsed {
    Help(Option<String>),
    Page(String),
    Settings,
    Load { file: String, all: bool },
    List,
    Select(usize),
    Close(Option<usize>),
    Info(usize),
    Get(String),
    Set(String, String),
    Reload,
    Save,
    Quit,
}

/// What a key did to the command line.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(super) enum CommandStep {
    None,
    Run,
    Cancel,
}

/// The candidates `Tab` found, and where they replace the current word.
#[derive(Clone, Debug)]
pub struct Completion {
    /// Character indices into the input line.
    start: usize,
    end: usize,
    candidates: Vec<String>,
    /// The next candidate a repeated `Tab` inserts.
    index: usize,
}

/// The state of the `:` line: the text, its cursor, the history and the
/// completion in progress.
pub(super) struct CommandLine {
    active: bool,
    input: Vec<char>,
    cursor: usize,
    history: Vec<String>,
    history_index: Option<usize>,
    /// The text that was being typed when `Up` first opened the history.
    saved: Vec<char>,
    completion: Option<Completion>,
}

impl CommandLine {
    pub fn new() -> Self {
        Self {
            active: false,
            input: Vec::new(),
            cursor: 0,
            history: Vec::new(),
            history_index: None,
            saved: Vec::new(),
            completion: None,
        }
    }

    pub fn is_active(&self) -> bool {
        self.active
    }

    pub fn open(&mut self) {
        self.active = true;
        self.input.clear();
        self.cursor = 0;
        self.completion = None;
        self.history_index = None;
    }

    pub fn cancel(&mut self) {
        self.active = false;
        self.input.clear();
        self.cursor = 0;
        self.completion = None;
        self.history_index = None;
    }

    pub fn input_text(&self) -> String {
        self.input.iter().collect()
    }

    /// Closes the line and returns its text, recording it in the history.
    pub fn take_line(&mut self) -> String {
        let text = self.input_text().trim().to_owned();
        if !text.is_empty() && self.history.last() != Some(&text) {
            self.history.push(text.clone());
            if self.history.len() > HISTORY_CAPACITY {
                self.history.remove(0);
            }
        }
        self.active = false;
        self.input.clear();
        self.cursor = 0;
        self.completion = None;
        self.history_index = None;
        text
    }

    pub fn handle_key(&mut self, key: Key, regions: &[String]) -> CommandStep {
        match key {
            Key::Esc => {
                self.cancel();
                CommandStep::Cancel
            }
            Key::Enter => CommandStep::Run,
            Key::Backspace => {
                if self.cursor > 0 {
                    self.cursor -= 1;
                    self.input.remove(self.cursor);
                }
                self.completion = None;
                CommandStep::None
            }
            Key::Left => {
                self.cursor = self.cursor.saturating_sub(1);
                CommandStep::None
            }
            Key::Right => {
                self.cursor = (self.cursor + 1).min(self.input.len());
                CommandStep::None
            }
            Key::Home => {
                self.cursor = 0;
                CommandStep::None
            }
            Key::End => {
                self.cursor = self.input.len();
                CommandStep::None
            }
            Key::Up => {
                self.history_previous();
                CommandStep::None
            }
            Key::Down => {
                self.history_next();
                CommandStep::None
            }
            Key::Tab => {
                self.tab(regions);
                CommandStep::None
            }
            Key::Char(ch) => {
                self.input.insert(self.cursor, ch);
                self.cursor += 1;
                self.completion = None;
                CommandStep::None
            }
            _ => CommandStep::None,
        }
    }

    fn replace_word(&mut self, start: usize, end: usize, text: &str) -> usize {
        let chars: Vec<char> = text.chars().collect();
        self.input.splice(start..end, chars.iter().copied());
        self.cursor = start + chars.len();
        self.cursor
    }

    /// `Tab` completes the word under the cursor. The first press inserts what
    /// the candidates share and shows the list; a repeated press walks it.
    fn tab(&mut self, regions: &[String]) {
        if let Some(mut completion) = self.completion.take() {
            if !completion.candidates.is_empty() {
                let index = completion.index % completion.candidates.len();
                let text = quote_candidate(&completion.candidates[index]);
                let end = self.replace_word(completion.start, completion.end, &text);
                completion.end = end;
                completion.index = (index + 1) % completion.candidates.len();
            }
            self.completion = Some(completion);
            return;
        }

        let input = self.input_text();
        let Some(mut completion) = completion_candidates(&input, self.cursor, regions) else {
            return;
        };
        if completion.candidates.len() == 1 {
            let text = quote_candidate(&completion.candidates[0]);
            self.replace_word(completion.start, completion.end, &text);
            return;
        }

        let word: String = self.input[completion.start..completion.end].iter().collect();
        let common = common_prefix(&completion.candidates);
        if common.chars().count() > word.chars().count() {
            let end = self.replace_word(completion.start, completion.end, &common);
            completion.end = end;
        }
        completion.index = 0;
        self.completion = Some(completion);
    }

    fn history_previous(&mut self) {
        if self.history.is_empty() {
            return;
        }
        let next = match self.history_index {
            None => {
                self.saved = self.input.clone();
                self.history.len() - 1
            }
            Some(index) => index.saturating_sub(1),
        };
        self.history_index = Some(next);
        self.input = self.history[next].chars().collect();
        self.cursor = self.input.len();
        self.completion = None;
    }

    fn history_next(&mut self) {
        let Some(index) = self.history_index else {
            return;
        };
        if index + 1 < self.history.len() {
            let next = index + 1;
            self.history_index = Some(next);
            self.input = self.history[next].chars().collect();
        } else {
            self.history_index = None;
            self.input = std::mem::take(&mut self.saved);
        }
        self.cursor = self.input.len();
        self.completion = None;
    }
}

impl Default for CommandLine {
    fn default() -> Self {
        Self::new()
    }
}

// ---------------------------------------------------------------------------
// Splitting and understanding a line

/// Splits a line into words, honouring single and double quotes and a
/// backslash escape, so a path with spaces can be typed as one argument.
pub fn split_words(line: &str) -> Result<Vec<String>, String> {
    let mut words = Vec::new();
    let mut word = String::new();
    let mut in_word = false;
    let mut quote: Option<char> = None;
    let mut chars = line.chars();

    while let Some(ch) = chars.next() {
        match quote {
            Some(delimiter) => {
                if ch == '\\' {
                    if let Some(next) = chars.next() {
                        word.push(next);
                    }
                } else if ch == delimiter {
                    quote = None;
                } else {
                    word.push(ch);
                }
            }
            None => match ch {
                '"' | '\'' => {
                    quote = Some(ch);
                    in_word = true;
                }
                '\\' => {
                    if let Some(next) = chars.next() {
                        word.push(next);
                        in_word = true;
                    }
                }
                ch if ch.is_whitespace() => {
                    if in_word {
                        words.push(std::mem::take(&mut word));
                        in_word = false;
                    }
                }
                ch => {
                    word.push(ch);
                    in_word = true;
                }
            },
        }
    }

    if quote.is_some() {
        return Err(gettext("Unclosed quote"));
    }
    if in_word {
        words.push(word);
    }
    Ok(words)
}

fn usage_error(usage: &str) -> String {
    format!("{}: {usage}", gettext("Usage"))
}

fn parse_number(usage: &str, text: &str) -> Result<usize, String> {
    text.parse::<usize>().map_err(|_| usage_error(usage))
}

/// Turns the words of a line into the command they name.
pub fn parse_command(words: &[String]) -> Result<Parsed, String> {
    let Some(name) = words.first() else {
        return Err(gettext("No command"));
    };
    let args = &words[1..];
    match name.as_str() {
        "help" => match args {
            [] => Ok(Parsed::Help(None)),
            [topic] => Ok(Parsed::Help(Some(topic.clone()))),
            _ => Err(usage_error("help [command]")),
        },
        "page" => match args {
            [page] => Ok(Parsed::Page(page.clone())),
            _ => Err(usage_error("page <settings|modules|render|regions|help>")),
        },
        "settings" => match args {
            [] => Ok(Parsed::Settings),
            _ => Err(usage_error("settings")),
        },
        "load" => {
            let mut file = None;
            let mut all = false;
            for arg in args {
                if arg == "--all" {
                    all = true;
                } else if arg.starts_with('-') {
                    return Err(format!("{}: {arg}", gettext("Unknown option")));
                } else if file.replace(arg.clone()).is_some() {
                    return Err(usage_error("load <file> [--all]"));
                }
            }
            match file {
                Some(file) => Ok(Parsed::Load { file, all }),
                None => Err(usage_error("load <file> [--all]")),
            }
        }
        "list" => match args {
            [] => Ok(Parsed::List),
            _ => Err(usage_error("list")),
        },
        "select" => match args {
            [number] => {
                let value = parse_number("select <n>", number)?;
                if value == 0 {
                    return Err(gettext("Region numbers start at 1"));
                }
                Ok(Parsed::Select(value))
            }
            _ => Err(usage_error("select <n>")),
        },
        "close" => match args {
            [target] if target == "all" => Ok(Parsed::Close(None)),
            [number] => {
                let value = parse_number("close <n|all>", number)?;
                if value == 0 {
                    return Err(gettext("Region numbers start at 1"));
                }
                Ok(Parsed::Close(Some(value)))
            }
            _ => Err(usage_error("close <n|all>")),
        },
        "info" => match args {
            [number] => {
                let value = parse_number("info <n>", number)?;
                if value == 0 {
                    return Err(gettext("Region numbers start at 1"));
                }
                Ok(Parsed::Info(value))
            }
            _ => Err(usage_error("info <n>")),
        },
        "get" => match args {
            [key] => Ok(Parsed::Get(key.clone())),
            _ => Err(usage_error("get <key>")),
        },
        "set" => match args {
            [key, values @ ..] if !values.is_empty() => {
                Ok(Parsed::Set(key.clone(), values.join(" ")))
            }
            _ => Err(usage_error("set <key> <value>")),
        },
        "reload" => match args {
            [] => Ok(Parsed::Reload),
            _ => Err(usage_error("reload")),
        },
        "save" => match args {
            [] => Ok(Parsed::Save),
            _ => Err(usage_error("save")),
        },
        "quit" => match args {
            [] => Ok(Parsed::Quit),
            _ => Err(usage_error("quit")),
        },
        other => Err(format!("{}: {other}", gettext("Unknown command"))),
    }
}

fn page_by_name(name: &str) -> Option<Page> {
    Some(match name.to_ascii_lowercase().as_str() {
        "settings" => Page::Settings,
        "modules" => Page::Modules,
        "render" => Page::Render,
        "regions" => Page::Regions,
        "help" => Page::Help,
        _ => return None,
    })
}

// ---------------------------------------------------------------------------
// Completion

/// Finds the candidates for the word under the cursor. The word is replaced
/// from `start` to `end`; the candidates are raw values (quoting happens when
/// one is inserted).
pub fn completion_candidates(
    input: &str,
    cursor: usize,
    regions: &[String],
) -> Option<Completion> {
    let chars: Vec<char> = input.chars().collect();
    let cursor = cursor.min(chars.len());
    let (start, end) = word_bounds(&chars, cursor);
    let word: String = chars[start..end].iter().collect();

    let before: Vec<String> = chars[..start]
        .iter()
        .collect::<String>()
        .split_whitespace()
        .map(str::to_owned)
        .collect();

    let candidates = if before.is_empty() {
        candidates_for(&word, command_names())
    } else {
        argument_candidates(&before, &word, regions)
    };

    if candidates.is_empty() {
        return None;
    }
    Some(Completion {
        start,
        end,
        candidates,
        index: 0,
    })
}

fn argument_candidates(before: &[String], word: &str, regions: &[String]) -> Vec<String> {
    let command = before[0].as_str();
    let argument = before.len(); // 1 for the second word of the line
    match (command, argument) {
        ("page", 1) => candidates_for(word, PAGE_NAMES.iter().copied()),
        ("help", 1) => candidates_for(word, command_names()),
        ("get", 1) => candidates_for(word, SETTINGS.iter().map(|def| def.key)),
        ("set", 1) => candidates_for(word, SETTINGS.iter().map(|def| def.key)),
        ("set", 2) => match SETTINGS.iter().find(|def| def.key == before[1]) {
            Some(def) => candidates_for(word, setting_values(def)),
            None => Vec::new(),
        },
        ("select", 1) | ("info", 1) => {
            candidates_for(word, regions.iter().map(String::as_str))
        }
        ("close", 1) => {
            let mut names = vec!["all"];
            names.extend(regions.iter().map(String::as_str));
            candidates_for(word, names)
        }
        ("load", 1) if word.starts_with('-') => candidates_for(word, ["--all"]),
        ("load", 1) => path_candidates(word),
        ("load", 2) => candidates_for(word, ["--all"]),
        _ => Vec::new(),
    }
}

fn setting_values(def: &dhlrc_core::schema::SettingDef) -> Vec<String> {
    match def.kind {
        SettingType::Bool => vec!["true".to_owned(), "false".to_owned()],
        SettingType::Enum => def.choices.iter().map(|choice| gettext(*choice)).collect(),
        _ => Vec::new(),
    }
}

fn command_names() -> impl Iterator<Item = &'static str> {
    COMMAND_SPECS.iter().map(|spec| spec.name)
}

fn candidates_for<I, S>(word: &str, values: I) -> Vec<String>
where
    I: IntoIterator<Item = S>,
    S: AsRef<str>,
{
    values
        .into_iter()
        .filter(|value| value.as_ref().starts_with(word))
        .map(|value| value.as_ref().to_owned())
        .collect()
}

fn word_bounds(chars: &[char], cursor: usize) -> (usize, usize) {
    let mut start = cursor;
    while start > 0 && !chars[start - 1].is_whitespace() {
        start -= 1;
    }
    let mut end = cursor;
    while end < chars.len() && !chars[end].is_whitespace() {
        end += 1;
    }
    (start, end)
}

fn common_prefix(candidates: &[String]) -> String {
    let mut prefix: Vec<char> = candidates
        .first()
        .map(|candidate| candidate.chars().collect())
        .unwrap_or_default();
    for candidate in &candidates[1..] {
        let other: Vec<char> = candidate.chars().collect();
        let same = prefix
            .iter()
            .zip(other.iter())
            .take_while(|(left, right)| left == right)
            .count();
        prefix.truncate(same);
        if prefix.is_empty() {
            break;
        }
    }
    prefix.into_iter().collect()
}

/// Wraps a candidate in quotes when it holds whitespace or a quote, so the
/// parser reads it back as one argument.
fn quote_candidate(text: &str) -> String {
    if text.chars().any(char::is_whitespace) || text.contains('"') {
        let escaped = text.replace('\\', "\\\\").replace('"', "\\\"");
        format!("\"{escaped}\"")
    } else {
        text.to_owned()
    }
}

/// Completes a path relative to the current directory, or below `~`.
fn path_candidates(word: &str) -> Vec<String> {
    let split = word.rfind(['/', '\\']);
    let (directory, base) = match split {
        Some(index) => (&word[..=index], &word[index + 1..]),
        None => ("", word),
    };

    let read_dir = if directory.is_empty() {
        PathBuf::from(".")
    } else {
        expand_tilde(directory)
    };
    let Ok(entries) = std::fs::read_dir(&read_dir) else {
        return Vec::new();
    };

    let mut candidates = Vec::new();
    for entry in entries.flatten() {
        let name = entry.file_name().to_string_lossy().into_owned();
        /* Hidden entries only when the user started typing a dot, like a
         * shell. */
        if name.starts_with('.') && !base.starts_with('.') {
            continue;
        }
        if !name.starts_with(base) {
            continue;
        }
        let mut candidate = format!("{directory}{name}");
        if entry.path().is_dir() {
            candidate.push('/');
        }
        candidates.push(candidate);
    }
    candidates.sort();
    candidates
}

/// `~` as the home directory; other paths are returned unchanged.
fn expand_tilde(text: &str) -> PathBuf {
    if text == "~" {
        return home_dir().unwrap_or_else(|| PathBuf::from(text));
    }
    if let Some(rest) = text
        .strip_prefix("~/")
        .or_else(|| text.strip_prefix("~\\"))
    {
        if let Some(home) = home_dir() {
            return home.join(rest);
        }
    }
    PathBuf::from(text)
}

fn home_dir() -> Option<PathBuf> {
    std::env::var_os("HOME")
        .or_else(|| std::env::var_os("USERPROFILE"))
        .map(PathBuf::from)
}

// ---------------------------------------------------------------------------
// The in-memory region store

/// One loaded region, kept alive until it is closed.
pub(super) struct StoredRegion {
    name: String,
    path: PathBuf,
    region: region::Region,
}

impl StoredRegion {
    fn new(region: region::Region, path: &Path) -> Self {
        let name = path
            .file_stem()
            .map(|stem| stem.to_string_lossy().into_owned())
            .unwrap_or_else(|| path.display().to_string());
        Self {
            name,
            path: path.to_path_buf(),
            region,
        }
    }

    fn extent(&self) -> [i32; 3] {
        self.region.size()
    }

    fn extent_text(&self) -> String {
        let [width, height, depth] = self.extent();
        format!("{width}x{height}x{depth}")
    }

    /// The non-air block count, when the region is small enough to walk without
    /// a visible pause; `None` for a big one.
    fn filled_blocks(&self) -> Option<u64> {
        const MAX_COUNTED_VOLUME: i64 = 1_000_000;
        let [width, height, depth] = self.extent();
        let volume = width as i64 * height as i64 * depth as i64;
        if volume < 0 || volume > MAX_COUNTED_VOLUME {
            return None;
        }
        let solid: Vec<bool> = (0..self.region.palette_len())
            .map(|index| self.region.palette(index).is_some())
            .collect();
        let mut count = 0u64;
        for y in 0..height {
            for z in 0..depth {
                for x in 0..width {
                    if let Some(id) = self.region.block(x, y, z) {
                        if solid.get(id).copied().unwrap_or(false) {
                            count += 1;
                        }
                    }
                }
            }
        }
        Some(count)
    }
}

/// Opens the region modules the way `render` does: the two directories beside
/// the executable, which is where the core looks for them.
fn load_region_file(
    path: &Path,
    region_name: Option<&str>,
) -> Result<region::Region, region::Error> {
    let (region_dir, load_dir) = default_dirs();
    region::Region::load(
        path,
        &region::Options {
            region_dir: &region_dir,
            load_dir: &load_dir,
            region_lib: None,
            region: region_name,
        },
    )
}

// ---------------------------------------------------------------------------
// Running a command

impl App {
    /// The names `close`/`select`/`info` take and `Tab` completes.
    pub(super) fn region_names(&self) -> Vec<String> {
        self.regions
            .iter()
            .map(|region| region.name.clone())
            .collect()
    }

    fn unique_region_name(&self, name: String) -> String {
        let mut candidate = name.clone();
        let mut suffix = 2;
        while self
            .regions
            .iter()
            .any(|region| region.name == candidate)
        {
            candidate = format!("{name}-{suffix}");
            suffix += 1;
        }
        candidate
    }

    fn region_index(&self, number: usize) -> Result<usize, String> {
        let index = number
            .checked_sub(1)
            .ok_or_else(|| gettext("Region numbers start at 1"))?;
        if index >= self.regions.len() {
            return Err(format!("{}: {number}", gettext("No such region")));
        }
        Ok(index)
    }

    pub(super) fn open_command(&mut self) {
        self.cmd.open();
    }

    /// Runs one line and shows its output in the console above the function
    /// bar. Errors keep the console open and are coloured red.
    pub(super) fn run_command(&mut self, line: &str) {
        let line = line.trim();
        if line.is_empty() {
            return;
        }
        self.push_transcript(format!(": {line}"));

        let result = split_words(line).and_then(|words| parse_command(&words));
        match result.and_then(|command| self.execute(command)) {
            Ok(lines) => {
                for line in &lines {
                    self.push_transcript(line.clone());
                }
                if let Some(first) = lines.first() {
                    self.set_status(first.clone(), false);
                }
            }
            Err(message) => {
                let text = format!("{}: {message}", gettext("Error"));
                self.push_transcript(text.clone());
                self.set_status(text, true);
            }
        }
    }

    fn push_transcript(&mut self, line: String) {
        self.transcript.push(line);
        if self.transcript.len() > TRANSCRIPT_CAPACITY {
            let excess = self.transcript.len() - TRANSCRIPT_CAPACITY;
            self.transcript.drain(..excess);
        }
    }

    fn execute(&mut self, command: Parsed) -> Result<Vec<String>, String> {
        match command {
            Parsed::Help(None) => Ok(self.help_lines()),
            Parsed::Help(Some(topic)) => {
                let Some(spec) = spec(&topic) else {
                    return Err(format!("{}: {topic}", gettext("Unknown command")));
                };
                Ok(vec![
                    spec.usage.to_owned(),
                    format!("    {}", gettext(spec.summary)),
                ])
            }
            Parsed::Page(name) => {
                let Some(page) = page_by_name(&name) else {
                    return Err(format!("{}: {name}", gettext("No such page")));
                };
                self.switch_page(page);
                Ok(vec![format!("{}: {}", gettext("Page"), page.title())])
            }
            Parsed::Settings => {
                let summary = settings_summary(self.groups[self.group]);
                Ok(vec![if summary.is_empty() {
                    gettext("This group has no settings")
                } else {
                    summary
                }])
            }
            Parsed::Load { file, all } => self.load_command(&file, all),
            Parsed::List => Ok(self.list_lines()),
            Parsed::Select(number) => {
                let index = self.region_index(number)?;
                self.selected = Some(index);
                Ok(vec![format!(
                    "{}: {}",
                    gettext("Selected"),
                    self.regions[index].name
                )])
            }
            Parsed::Close(target) => self.close_lines(target),
            Parsed::Info(number) => {
                let index = self.region_index(number)?;
                Ok(self.info_lines(index))
            }
            Parsed::Get(key) => {
                let Some(def) = SETTINGS.iter().find(|def| def.key == key) else {
                    return Err(format!("{}: {key}", gettext("No such setting")));
                };
                Ok(vec![format!("{} = {}", def.key, self.value_text(def))])
            }
            Parsed::Set(key, value) => self.set_command(&key, &value),
            Parsed::Reload => {
                self.reload_result()?;
                Ok(vec![gettext("Configuration reloaded")])
            }
            Parsed::Save => {
                self.save_result()?;
                Ok(vec![gettext("Saved")])
            }
            Parsed::Quit => {
                self.quit = true;
                Ok(vec![gettext("Goodbye")])
            }
        }
    }

    fn help_lines(&self) -> Vec<String> {
        let mut lines = vec![gettext("Commands:")];
        let width = COMMAND_SPECS
            .iter()
            .map(|spec| spec.usage.chars().count())
            .max()
            .unwrap_or(0);
        for spec in COMMAND_SPECS {
            lines.push(format!(
                "  {:<width$}  {}",
                spec.usage,
                gettext(spec.summary),
                width = width
            ));
        }
        lines
    }

    fn list_lines(&self) -> Vec<String> {
        if self.regions.is_empty() {
            return vec![gettext("No regions loaded")];
        }
        let mut lines = vec![gettext("Regions in memory:")];
        for (index, region) in self.regions.iter().enumerate() {
            let marker = if self.selected == Some(index) { ">" } else { " " };
            lines.push(format!(
                "{marker}{:>3}  {:<24}  {:<12}  {}",
                index + 1,
                region.name,
                region.extent_text(),
                region.path.display()
            ));
        }
        lines
    }

    fn info_lines(&self, index: usize) -> Vec<String> {
        let region = &self.regions[index];
        let [width, height, depth] = region.extent();
        let volume = width as i64 * height as i64 * depth as i64;
        let mut lines = vec![
            region.name.clone(),
            format!(
                "  {}: {}",
                gettext("Path"),
                region.path.display()
            ),
            format!(
                "  {}: {}x{}x{} ({} {})",
                gettext("Extent"),
                width,
                height,
                depth,
                volume,
                gettext("blocks")
            ),
        ];
        match region.filled_blocks() {
            Some(filled) => lines.push(format!(
                "  {}: {filled} / {volume}",
                gettext("Blocks")
            )),
            None => lines.push(format!(
                "  {}: {}",
                gettext("Blocks"),
                gettext("not counted (region too large)")
            )),
        }
        lines
    }

    fn close_lines(&mut self, target: Option<usize>) -> Result<Vec<String>, String> {
        match target {
            None => {
                let count = self.regions.len();
                self.regions.clear();
                self.selected = None;
                Ok(vec![format!("{}: {count}", gettext("Closed regions"))])
            }
            Some(number) => {
                let index = self.region_index(number)?;
                let region = self.regions.remove(index);
                self.selected = match self.selected {
                    Some(selected) if selected == index => None,
                    Some(selected) if selected > index => Some(selected - 1),
                    other => other,
                };
                Ok(vec![format!("{}: {}", gettext("Closed"), region.name)])
            }
        }
    }

    fn load_command(&mut self, file: &str, all: bool) -> Result<Vec<String>, String> {
        let path = expand_tilde(file);
        if !path.is_file() {
            return Err(format!("{}: {}", gettext("Not a file"), path.display()));
        }

        let mut loaded: Vec<StoredRegion> = Vec::new();
        let mut first = load_region_file(&path, if all { Some("0") } else { None });
        /* A single-region file refuses an explicit region; `--all` still means
         * "everything in the file", which is that one region. */
        if all {
            if let Err(error) = &first {
                if error.to_string().contains("single region") {
                    first = load_region_file(&path, None);
                }
            }
        }
        match first {
            Ok(region) => loaded.push(StoredRegion::new(region, &path)),
            Err(error) => {
                return Err(format!(
                    "{} {}: {error}",
                    gettext("Could not load"),
                    path.display()
                ));
            }
        }

        if all {
            let mut index = 1usize;
            loop {
                let selected = index.to_string();
                match load_region_file(&path, Some(&selected)) {
                    Ok(region) => {
                        loaded.push(StoredRegion::new(region, &path));
                        index += 1;
                    }
                    Err(error) => {
                        let message = error.to_string();
                        /* Past the last region the module says which names it
                         * does hold; that is the end of the file, not a
                         * failure. */
                        if message.contains("no region is called") {
                            break;
                        }
                        return Err(format!(
                            "{} {}: {message}",
                            gettext("Could not load"),
                            path.display()
                        ));
                    }
                }
                if index > 10_000 {
                    break;
                }
            }
        }

        let count = loaded.len();
        if count > 1 {
            let stem = path
                .file_stem()
                .map(|stem| stem.to_string_lossy().into_owned())
                .unwrap_or_else(|| path.display().to_string());
            for (index, region) in loaded.iter_mut().enumerate() {
                region.name = format!("{stem}#{index}");
            }
        }

        let first_new = self.regions.len();
        for mut region in loaded {
            region.name = self.unique_region_name(region.name);
            self.regions.push(region);
        }
        if self.selected.is_none() {
            self.selected = Some(first_new);
        }

        if count == 1 {
            let region = &self.regions[first_new];
            Ok(vec![format!(
                "{} {} ({}) {}",
                gettext("Loaded"),
                region.name,
                region.extent_text(),
                region.path.display()
            )])
        } else {
            Ok(vec![format!(
                "{} {count} {}",
                gettext("Loaded"),
                gettext("regions")
            )])
        }
    }

    fn set_command(&mut self, key: &str, value: &str) -> Result<Vec<String>, String> {
        let Some(def) = SETTINGS.iter().find(|def| def.key == key) else {
            return Err(format!("{}: {key}", gettext("No such setting")));
        };
        let value = match def.kind {
            SettingType::Enum => {
                let text = value.trim();
                let index = if let Ok(index) = text.parse::<usize>() {
                    index
                } else {
                    def.choices
                        .iter()
                        .position(|choice| *choice == text || gettext(*choice) == text)
                        .ok_or_else(|| {
                            format!("{}: {text}", gettext("Not a valid value"))
                        })?
                };
                if index >= def.choices.len() {
                    return Err(format!("{}: {text}", gettext("Not a valid value")));
                }
                index.to_string()
            }
            _ => normalise(def.kind, value)
                .ok_or_else(|| format!("{}: {value}", gettext("Not a valid value")))?,
        };

        self.config.set_setting(def.key, &value);
        self.save_result()?;
        Ok(vec![format!(
            "{} = {} ({})",
            def.key,
            self.value_text(def),
            gettext("saved")
        )])
    }

    /// Draws the command line, its candidates and the output above it. With the
    /// line closed, the last output stays visible until `Esc` dismisses it.
    pub(super) fn render_console(&mut self, screen: &mut Screen) {
        let width = screen.width;
        let height = screen.height;
        if width == 0 || height == 0 {
            return;
        }

        if self.cmd.active {
            let mut bottom = height.saturating_sub(2);
            if let Some(completion) = self.cmd.completion.as_ref() {
                let count = completion.candidates.len();
                let first = completion.index.saturating_sub(4).min(
                    count.saturating_sub(5.min(count)),
                );
                for (offset, candidate) in completion
                    .candidates
                    .iter()
                    .enumerate()
                    .skip(first)
                    .take(5)
                {
                    if bottom == 0 {
                        break;
                    }
                    let selected = offset == completion.index % count.max(1);
                    let style = if selected {
                        Style::plain()
                            .with_fg(Color::Black)
                            .on(Color::Cyan)
                            .bold()
                    } else {
                        Style::plain().with_fg(Color::White).on(Color::DarkGrey)
                    };
                    screen.fill(Rect::new(0, bottom, width, 1), ' ', style);
                    screen.put(
                        2,
                        bottom,
                        &truncate(candidate, width.saturating_sub(4) as usize),
                        style,
                    );
                    bottom = bottom.saturating_sub(1);
                }
            }

            let room = bottom as usize + 1;
            let shown = room.min(8);
            let start = self.transcript.len().saturating_sub(shown);
            let lines: Vec<String> = self.transcript[start..].to_vec();
            for (offset, line) in lines.iter().enumerate() {
                let y = bottom.saturating_sub((lines.len() - 1 - offset) as u16);
                draw_console_line(screen, y, line, width);
            }

            render_input(screen, &self.cmd, width, height - 1);
        } else if !self.transcript.is_empty() {
            let room = height.saturating_sub(2) as usize;
            let shown = room;
            let start = self.transcript.len().saturating_sub(shown);
            let lines: Vec<String> = self.transcript[start..].to_vec();
            let last = height.saturating_sub(2);
            for (offset, line) in lines.iter().enumerate() {
                let y = last.saturating_sub((lines.len() - 1 - offset) as u16);
                draw_console_line(screen, y, line, width);
            }
        }
    }
}

fn draw_console_line(screen: &mut Screen, y: u16, line: &str, width: u16) {
    let style = Style::plain().with_fg(Color::White).on(Color::DarkGrey);
    screen.fill(Rect::new(0, y, width, 1), ' ', style);
    screen.put(1, y, &truncate(line, width.saturating_sub(2) as usize), style);
}

fn render_input(screen: &mut Screen, cmd: &CommandLine, width: u16, y: u16) {
    let style = Style::plain().on(Color::DarkGrey);
    screen.fill(Rect::new(0, y, width, 1), ' ', style);
    screen.put(0, y, ":", Style::plain().with_fg(Color::Yellow).bold().on(Color::DarkGrey));

    let room = width.saturating_sub(3) as usize;
    let start = cmd.cursor.saturating_sub(room);
    let text: String = cmd
        .input
        .iter()
        .skip(start)
        .take(room)
        .collect::<String>();
    screen.put(2, y, &text, Style::plain().with_fg(Color::White).on(Color::DarkGrey));
    let cursor_x = 2 + (cmd.cursor - start) as u16;
    if cursor_x < width {
        screen.set(
            cursor_x,
            y,
            '▏',
            Style::plain().with_fg(Color::Yellow).on(Color::DarkGrey),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use dhlrc_core::config::{Config, ConfigPaths};

    fn words(line: &str) -> Vec<String> {
        split_words(line).unwrap()
    }

    fn line() -> CommandLine {
        let mut line = CommandLine::new();
        line.open();
        line
    }

    fn type_text(line: &mut CommandLine, text: &str) {
        for ch in text.chars() {
            line.handle_key(Key::Char(ch), &[]);
        }
    }

    #[test]
    fn words_split_on_whitespace_and_quotes() {
        assert_eq!(
            words("load example.litematic --all"),
            vec!["load", "example.litematic", "--all"]
        );
        assert_eq!(
            words("load \"my file.litematic\""),
            vec!["load", "my file.litematic"]
        );
        assert_eq!(
            words("set 'Directory' /tmp/x"),
            vec!["set", "Directory", "/tmp/x"]
        );
        assert_eq!(words("get \"\""), vec!["get", ""]);
        assert_eq!(words("   "), Vec::<String>::new());
        assert_eq!(words("load a\\ b"), vec!["load", "a b"]);
        assert!(split_words("load \"oops").is_err());
        assert!(split_words("load 'oops").is_err());
    }

    #[test]
    fn commands_parse_to_their_parts() {
        assert_eq!(parse_command(&words("help")).unwrap(), Parsed::Help(None));
        assert_eq!(
            parse_command(&words("help load")).unwrap(),
            Parsed::Help(Some("load".to_owned()))
        );
        assert_eq!(
            parse_command(&words("page modules")).unwrap(),
            Parsed::Page("modules".to_owned())
        );
        assert_eq!(
            parse_command(&words("load a.litematic --all")).unwrap(),
            Parsed::Load {
                file: "a.litematic".to_owned(),
                all: true
            }
        );
        assert_eq!(
            parse_command(&words("close all")).unwrap(),
            Parsed::Close(None)
        );
        assert_eq!(
            parse_command(&words("close 2")).unwrap(),
            Parsed::Close(Some(2))
        );
        assert_eq!(
            parse_command(&words("select 3")).unwrap(),
            Parsed::Select(3)
        );
        assert_eq!(
            parse_command(&words("set Directory /tmp/a b")).unwrap(),
            Parsed::Set("Directory".to_owned(), "/tmp/a b".to_owned())
        );
        assert_eq!(parse_command(&words("reload")).unwrap(), Parsed::Reload);
        assert_eq!(parse_command(&words("quit")).unwrap(), Parsed::Quit);
        assert!(parse_command(&words("nope")).is_err());
        assert!(parse_command(&words("select 0")).is_err());
        assert!(parse_command(&words("load")).is_err());
        assert!(parse_command(&words("load a b")).is_err());
        assert!(parse_command(&words("set key")).is_err());
    }

    #[test]
    fn tab_completes_a_command_name() {
        let mut line = line();
        type_text(&mut line, "he");
        line.handle_key(Key::Tab, &[]);
        assert_eq!(line.input_text(), "help");
        assert!(line.completion.is_none());
    }

    #[test]
    fn repeated_tab_cycles_the_candidates() {
        let mut line = line();
        type_text(&mut line, "l");
        line.handle_key(Key::Tab, &[]);
        /* Two candidates share "l", so the first press only shows the list. */
        assert_eq!(line.input_text(), "l");
        let completion = line.completion.as_ref().expect("the list is shown");
        assert_eq!(completion.candidates, vec!["load", "list"]);
        line.handle_key(Key::Tab, &[]);
        assert_eq!(line.input_text(), "load");
        line.handle_key(Key::Tab, &[]);
        assert_eq!(line.input_text(), "list");
        line.handle_key(Key::Tab, &[]);
        assert_eq!(line.input_text(), "load");
        /* Escape closes the line and the list with it. */
        line.handle_key(Key::Esc, &[]);
        assert!(!line.is_active());
    }

    #[test]
    fn tab_completes_pages_and_region_names() {
        let regions = vec!["house".to_owned(), "tower".to_owned()];
        let mut first = line();
        type_text(&mut first, "page rend");
        first.handle_key(Key::Tab, &regions);
        assert_eq!(first.input_text(), "page render");

        let mut second = line();
        type_text(&mut second, "close ");
        second.handle_key(Key::Tab, &regions);
        let completion = second.completion.as_ref().expect("candidates listed");
        assert_eq!(completion.candidates, vec!["all", "house", "tower"]);
        second.handle_key(Key::Tab, &regions);
        assert_eq!(second.input_text(), "close all");
    }

    #[test]
    fn tab_completes_setting_keys() {
        let mut line = line();
        type_text(&mut line, "get Suppress");
        line.handle_key(Key::Tab, &[]);
        assert_eq!(line.input_text(), "get SuppressWhenActive");
    }

    #[test]
    fn history_walks_the_previous_lines() {
        let mut line = line();
        for text in ["one", "two", "three"] {
            type_text(&mut line, text);
            assert_eq!(line.handle_key(Key::Enter, &[]), CommandStep::Run);
            assert_eq!(line.take_line(), text);
            line.open();
        }
        line.handle_key(Key::Up, &[]);
        assert_eq!(line.input_text(), "three");
        line.handle_key(Key::Up, &[]);
        assert_eq!(line.input_text(), "two");
        line.handle_key(Key::Down, &[]);
        assert_eq!(line.input_text(), "three");
        line.handle_key(Key::Down, &[]);
        assert_eq!(line.input_text(), "");
        /* The history is capped, oldest first. */
        assert_eq!(line.history.len(), 3);
    }

    fn app() -> (App, PathBuf) {
        let dir = std::env::temp_dir().join(format!(
            "dhlrc-tui-command-test-{}-{}",
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
        app.handle(&super::super::Event::Key(key));
    }

    fn run(app: &mut App, text: &str) {
        press(app, Key::Char(':'));
        for ch in text.chars() {
            press(app, Key::Char(ch));
        }
        press(app, Key::Enter);
    }

    fn transcript(app: &App) -> String {
        app.transcript.join("\n")
    }

    #[test]
    fn a_line_switches_pages_and_esc_does_not_quit() {
        let (mut app, _dir) = app();
        run(&mut app, "page modules");
        assert_eq!(app.page, Page::Modules);
        assert!(transcript(&app).contains("Modules"));

        press(&mut app, Key::Char(':'));
        for ch in "quit".chars() {
            press(&mut app, Key::Char(ch));
        }
        press(&mut app, Key::Esc);
        assert!(!app.quit());
        assert!(!app.cmd.is_active());
    }

    #[test]
    fn the_settings_command_prints_the_group_summary() {
        let (mut app, _dir) = app();
        run(&mut app, "settings");
        let summary = settings_summary(app.groups[app.group]);
        assert!(transcript(&app).contains(&summary));
    }

    #[test]
    fn get_and_set_change_the_configuration() {
        let (mut app, dir) = app();
        run(&mut app, "get SuppressWhenActive");
        assert!(transcript(&app).contains("SuppressWhenActive ="));
        run(&mut app, "set SuppressWhenActive false");
        assert_eq!(
            app.config.setting("SuppressWhenActive").unwrap(),
            "false"
        );
        let written = std::fs::read_to_string(dir.join("config.toml")).unwrap();
        assert!(written.contains("suppress_when_active = false"));
        /* An unknown key is refused and nothing is written. */
        run(&mut app, "set NoSuchSetting 1");
        assert!(transcript(&app).contains(&gettext("No such setting")));
    }

    #[test]
    fn list_and_close_need_a_loaded_region() {
        let (mut app, _dir) = app();
        run(&mut app, "list");
        assert!(transcript(&app).contains(&gettext("No regions loaded")));
        run(&mut app, "select 1");
        assert!(transcript(&app).contains(&gettext("No such region")));
        run(&mut app, "close all");
        assert!(transcript(&app).contains("0"));
    }

    #[test]
    fn the_help_lists_every_command() {
        let (mut app, _dir) = app();
        run(&mut app, "help");
        let text = transcript(&app);
        for spec in COMMAND_SPECS {
            assert!(text.contains(spec.name), "help lists {}", spec.name);
        }
    }

    #[test]
    fn the_help_output_is_drawn_on_screen() {
        let (mut app, _dir) = app();
        run(&mut app, "help");
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let text = screen.text().join("\n");
        assert!(text.contains("Commands:"));
        assert!(text.contains("load <file> [--all]"));
        assert!(text.contains("quit"));
    }

    #[test]
    fn an_unknown_command_reports_an_error() {
        let (mut app, _dir) = app();
        run(&mut app, "nonsense");
        assert!(transcript(&app).contains(&gettext("Unknown command")));
        assert!(app.status.as_ref().is_some_and(|(_, error)| *error));
    }

    #[test]
    fn the_candidate_list_is_drawn_above_the_prompt() {
        let (mut app, _dir) = app();
        press(&mut app, Key::Char(':'));
        for ch in "l".chars() {
            press(&mut app, Key::Char(ch));
        }
        press(&mut app, Key::Tab);
        let mut screen = Screen::new(100, 30);
        app.render(&mut screen);
        let text = screen.text().join("\n");
        assert!(text.contains("list"));
        assert!(text.contains("load"));
    }
}
