/*! The terminal half of the UI: raw mode, the alternate screen, mouse capture
 * and the event loop. Nothing here knows what the screen means; it paints the
 * [`Screen`](super::Screen) the page produced and turns terminal input into the
 * page's own [`Event`] values.
 */

use super::{App, Color, Event, Key, Mouse, MouseKind, Screen, Style};
use crossterm::cursor::{Hide, MoveTo, Show};
use crossterm::event::{
    self, DisableMouseCapture, EnableMouseCapture, Event as TerminalEvent, KeyCode, KeyModifiers,
    MouseEventKind,
};
use crossterm::style::{
    Attribute, Color as TerminalColor, Print, ResetColor, SetAttribute, SetBackgroundColor,
    SetForegroundColor,
};
use crossterm::terminal::{
    EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode, size,
};
use crossterm::{execute, queue};
use std::io::{self, Write};

/// Opens the UI and runs it until the user quits.
pub fn run(app: &mut App) -> io::Result<()> {
    let mut stdout = io::stdout();
    enable_raw_mode()?;
    execute!(
        stdout,
        EnterAlternateScreen,
        EnableMouseCapture,
        Hide,
        ResetColor,
        /* Painting writes every cell including the last column; with the
         * terminal's own wrapping on, that cell sets the pending wrap and the
         * next line feed scrolls the whole picture up by one row. */
        Print("\x1b[?7l")
    )?;

    let result = event_loop(app, &mut stdout);

    let _ = execute!(
        stdout,
        Print("\x1b[?7h"),
        DisableMouseCapture,
        LeaveAlternateScreen,
        Show,
        ResetColor
    );
    let _ = disable_raw_mode();
    result
}

fn event_loop(app: &mut App, stdout: &mut io::Stdout) -> io::Result<()> {
    let (mut width, mut height) = size().unwrap_or((80, 24));
    let mut screen = Screen::new(width.max(2), height.max(4));

    loop {
        if (width, height) != (screen.width, screen.height) {
            screen.resize(width.max(2), height.max(4));
        }
        app.render(&mut screen);
        paint(stdout, &screen)?;
        stdout.flush()?;

        if app.quit() {
            return Ok(());
        }

        /* The command output is transient: wake up when it is due to go
         * instead of waiting for the next key. */
        if let Some(wait) = app.console_timeout() {
            if !event::poll(wait)? {
                app.expire_console();
                continue;
            }
        }

        match event::read()? {
            TerminalEvent::Key(key) => {
                if key.code == KeyCode::Char('c') && key.modifiers.contains(KeyModifiers::CONTROL) {
                    return Ok(());
                }
                if let Some(key) = map_key(key.code) {
                    app.handle(&Event::Key(key));
                }
            }
            TerminalEvent::Mouse(mouse) => {
                if let Some(mouse) = map_mouse(mouse) {
                    app.handle(&Event::Mouse(mouse));
                }
            }
            TerminalEvent::Resize(new_width, new_height) => {
                width = new_width;
                height = new_height;
                app.handle(&Event::Resize(width, height));
            }
            _ => {}
        }
    }
}

fn paint(out: &mut io::Stdout, screen: &Screen) -> io::Result<()> {
    let mut current = Style::default();
    for y in 0..screen.height {
        queue!(out, MoveTo(0, y))?;
        for x in 0..screen.width {
            let Some((ch, style)) = screen.cell(x, y) else {
                continue;
            };
            if ch == '\0' {
                /* The second cell of a wide character; the character itself
                 * already moved the cursor over it. */
                continue;
            }
            if style != current {
                if style.fg != current.fg {
                    queue!(out, SetForegroundColor(terminal_color(style.fg)))?;
                }
                if style.bg != current.bg {
                    queue!(out, SetBackgroundColor(terminal_color(style.bg)))?;
                }
                if style.bold != current.bold {
                    queue!(
                        out,
                        SetAttribute(if style.bold {
                            Attribute::Bold
                        } else {
                            Attribute::NormalIntensity
                        })
                    )?;
                }
                current = style;
            }
            queue!(out, crossterm::style::Print(ch))?;
        }
    }
    Ok(())
}

fn terminal_color(colour: Color) -> TerminalColor {
    match colour {
        Color::Default => TerminalColor::Reset,
        Color::Black => TerminalColor::Black,
        Color::Red => TerminalColor::DarkRed,
        Color::Green => TerminalColor::DarkGreen,
        Color::Yellow => TerminalColor::DarkYellow,
        Color::Cyan => TerminalColor::DarkCyan,
        Color::White => TerminalColor::White,
        Color::DarkGrey => TerminalColor::DarkGrey,
        Color::LightCyan => TerminalColor::Cyan,
    }
}

fn map_key(code: KeyCode) -> Option<Key> {
    Some(match code {
        KeyCode::Up => Key::Up,
        KeyCode::Down => Key::Down,
        KeyCode::Left => Key::Left,
        KeyCode::Right => Key::Right,
        KeyCode::Enter => Key::Enter,
        KeyCode::Esc => Key::Esc,
        KeyCode::Tab => Key::Tab,
        KeyCode::Backspace => Key::Backspace,
        KeyCode::Home => Key::Home,
        KeyCode::End => Key::End,
        KeyCode::PageUp => Key::PageUp,
        KeyCode::PageDown => Key::PageDown,
        KeyCode::Char(ch) => Key::Char(ch),
        KeyCode::F(number) => Key::F(number),
        _ => return None,
    })
}

fn map_mouse(mouse: crossterm::event::MouseEvent) -> Option<Mouse> {
    let kind = match mouse.kind {
        MouseEventKind::Down(_) => MouseKind::Down,
        MouseEventKind::Up(_) => MouseKind::Up,
        MouseEventKind::Drag(_) => MouseKind::Drag,
        MouseEventKind::ScrollUp => MouseKind::WheelUp,
        MouseEventKind::ScrollDown => MouseKind::WheelDown,
        _ => return None,
    };
    Some(Mouse {
        kind,
        x: mouse.column,
        y: mouse.row,
    })
}
