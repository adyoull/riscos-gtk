# riscos-gtk 3.24.41-riscos1.0-rc13 (public beta) — Pi sign-off

Run on real hardware with the RC packages exactly as released (the zips
named in `MANIFEST.txt`, their SHA-256 checked). Note the machine, the
RISC OS version, the screen mode and the desktop theme.

Each line: **Pass**, **Fail** (send a note or photo, and the program's
Log from `<Wimp$ScrapDir>.<Program>.Log`), or **n/a**.

The "Seen before" column says whether an earlier build already passed this
on the Pi:
- **yes:** the code hasn't changed since; one quick look is enough.
- **changed:** an earlier build passed, but the code has changed since;
  test it properly.
- **new:** never tried on the Pi; test it properly.

The release goes ahead when every line passes or has an agreed note in
the release notes.

Machine: ____________  RISC OS: ______  Mode: ________  Theme: ____________

## 1. Installing and starting

| # | Test | Seen before | Result |
|---|---|---|---|
| 1.1 | Each zip unpacks with SparkFS/!Infozip and the !App has its icon in the Filer | yes | |
| 1.2 | Each program starts from a fresh boot with SharedUnixLibrary, ARMEABISupport and Iconv loaded only by its !Run | yes | |
| 1.3 | Each program's Log ends with no warnings at start-up (Claws Mail: only the known start-up GTK warnings) | yes (L3afpad, ThemeTest) / new (Claws) | |
| 1.4 | Start each under the RISC OS 5 default theme and under the Raspberry theme | yes | |
| 1.5 | In an EX0 EY0 (180 dpi) mode, each program's windows, text and widgets are their usual size and sharp; option buttons, radio buttons and scroll bars are full size; the Log says "GDK scale 2" | yes (ClawsMail 0.9, L3afpad 0.20) | |
| 1.6 | In an EX0 EY0 mode, ClawsMail's toolbar, message list marks, folder icons and About logo are sharp, not soft | new | |
| 1.7 | In an EX0 EY0 mode, !ClawsMail and !L3afpad show their large 180 dpi icons in the Filer and on the icon bar (after the Filer sees them again, or a desktop restart); in an EX1 EY1 mode everything looks as in RC3 | new | |

## 2. GTKThemeTest 3.24.41-riscos1.0-rc13 (the backend and theme)

| # | Test | Seen before | Result |
|---|---|---|---|
| 2.1 | All four pages: widgets drawn in the desktop's colours and sprites (option, radio, scroll bars) | yes | |
| 2.2 | Menu over the window gives the program's menu as a Wimp menu; short-cuts shown; ticks and shading right | yes | |
| 2.2a | **Menu over the context area (page 4): the window's menu with "Selection >" first; Selection > Info works** | **changed (0.43, RC2)** | |
| 2.3 | Pop-up (combo): a Wimp menu to the right of the field | yes | |
| 2.4 | Text fields: typing, the caret, Menu gives Cut/Copy/Paste; paste text copied in Edit and copy text into Edit | partly (drag yes; clipboard with other programs not reported) | |
| 2.5 | Drag text from a field to a Filer window (a Text file) and a Text file onto a field | yes | |
| 2.6 | File > Save: Toolbox-style Save box; drag the icon to a Filer window; type a full path and Save | yes | |
| 2.7 | File > Open: Open box; drag a file to it | yes | |
| 2.8 | Pointer shapes over text and resize handles; pointer colours | yes | |
| 2.9 | Scroll wheel in the lists and text view | yes | |
| 2.10 | **Size grip (one icon only, from the first frame): no scroll bar runs into the corner, so an empty scroll bar outside the window's right edge with the size icon at its foot; nothing of the window is covered** | **new (0.39–0.42)** | |
| 2.11 | Drag the size icon: the window resizes; clicking the empty bar elsewhere does nothing | **new** | |
| 2.12 | Toggle size: the window fills the screen and is laid out again | yes | |
| 2.13 | Icon bar: Select shows the window; Menu > Info, Quit | yes | |

## 3. L3afpad 0.8.18.1.11-riscos32

| # | Test | Seen before | Result |
|---|---|---|---|
| 3.1 | Select on the icon opens a window; again opens a second, offset down and right | yes | |
| 3.2 | Double-click a Text file and drop one on the icon: each opens in its own window; the same file again brings its window forward | yes | |
| 3.3 | Undo in one window doesn't touch another | yes | |
| 3.4 | Save As / Open boxes; a name without an extension saves as Text | yes | |
| 3.5 | File > Print: the Print box, print a page with text: sharp text, layout right, no residue on screen | yes | |
| 3.6 | Print Preview, then Print from it | yes | |
| 3.7 | **Size icon at the bottom right, under the text's scroll bar, no extra bar outside** | changed (0.15–0.18 rework) | |
| 3.8 | Choices: font, tab width, wrap, line numbers; Set applies to all windows; kept after Quit | yes | |
| 3.9 | Close windows with unsaved text: asks; the last window leaves L3afpad on the icon bar; Quit with unsaved text asks once | yes | |
| 3.10 | Running !L3afpad again with a file: the running copy opens it | yes | |

## 4. ClawsMail 4.4.0-riscos22

Use a real account. Ideally test one POP3 account and one IMAP account
(Gmail, Fastmail, an ISP), with both SSL/TLS and STARTTLS where the
provider offers them.

| # | Test | Seen before | Result |
|---|---|---|---|
| 4.1 | First start: the setup wizard, then the main window; settings land in `<Choices$Write>.ClawsMail` | new | |
| 4.1a | Later starts: only the icon on the icon bar, no window; Select on the icon opens the main window | yes | |
| 4.1b | Close the main window: Claws Mail stays on the icon bar (and keeps checking mail); Select opens it again; Quit from the icon bar menu quits | yes | |
| 4.1c | One size icon only, from the moment the main window opens (not one in the window's corner as well) | yes | |
| 4.2 | POP3 over SSL/TLS (995): Get Mail fetches into the Inbox; AcornSSL's certificate question if any | new | |
| 4.3 | SMTP over SSL/TLS (465) and STARTTLS (587): Write, send to yourself; filed in Sent | new | |
| 4.4 | IMAP over SSL/TLS (993): the server's folders appear; INBOX lists messages; a message opens | new | |
| 4.5 | IMAP with STARTTLS (143) | new | |
| 4.6 | While IMAP is busy (a big folder), the desktop and other programs keep running | new | |
| 4.7 | A wrong password and a wrong server name each give a clear error, not a hang | new | |
| 4.8 | Drop a PDF on the icon: a new message with it attached (application/pdf, name with .pdf) | new | |
| 4.9 | Quit with a message half-written: it's kept in Drafts; Quit with mail queued asks | new | |
| 4.10 | Main window: the message list's bar ends above the message view, so the empty scroll bar is outside, with the size icon at its foot | new | |
| 4.11 | Compose window: the size icon under the text's scroll bar | new | |
| 4.12 | Choices... on the icon bar menu opens Preferences | new | |
| 4.14 | Menu over the message list, the folder list and elsewhere in the main window: always the main menu (File, Edit, View...); over the message list its first entry is "Message >" (Reply...), over the folder list "Folder >"; choosing from them works | yes (0.8) | |
| 4.13 | Leave it running an hour checking mail: no growth in memory (Task Manager), no crash | new | |
| 4.15 | HTML mail (a newsletter): laid out on a white page, not plain text; From/To/Subject/Date above it; pictures sent with it shown | yes (0.8) | |
| 4.16 | Remote content on (Choices... > Plugins > LiteHTML): the newsletter's web pictures load over https; the desktop keeps running meanwhile | new | |
| 4.17 | Menu over HTML mail: the window's menu; on a link, "Link >" first (Open Link, Copy Link; Copy Link pastes in Edit) | yes (0.8) | |
| 4.18 | Click a link: Iris running opens it; with no browser running Iris starts and opens it; with only NetSurf installed, NetSurf does; a mailto: link starts a new message | partly (Iris yes, 0.8) | |
| 4.19 | RISC OS icons: the !ClawsMail icon; the toolbar; the folders as the Filer's directory icons with signs on Inbox, Sent, Queue, Drafts; Choices... > Display > Themes switches to Claws Mail's own icons and back | yes (0.8) | |
| 4.20 | Help > About (or icon bar Menu > Info): AcornSSL (not GnuTLS) and the RISC OS logo | yes (0.8) | |

## 5. New in RC5

| # | Test | Seen before | Result |
|---|---|---|---|
| 5.1 | L3afpad: File > Open, then the Open box's Browse... button (and, another time, Menu over the box > Directory browser...): a window of the folder's files with their sprites, a sidebar of discs and places | new | |
| 5.2 | In the browser: double-click a directory, Parent, a disc in the sidebar, Menu > Display > Small icons / Full info; double-click a file opens it | new | |
| 5.3 | `*Set RISCOS_GTK_CHOOSER filer`, then L3afpad File > Save As: the browser; type a name, Save: saved in the folder shown, typed Text; drag its file icon to a Filer window also saves | new | |
| 5.4 | Drag a directory from the Filer onto the browser: it's shown | new | |
| 5.5 | With PackMan's FontConfig installed: the Log says "fonts: the system's (UnixFC)"; text looks as before; a second start is quick | new | |
| 5.6 | GTKGLTest: the triangle spins; the frame rate shows; `*Set GDK_GL gles` also works | new | |
| 5.7 | Geany 2.1-riscos11: starts with its window and an icon bar icon; a C file dropped on it opens highlighted, with its functions in the Symbols list | new | |
| 5.8 | Geany: File > Open (box and browser), File > Save As (box), Edit > Preferences, Quit with an unsaved document asks | new | |
| 5.9 | Geany: settings kept in `<Choices$Write>.geany` after a restart | new | |

## 6. New in RC6

| # | Test | Seen before | Result |
|---|---|---|---|
| 6.1 | mtPaint: Menu over the picture, the toolbars and the palette gives its menu; Adjust paints with colour B | new | Pass (2026-10-11) |
| 6.2 | mtPaint: View > Pan Window and the Pattern Chooser (click the pattern in the toolbar) have a title bar; the close icon closes them and they open again | new | Pass (2026-10-11) |
| 6.3 | Geany 2.1-riscos11 with GCC installed: a hello.c with an error, Build > Compile: the error in the Compiler tab, clicking it goes to the line; fix it, Build > Build, then Build > Execute: a task window with its output and "(program exited with code: 0)"; the desktop keeps running throughout | new | |
| 6.4 | Geany: Build > Make on a long build, then Build > Stop: it ends | new | |
| 6.5 | Geany with the DDE (!SetPaths run): a folder with c.hello containing an error, open hello.c, Set Build Commands Compile to `cc -throwback -c c.%e -o o.%e`, Compile: the error in the Compiler tab, clicking goes to the line; StrongED gets throwback again afterwards | new | |
| 6.6 | The directory browser's Discs: the same discs and mounts as the icon bar (no extra filing systems); with a LanMan98 mount, clicking it shows the share; a USB stick plugged in appears the next time the browser opens | new | Discs as the icon bar: pass; LanMan98 mount: fails (2026-10-11) |
| 6.7 | mtPaint: its main window opens with its title bar on the screen; maximising it leaves the title bar showing | new | Pass (2026-10-11) |

## 7. New in RC7

| # | Test | Seen before | Result |
|---|---|---|---|
| 7.1 | Gnumeric 1.12.57-riscos16: starts with a new workbook and an icon bar icon; type numbers and `=SUM(A1:A3)`, `=AVERAGE(...)`: right results | yes (test builds) | |
| 7.2 | Gnumeric: File > Save As `Test.xlsx`, then `Test.ods` (Save box, also dragging its icon to a Filer window); File > Open each again (Open box and Browse...): the same cells | changed | |
| 7.3 | Gnumeric: drop a .xlsx and a CSV on its window and on its icon bar icon; double-click a CSV in the Filer while it's loaded: each opens | changed | |
| 7.4 | Gnumeric: Insert > Chart, a bar chart of the cells: drawn; scroll the sheet past it and past a check box (Insert > Check box): it keeps working | changed | |
| 7.5 | Gnumeric: File > Properties: Owner and Group show numbers, no "null pointer"; Choices... on the icon bar menu opens its Preferences; settings kept after a restart in `<Choices$Write>.Gnumeric` | new | |
| 7.6 | Every program's toolbars and dialogue boxes have the RISC OS-style icons (open, save, print, cut, copy, paste, find, the browser's places, a field's clear icon); Gnumeric's and Geany's toolbars too; sharp in an EX0 EY0 mode | new | |
| 7.7 | A file whose name has a hard space or an accented letter: shown right in the Open box, the browser and the window title; opens and saves | changed | |
| 7.8 | The directory browser's Discs are the icon bar's (Gnumeric and L3afpad), not every filing system | changed | |

## 8. New in RC8

| # | Test | Seen before | Result |
|---|---|---|---|
| 8.1 | Gnumeric: Insert > Chart lists the chart types (Line, Area, Bar, Column, Pie, XY, Radar, Surface...) with their pictures; a chart is made; no `cb_pending_plot_types_load` lines in the Log | new | |
| 8.2 | With PackMan's FontConfig installed and booted: each program's Log has `riscos-gtk: fonts: the system's (UnixFC, ...)`, with the fonts in UnixFont and "Serif DejaVu Serif, Monospace DejaVu Sans Mono" (if those are installed) | changed | |
| 8.3 | Gnumeric: Format Cells > Font, choose Serif, then Monospace: the preview and the cells change face, not only size | changed | |
| 8.4 | Without FontConfig booted: the Log's fonts line says "UnixFC$Dir isn't set"; every program still starts and draws text (DejaVu Sans) | new | |
| 8.5 | L3afpad and Geany: a monospaced font (Monospace / DejaVu Sans Mono) in the editor, with and without FontConfig | changed | |

## 9. New in RC9

| # | Test | Seen before | Result |
|---|---|---|---|
| 9.1 | With a colour emoji font installed in UnixFont (Noto Color Emoji from PackMan): an emoji typed or pasted into L3afpad, or in a ClawsMail message, is about the size of the text around it; the Log's fonts line names the emoji font | new | |
| 9.2 | Without one: emoji show as DejaVu Sans's plain glyphs, the text's size | new | |

## 10. New in RC10

| # | Test | Seen before | Result |
|---|---|---|---|
| 10.1 | FontConfig installed, then TTF-Noto-Color-Emoji (PackMan, or the release zip copied into !UnixFont.truetype): a message with emoji (smileys, flags, a family, a skin tone) shows them in colour at the text's size in ClawsMail, not as boxes with their code in or as plain black glyphs; the Log's fonts line says emoji Noto Color Emoji | 9.2 showed boxes (photo) | |
| 10.1a | Without the package: the Log's fonts line ends "for colour emoji install PackMan's TTF-Noto-Color-Emoji (it needs FontConfig)" | new | |
| 10.1b | PackMan installs TTF-Noto-Color-Emoji with UnixFont and FontConfig, runs fc-cache, and removes it again | new | |
| 10.2 | ClawsMail's message window: Message > Insert emoji... (and Ctrl-.) opens the emoji chooser at the caret; a group button and the search field work; the emoji clicked goes into the message text; with the caret in the Subject line, into the subject | new | |
| 10.3 | The message sent with an emoji in its text and subject arrives right in another mail program | new | |
| 10.4 | The chooser on a long message scrolled down, and in a small message window: it opens where the caret is and fits | new | |
| 10.5 | In another program's text field (L3afpad's text; not Geany's editor, which is Scintilla): Ctrl-. or the field's menu > Insert Emoji opens the same chooser (pop-overs were never shown before) | new | |

## 11. New in RC11

| # | Test | Seen before | Result |
|---|---|---|---|
| 11.1 | Typing quickly in a large L3afpad window, and in Geany and ClawsMail's message window: keeps up; the text, the caret and the lines around it look right (no strips left over) | L3afpad test build: snappier | |
| 11.2 | Scrolling a long text, a Gnumeric sheet and ClawsMail's message list; moving the pointer over buttons and menus: everything is redrawn right | new | |
| 11.3 | A window covered by another, or iconised, while it changes (a ClawsMail fetch, a Gnumeric recalculation), then brought back: it shows the new state | new | |
| 11.4 | The resize grip in the bottom right corner stays drawn while typing and scrolling near it | new | |
| 11.5 | In an EX0 EY0 mode: 11.1 and 11.2 look right too | new | |
| 11.6 | With the Raspberry theme (or another with translation tables): programs start a little faster than RC10; the scroll bars and title bars look as before | new | |

## 12. New in RC12

| # | Test | Seen before | Result |
|---|---|---|---|
| 12.1 | The first start of any program (L3afpad, say): the Log has `riscos-gtk: fonts: N RISC OS faces (N made now in S s ...)`; note N and S. `<Choices$Write>.GTK.FontCache` has an .otf file for each face and an Index | RISC OS on Linux: 22 faces in 1.3 s | |
| 12.2 | The next start, and a second program: "0 made now" | RISC OS on Linux | |
| 12.3 | L3afpad's Choices > Font, Gnumeric's Format > Cells > Font: Homerton, Trinity, Corpus and the fonts in !Fonts are listed; chosen, they look as the desktop draws them; Bold, Italic and Bold Italic choose the right faces | RISC OS on Linux (AbiWord) | |
| 12.4 | Add a font to !Fonts (or change one), start a program: it's made and listed; a font that has no outlines is left out and the Log says so | new | |
| 12.5 | Without FontConfig booted: Gnumeric's Serif is Trinity and Monospace is Corpus; with FontConfig booted, its own Serif and Monospace fonts come first, as before | new | |
| 12.6 | A Gnumeric sheet saved by Excel or LibreOffice using Times New Roman, Arial and Courier New: serif, sans and monospaced faces, not all DejaVu Sans | new | |
| 12.7 | Left Alt+F, Alt+E... in Gnumeric, Geany, ClawsMail and mtPaint: that menu opens at the window's top left; F10 opens the whole menu; Alt and a letter in a text field of L3afpad still types its character; Right Alt (AltGr) types as before | new | |
| 12.8 | `*Set RISCOS_GTK_RISCOS_FONTS 0`, then start a program: the Log says the desktop's fonts are off and they aren't listed | new | |

## 13. New in RC13: AbiWord 3.0.8-riscos1

| # | Test | Seen before | Result |
|---|---|---|---|
| 13.1 | AbiWord starts with a new document and an icon bar icon; typing, Bold/Italic, a table (Table > Insert Table), a picture (Insert > Picture): all drawn right | test1-test6 | |
| 13.2 | File > Save As `Test.odt`, `Test.docx`, `Test.rtf`, `Test.doc` (Save box, also dragging its icon to a Filer window); open each again (Open box, and dropped on the icon bar icon): the same text | test builds | |
| 13.3 | A Word file double-clicked in the Filer while AbiWord is loaded opens in a window of its own (or in the empty new document) | test builds | |
| 13.4 | Spelling: a misspelt word gets a red underline; Menu over it offers suggestions; Tools > Spelling works; a language's .dic/.aff in `<Choices$Write>.GTK.Dictionaries` is used for text set to that language | test2-test6 | |
| 13.5 | Format > Font: the desktop's fonts are listed; the preview follows each change of font, size, style, underline and colour; Format > Paragraph's and Bullets and Numbering's previews follow too | test6 | |
| 13.6 | Left Alt+F, Alt+E... open AbiWord's menus; the toolbars have the RISC OS-style icons; the icon in the Filer, on the icon bar and in Help > About | test3-test6 | |
| 13.7 | The Log (`<Wimp$ScrapDir>.AbiWord.Log`) has the fonts lines and no errors | test builds | |
| 13.8 | Every program: a dialogue box's preview or picture that the program draws itself (AbiWord's above) changes straight away | new | |

## 14. Packaging and licences

| # | Test | Seen before | Result |
|---|---|---|---|
| 14.1 | Each !Help has the right version, requirements and licences (LicenceEmoji in fonts) | — | |
| 14.2 | `MANIFEST.txt` SHA-256s match the zips | — | |
| 14.3 | The source tar unpacks and `make deps release` (BUILDING.md) rebuilds zips that work the same | — | |

Signed off by: ____________  Date: ____________
