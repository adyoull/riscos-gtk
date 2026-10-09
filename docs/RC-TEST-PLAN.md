# riscos-gtk 3.24.41-riscos1.0-rc1 — Pi sign-off

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

## 2. GTKThemeTest 0.42 (the backend and theme)

| # | Test | Seen before | Result |
|---|---|---|---|
| 2.1 | All four pages: widgets drawn in the desktop's colours and sprites (option, radio, scroll bars) | yes | |
| 2.2 | Menu over the window gives the program's menu as a Wimp menu; short-cuts shown; ticks and shading right | yes | |
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

## 3. L3afpad 0.18

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

## 4. ClawsMail 0.5 (0.4 started once on the Pi; the rest new)

Use a real account. Ideally test one POP3 account and one IMAP account
(Gmail, Fastmail, an ISP), with both SSL/TLS and STARTTLS where the
provider offers them.

| # | Test | Seen before | Result |
|---|---|---|---|
| 4.1 | First start: the setup wizard, then the main window; settings land in `<Choices$Write>.ClawsMail` | new | |
| 4.1a | Later starts: only the icon on the icon bar, no window; Select on the icon opens the main window | **new (0.5)** | |
| 4.1b | Close the main window: Claws Mail stays on the icon bar (and keeps checking mail); Select opens it again; Quit from the icon bar menu quits | **new (0.5)** | |
| 4.1c | One size icon only, from the moment the main window opens (not one in the window's corner as well) | **new (0.5)** | |
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
| 4.13 | Leave it running an hour checking mail: no growth in memory (Task Manager), no crash | new | |

## 5. Packaging and licences

| # | Test | Seen before | Result |
|---|---|---|---|
| 5.1 | Each !Help has the right version, requirements and licences | — | |
| 5.2 | `MANIFEST.txt` SHA-256s match the zips | — | |
| 5.3 | The source tar unpacks and `make deps release` (BUILDING.md) rebuilds zips that work the same | — | |

Signed off by: ____________  Date: ____________
