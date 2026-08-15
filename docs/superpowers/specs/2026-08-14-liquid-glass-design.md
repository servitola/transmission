# Liquid Glass macOS UI — Design

## Context

Personal fork of transmission/transmission (`servitola/transmission`, upstream tracked via
`upstream` remote), cloned to `/Volumes/SanDisk/projects/transmission`. Scope is the macOS
Cocoa client (`macosx/`) only — GTK, Qt, daemon, web UI are untouched.

The fork rebases onto upstream continuously, so upstream-file diffs must stay minimal and
predictable. It targets a single machine running macOS 26 (Tahoe), so backward compatibility
with older macOS is not a goal — minimum deployment target moves to macOS 26.0.

## Current window architecture (macosx/)

- **Toolbar**: `Toolbar : NSToolbar` — plain subclass, no custom drawing.
- **Filter bar**: `FilterBarController : NSTitlebarAccessoryViewController`, view from
  `FilterBar.xib`.
- **Status bar**: `StatusBarController : NSTitlebarAccessoryViewController`, view from
  `StatusBar.xib`.
- **Torrent list**: `NSTableView` (`TorrentTableView.mm`) with custom cell views
  (`TorrentCell.mm`). No custom `NSTableRowView`, no hardcoded row background color — rows
  draw via the system default, `TorrentCell` only branches on `NSBackgroundStyleEmphasized`
  for label color.
- No `NSVisualEffectView` / glass usage anywhere today. Window is fully opaque.
- There is no sidebar (unlike Finder) — this is a single-window, toolbar + accessory-bars
  + table layout.

## Goal

Apply macOS 26 native Liquid Glass (`NSGlassEffectView` et al., not `NSVisualEffectView`
blur-only compatibility styling) to make the window chrome and background transparent,
while keeping the fork trivially rebasable.

## Approach: injection layer, not XIB edits

All glass logic lives in new files the fork fully owns (e.g. `LiquidGlass.h/.mm`), wired in
through a single call added to `Controller.mm` (`applicationDidFinishLaunching:`). No XIB is
edited directly.

Rationale: XIBs are the files upstream touches most often when fixing toolbar/filter/status
bar bugs. Editing them directly guarantees XML merge conflicts on every rebase. A single
added line in `Controller.mm`, plus one deployment-target line in `CMakeLists.txt`, is the
entire upstream-file footprint — both are trivial "take mine" resolutions on conflict. New
files cannot conflict with upstream at all.

## Scope: Tier 2 (this iteration)

1. **Deployment target** → macOS 26.0 in `CMakeLists.txt` (one line changed).
2. **Toolbar**: no code needed — `NSToolbar` picks up system Liquid Glass automatically
   once linked against the macOS 26 SDK at that deployment target.
3. **Filter bar & status bar**: wrap each `NSTitlebarAccessoryViewController`'s view in an
   `NSGlassEffectView` container, inserted programmatically at launch (new file, no XIB
   edits).
4. **Window background**: set the main window's content view backdrop to glass as well, so
   the torrent list area shows glass behind/around the table rather than a flat opaque fill.
5. **Torrent list rows**: left as-is (opaque system row drawing) for this iteration —
   legibility of speed/progress text is unverified against a moving glass backdrop, and
   `TorrentCell.mm` is a file upstream changes often.

## Deferred: Tier 3 (explicitly wanted later, not now)

Make individual torrent rows translucent too, by touching `TorrentCell.mm` /
`TorrentTableView.mm` row drawing. Higher rebase-conflict surface (upstream edits these
often) and needs visual verification that progress bars / speed numbers stay readable over
glass. Follow-up iteration once Tier 2 has been used for a while.

## Verification

- `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -t
  transmission-mac`, then launch `build/macosx/Transmission.app` and visually confirm:
  toolbar, filter bar, status bar, and window background all render as glass; torrent list
  rows stay legible; window resizing/scrolling doesn't break the glass containers.
- No automated UI tests exist in this project for visual styling — verification is manual.
