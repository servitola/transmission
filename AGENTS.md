# AGENTS.md

Personal fork of transmission/transmission (`servitola/transmission`, public,
`upstream` remote wired for rebasing). Scope: macOS Cocoa client
(`macosx/`) only — GTK/Qt/daemon/web UI untouched. Minimum deployment
target raised to macOS 26.0 (Tahoe) to use native Liquid Glass
(`NSGlassEffectView`). Personal use, no backward-compat concern.

Design spec: `docs/superpowers/specs/2026-08-14-liquid-glass-design.md`
Implementation plan: `docs/superpowers/plans/2026-08-14-liquid-glass-tier2.md`

## Architecture

All Liquid Glass logic lives in fork-owned new files, never in XIBs, to
keep rebase conflicts confined to a handful of one-line hooks in
`Controller.mm` / `TorrentTableView.mm`:

- `macosx/LiquidGlass.h/.mm` — glass-wrapping helpers (window background,
  titlebar accessories, row backgrounds, card font size, legibility
  shadow). All static/class methods, no state.
- `macosx/LiquidGlassRowView.h/.mm` — custom `NSTableRowView` for
  translucent (not solid) selection highlight.
- Hook points: `Controller.mm` `awakeFromNib` (window bg) and
  `updateMainWindow` (status/filter bar), `TorrentTableView.mm`
  `outlineView:didAddRowView:forRow:` / `outlineView:viewForTableColumn:`
  / `outlineView:rowViewForItem:`.

## Known gotchas hit during implementation

- `NSOutlineView` delegate methods use `outlineView:...` selectors, not
  `tableView:...` (e.g. `outlineView:didAddRowView:forRow:`) — the
  `NSTableView` selector silently never fires on an outline view.
- A window defaults `isOpaque = YES`; glass looks flat/opaque until you
  also set `window.opaque = NO` + `window.backgroundColor = clearColor`.
- Titlebar/toolbar strip is a separate render layer from `contentView` —
  needs `window.titlebarAppearsTransparent = YES` on top of the
  contentView glass wrap.
- `TorrentTableView` hardcodes the *getter* of
  `usesAlternatingRowBackgroundColors` (returns `!SmallView`), so setting
  the property from outside is silently ignored. Fixed via
  `NSTableView.backgroundColor = clearColor` + per-row
  `didAddRowView:forRow:` override instead.
- `NSOutlineView` defaults to `NSTableViewSelectionHighlightStyleSourceList`,
  which — per Apple's own doc comment — never calls
  `-drawSelectionInRect:`. Must switch to `.Regular` style first.
- `clang-format` version must match CI (`clang-format-20`, not whatever
  brew's plain `clang-format` resolves to) or the pre-commit hook rejects
  unrelated pre-existing files. Symlinked
  `/opt/homebrew/opt/llvm@20/bin/clang-format` →
  `/opt/homebrew/bin/clang-format-20` (matches `code_style.sh`'s own
  version-detection preference order).
- Font size bumps and legibility shadows must guard against re-application
  (associated-object flag) since view-based table cells are reused and
  reconfigured on every UI refresh tick (~1s timer) — a bare delta-add
  compounds without a guard.

## Handoff

Tier 2 (toolbar/filter bar/status bar/window background/torrent rows all
glass) is done and confirmed working by the user via live screenshots —
focused and unfocused states both look right, titlebar is transparent,
chrome bars (Regular style) read as visually distinct from the
content/rows (Clear style), card text is legible over arbitrary
backgrounds via a blurred shadow, selection is a translucent accent tint
instead of a solid block.

Latest commit `65f49b88f` (glass selection) was just pushed to the local
build and the user was asked to confirm — **no confirmation received yet
this session**. Next session should start by asking whether the glass
selection highlight looks right, or whether they already checked it
between sessions.

Not started: Tier 3 (per the design spec) — making individual torrent
row content more transparent beyond what's already done — was effectively
folded into this session's work already (rows are fully glass now via
`TorrentCell`/`TorrentTableView` changes), so Tier 3 as originally scoped
is essentially complete too. No known remaining scope unless the user asks
for more.

Build: `cd /Volumes/SanDisk/projects/transmission && cmake --build build -t transmission-mac`
(reconfigure with `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
first if new files were added to `CMakeLists.txt`). Run:
`open build/macosx/Transmission.app`. Nothing pushed to GitHub — 12 commits
ahead of `origin/main` locally, all on `main` directly (personal fork, no
feature branch, by the user's own choice).
