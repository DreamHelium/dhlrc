/*! A small Markdown reader for the character grid.
 *
 * The core and the region modules write messages for a Markdown-capable
 * frontend: `**bold**`, inline code in backticks, headings, bullets, quotes
 * and links. This page is a plain character grid, so one line becomes styled
 * spans — markers disappear, emphasis becomes the bold attribute and inline
 * code gets its own colour.
 *
 * Plain text is the common case and must come out character for character:
 * a marker only counts when it is complete and at a word boundary, so text
 * like `object_base_type`, `2 * 3` or an unpaired backtick is left alone.
 */

use super::{Color, Screen, Style};
use unicode_width::UnicodeWidthChar;

/// A run of text that shares one style.
pub struct Span {
    pub text: String,
    pub style: Style,
}

/// Parses one line into spans. `base` styles the plain text; bold and code
/// are derived from it.
pub fn line(text: &str, base: Style) -> Vec<Span> {
    let text = text.trim_end_matches('\r');
    let mut spans: Vec<Span> = Vec::new();
    let mut rest = text;
    let mut style = base;
    if let Some(after) = heading(rest) {
        rest = after;
        style = base.bold();
    } else if let Some(after) = rest.strip_prefix("> ") {
        push(&mut spans, "│ ", base);
        rest = after;
    } else if let Some(after) = list_item(rest) {
        push(&mut spans, "• ", base);
        rest = after;
    }
    inline(rest, style, &mut spans);
    spans
}

/// Draws `spans` from `(x, y)` using at most `limit` cells, ending a line that
/// does not fit with an ellipsis. Returns the column after the last cell.
pub fn draw(screen: &mut Screen, x: u16, y: u16, spans: &[Span], limit: u16, base: Style) -> u16 {
    let limit = limit as usize;
    if limit == 0 {
        return x;
    }
    let total: usize = spans
        .iter()
        .flat_map(|span| span.text.chars())
        .map(|ch| ch.width().unwrap_or(0))
        .sum();
    let clipped = total > limit;
    /* One cell is kept for the ellipsis. */
    let budget = if clipped { limit - 1 } else { limit };
    let mut used = 0usize;
    'draw: for span in spans {
        for ch in span.text.chars() {
            let width = ch.width().unwrap_or(0) as usize;
            if width == 0 {
                continue;
            }
            if used + width > budget {
                break 'draw;
            }
            screen.set(x + used as u16, y, ch, span.style);
            if width == 2 {
                screen.set(x + used as u16 + 1, y, '\0', span.style);
            }
            used += width;
        }
    }
    if clipped {
        let style = spans.last().map(|span| span.style).unwrap_or(base);
        screen.set(x + used as u16, y, '…', style);
        used += 1;
    }
    x + used as u16
}

/// A heading: one to six `#` followed by a space.
fn heading(text: &str) -> Option<&str> {
    let hashes = text.chars().take_while(|ch| *ch == '#').count();
    if (1..=6).contains(&hashes) && text[hashes..].starts_with(' ') {
        Some(&text[hashes + 1..])
    } else {
        None
    }
}

/// A bullet list item; the marker becomes a bullet.
fn list_item(text: &str) -> Option<&str> {
    ["- ", "* ", "+ "]
        .iter()
        .find_map(|prefix| text.strip_prefix(prefix))
}

fn push(spans: &mut Vec<Span>, text: &str, style: Style) {
    if !text.is_empty() {
        spans.push(Span {
            text: text.to_owned(),
            style,
        });
    }
}

fn flush(plain: &mut String, style: Style, spans: &mut Vec<Span>) {
    if !plain.is_empty() {
        spans.push(Span {
            text: std::mem::take(plain),
            style,
        });
    }
}

/// Parses the inline constructs of one line into `spans`.
fn inline(text: &str, style: Style, spans: &mut Vec<Span>) {
    let chars: Vec<char> = text.chars().collect();
    let mut plain = String::new();
    let mut at = 0;
    while at < chars.len() {
        match chars[at] {
            '`' => {
                if let Some(end) = (at + 1..chars.len()).find(|&index| chars[index] == '`') {
                    flush(&mut plain, style, spans);
                    let code: String = chars[at + 1..end].iter().collect();
                    push(spans, &code, style.with_fg(Color::Yellow));
                    at = end + 1;
                } else {
                    plain.push('`');
                    at += 1;
                }
            }
            '*' | '_' => {
                if let Some((end, double)) = emphasis(&chars, at) {
                    flush(&mut plain, style, spans);
                    let from = at + if double { 2 } else { 1 };
                    let inner: String = chars[from..end].iter().collect();
                    push(spans, &inner, if double { style.bold() } else { style });
                    at = end + if double { 2 } else { 1 };
                } else {
                    plain.push(chars[at]);
                    at += 1;
                }
            }
            '[' => {
                if let Some((label, url, next)) = link(&chars, at) {
                    flush(&mut plain, style, spans);
                    push(spans, &format!("{label} ({url})"), style);
                    at = next;
                } else {
                    plain.push('[');
                    at += 1;
                }
            }
            ch => {
                plain.push(ch);
                at += 1;
            }
        }
    }
    flush(&mut plain, style, spans);
}

/// Ends of an emphasis run: `(index of the closing marker, is it doubled)`.
/// Emphasis only counts at word boundaries, so identifiers and arithmetic
/// keep their characters.
fn emphasis(chars: &[char], start: usize) -> Option<(usize, bool)> {
    let marker = chars[start];
    if start > 0 && chars[start - 1].is_alphanumeric() {
        return None;
    }
    let double = start + 1 < chars.len() && chars[start + 1] == marker;
    let from = start + if double { 2 } else { 1 };
    let mut at = from + 1;
    while at < chars.len() {
        let close = chars[at] == marker
            && if double {
                at + 1 < chars.len() && chars[at + 1] == marker
            } else {
                !(at + 1 < chars.len() && chars[at + 1] == marker)
            };
        let after = at + if double { 2 } else { 1 };
        if close && (after >= chars.len() || !chars[after].is_alphanumeric()) {
            return Some((at, double));
        }
        at += 1;
    }
    None
}

/// A `[label](target)` link: the label followed by the target in parentheses.
fn link(chars: &[char], start: usize) -> Option<(String, String, usize)> {
    if chars.get(start) != Some(&'[') {
        return None;
    }
    let close = (start + 1..chars.len()).find(|&index| chars[index] == ']')?;
    if chars.get(close + 1) != Some(&'(') {
        return None;
    }
    let end = (close + 2..chars.len()).find(|&index| chars[index] == ')')?;
    let label: String = chars[start + 1..close].iter().collect();
    let url: String = chars[close + 2..end].iter().collect();
    Some((label, url, end + 1))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn plain(text: &str) -> String {
        line(text, Style::plain())
            .into_iter()
            .map(|span| span.text)
            .collect()
    }

    #[test]
    fn plain_text_passes_through() {
        for text in [
            "Could not load /tmp/a.nbt: io error",
            "object_base_type is missing",
            "2 * 3 = 6",
            "a `unpaired backtick",
            "100% done, [not a link]",
            "*",
            "_",
        ] {
            assert_eq!(plain(text), text, "{text:?} must not change");
        }
    }

    #[test]
    fn inline_constructs_are_rendered() {
        let spans = line("**bold** and `code` and [docs](https://x)", Style::plain());
        let text: String = spans.iter().map(|span| span.text.as_str()).collect();
        assert_eq!(text, "bold and code and docs (https://x)");
        assert!(
            spans
                .iter()
                .any(|span| span.text == "bold" && span.style.bold)
        );
        assert!(
            spans
                .iter()
                .any(|span| span.text == "code" && span.style.fg == Color::Yellow)
        );
    }

    #[test]
    fn block_markers_are_rendered() {
        assert_eq!(plain("# Title"), "Title");
        assert_eq!(plain("- item"), "• item");
        assert_eq!(plain("> quoted"), "│ quoted");
    }

    #[test]
    fn a_line_that_does_not_fit_gets_an_ellipsis() {
        let spans = line("abcdefghij", Style::plain());
        let mut screen = Screen::new(8, 1);
        draw(&mut screen, 0, 0, &spans, 5, Style::plain());
        let row: String = (0..8).map(|x| screen.cell(x, 0).unwrap().0).collect();
        assert_eq!(row.trim_end(), "abcd…");
    }

    #[test]
    fn a_wide_character_is_never_cut_in_half() {
        let spans = line("加载加载", Style::plain());
        let mut screen = Screen::new(6, 1);
        draw(&mut screen, 0, 0, &spans, 6, Style::plain());
        let row: Vec<char> = (0..6).map(|x| screen.cell(x, 0).unwrap().0).collect();
        /* Two wide characters, then the ellipsis; no orphaned half glyph. */
        assert_eq!(row[0], '加');
        assert_eq!(row[1], '\0');
        assert_eq!(row[2], '载');
        assert_eq!(row[3], '\0');
        assert_eq!(row[4], '…');
    }
}
