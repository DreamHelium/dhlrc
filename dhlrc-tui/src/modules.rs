/*! The `modules` command.
 *
 * It reports what the core can load from `region_module/` and `load_module/`
 * and, when both lists are shown, points out the region modules whose object
 * codec is not installed — the ones that would be offered a file but could
 * never read it. The core does the opening and the metadata reading; this only
 * lays the result out.
 *
 * The default directories sit next to the *binary*, which for a cargo build is
 * `target/debug/`, so the flags are the way to point at a Qt build's
 * `region_module/` and `load_module/`.
 */

use crate::cli::{Kind, ModulesArgs};
use crate::i18n::gettext;
use dhlrc_core::modules::{Modules, default_dirs};
use formatx::formatx;
use std::ffi::CString;
use std::fmt::Write as _;
use std::path::Path;
use std::process::ExitCode;
use unicode_width::UnicodeWidthStr;

pub fn run(args: ModulesArgs) -> ExitCode {
    let (default_region, default_load) = default_dirs();
    let region_dir = args.region_dir.unwrap_or(default_region);
    let load_dir = args.load_dir.unwrap_or(default_load);

    /* Both directories are always opened, so the analysis below can compare the
     * two lists whatever the display filter is. */
    let modules = Modules::load(&region_dir, &load_dir);

    let mut sections = Vec::new();
    if args.kind != Kind::Load {
        sections.push(render_regions(&modules, &region_dir, args.verbose));
    }
    if args.kind != Kind::Region {
        sections.push(render_codecs(&modules, &load_dir, args.verbose));
    }
    if args.kind == Kind::All {
        let analysis = render_analysis(&modules);
        if !analysis.is_empty() {
            sections.push(analysis);
        }
    }

    print!("{}", sections.join("\n"));
    ExitCode::SUCCESS
}

fn render_regions(modules: &Modules, dir: &Path, verbose: bool) -> String {
    let count = modules.region_count();
    let mut out = String::new();
    let _ = writeln!(out, "{} ({count})", gettext("Region modules"));
    let _ = writeln!(out, "  {}", dir.display());
    if count == 0 {
        let _ = writeln!(out, "  {}", gettext("none found"));
        return out;
    }

    let mut header = vec![
        "#".to_string(),
        gettext("type"),
        gettext("suffix"),
        gettext("codec"),
        gettext("regions"),
        gettext("encoding"),
    ];
    if verbose {
        header.push(gettext("file"));
    }
    let mut rows = vec![header];

    for index in 0..count {
        let Some(info) = modules.region_info(index) else {
            continue;
        };
        let mut row = vec![
            index.to_string(),
            text(info.type_name.as_ref()),
            text(info.file_suffix.as_ref()),
            text(info.base_type.as_ref()),
            if info.is_multi {
                gettext("multi")
            } else {
                gettext("single")
            },
            encoding_name(info.encoding),
        ];
        if verbose {
            row.push(info.file.to_string_lossy().into_owned());
        }
        rows.push(row);
    }

    out.push_str(&render_rows(&rows));
    out
}

fn render_codecs(modules: &Modules, dir: &Path, verbose: bool) -> String {
    let count = modules.load_count();
    let mut out = String::new();
    let _ = writeln!(out, "{} ({count})", gettext("Object codecs"));
    let _ = writeln!(out, "  {}", dir.display());
    if count == 0 {
        let _ = writeln!(out, "  {}", gettext("none found"));
        return out;
    }

    let mut header = vec!["#".to_string(), gettext("codec")];
    if verbose {
        header.push(gettext("file"));
    }
    let mut rows = vec![header];

    for index in 0..count {
        let Some(info) = modules.load_info(index) else {
            continue;
        };
        let mut row = vec![index.to_string(), text(info.base_type.as_ref())];
        if verbose {
            row.push(info.file.to_string_lossy().into_owned());
        }
        rows.push(row);
    }

    out.push_str(&render_rows(&rows));
    out
}

/// Reports the region modules that name a codec which no installed codec
/// provides. Such a module is knowingly left in the load path (its suffix still
/// matches files), but files it claims cannot be decoded. Empty when there is
/// nothing to report, so the caller can leave the section out.
fn render_analysis(modules: &Modules) -> String {
    let codecs: Vec<String> = (0..modules.load_count())
        .filter_map(|index| modules.load_info(index))
        .filter_map(|info| info.base_type.as_ref())
        .map(|base| base.to_string_lossy().into_owned())
        .collect();

    let mut missing = Vec::new();
    for index in 0..modules.region_count() {
        let Some(info) = modules.region_info(index) else {
            continue;
        };
        let Some(base) = info.base_type.as_ref() else {
            continue;
        };
        let base = base.to_string_lossy().into_owned();
        if !codecs.contains(&base) {
            missing.push((text(info.type_name.as_ref()), base));
        }
    }

    if missing.is_empty() {
        return String::new();
    }

    let mut out = String::new();
    let _ = writeln!(out, "{}", gettext("Warnings"));
    for (module, codec) in missing {
        let _ = writeln!(
            out,
            "  - {}",
            formatx!(
                gettext("Region module \"{}\" needs codec \"{}\", which is not installed."),
                module,
                codec
            )
            .unwrap_or_default()
        );
    }
    out
}

/// Lays `rows` out as columns, each row indented by two spaces. Widths are
/// taken from the widest cell, so the last column stays unpadded (nothing
/// follows it to align with). Width is measured in terminal cells, not
/// characters, so a translated cell holding double-width characters still lines
/// up.
fn render_rows(rows: &[Vec<String>]) -> String {
    let mut widths: Vec<usize> = Vec::new();
    for row in rows {
        for (column, cell) in row.iter().enumerate() {
            if widths.len() <= column {
                widths.push(0);
            }
            widths[column] = widths[column].max(cell.width());
        }
    }

    let mut output = String::new();
    for row in rows {
        output.push_str("  ");
        for (column, cell) in row.iter().enumerate() {
            output.push_str(cell);
            if column + 1 == row.len() {
                break;
            }
            output.push_str(&" ".repeat(widths[column] - cell.width() + 2));
        }
        output.push('\n');
    }
    output
}

/// A module string, or `-` when the module does not provide it.
fn text(value: Option<&CString>) -> String {
    value
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_else(|| "-".to_string())
}

/// The name of an `ObjectEncoding` value (0 any, 1/2/3 the named encodings).
fn encoding_name(encoding: i32) -> String {
    match encoding {
        1 => gettext("big-endian"),
        2 => gettext("little-endian"),
        3 => gettext("network little-endian"),
        _ => gettext("any"),
    }
}

#[cfg(test)]
mod tests {
    use super::{encoding_name, render_rows, text};
    use unicode_width::UnicodeWidthStr;

    fn row(cells: &[&str]) -> Vec<String> {
        cells.iter().map(|cell| (*cell).to_string()).collect()
    }

    #[test]
    fn columns_are_aligned_and_the_last_cell_is_unpadded() {
        let rows = vec![
            row(&["#", "type", "suffix"]),
            row(&["0", "nbt", "nbt"]),
            row(&["1", "litematic", "litematic"]),
        ];
        let rendered = render_rows(&rows);
        let lines: Vec<&str> = rendered.lines().collect();
        assert_eq!(lines.len(), 3);
        assert_eq!(lines[0], "  #  type       suffix");
        assert_eq!(lines[1], "  0  nbt        nbt");
        assert_eq!(lines[2], "  1  litematic  litematic");
        for line in &lines {
            assert!(!line.ends_with(' '), "{line:?} should not be padded");
        }
    }

    #[test]
    fn double_width_cells_line_up_like_ascii_ones() {
        /* "类型" is four cells wide, so the next column has to start in the same
         * place as it does after "nbt ". */
        let rows = vec![row(&["类型", "后缀"]), row(&["nbt", "nbt"])];
        let rendered = render_rows(&rows);
        let lines: Vec<&str> = rendered.lines().collect();
        let cjk = lines[0].width() - "后缀".width();
        let ascii = lines[1].width() - "nbt".width();
        assert_eq!(cjk, ascii);
    }

    #[test]
    fn a_value_the_module_does_not_provide_is_a_dash() {
        assert_eq!(text(None), "-");
    }

    #[test]
    fn every_encoding_has_a_name() {
        assert_eq!(encoding_name(0), "any");
        assert_eq!(encoding_name(1), "big-endian");
        assert_eq!(encoding_name(2), "little-endian");
        assert_eq!(encoding_name(3), "network little-endian");
        /* Anything unknown is treated as "any", like the core does. */
        assert_eq!(encoding_name(99), "any");
    }
}
