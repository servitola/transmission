# Liquid Glass Tier 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply macOS 26 native Liquid Glass (`NSGlassEffectView`) to the Transmission macOS client's window chrome — toolbar (free via SDK), filter bar, status bar, and window background — while touching upstream files as little as possible so continuous rebases stay conflict-free.

**Architecture:** A new, fork-owned `LiquidGlass` helper class (`macosx/LiquidGlass.h/.mm`) wraps existing views in `NSGlassEffectView` at the exact moment `Controller.mm` creates them. No XIB is edited. `Controller.mm`'s footprint is one new `#import` and three one-line call insertions inside two existing methods (`awakeFromNib`, `updateMainWindow`) — both blocks upstream rarely touches. `CMakeLists.txt`'s footprint is one changed value (`MACOS_SUPPORT_MINIMUM`) and two new lines registering the new files.

**Tech Stack:** Objective-C++ (`.mm`), AppKit (`NSGlassEffectView`, `NSTitlebarAccessoryViewController`), CMake + Ninja build.

## Global Constraints

- Minimum deployment target: macOS 26.0 (from spec `docs/superpowers/specs/2026-08-14-liquid-glass-design.md`) — this fork does not support older macOS.
- No XIB files are edited — all changes are new Objective-C files plus minimal call-site insertions in `Controller.mm`.
- Scope is Tier 2 only: toolbar, filter bar, status bar, window background. Torrent list row cells (`TorrentCell.mm`, `TorrentTableView.mm`) are explicitly out of scope (Tier 3, future).
- No automated UI test suite exists for `macosx/` in this project (confirmed: `tests/` only covers `libtransmission`, `libtransmission-app`, `qt`, `utils`). Verification for every task in this plan is: the CMake build succeeds, and (for the final task) a manual visual check of the running app.
- `clang-format-20` must be on `PATH` for the pre-commit hook (`code_style.sh --check`) to pass — already installed via `brew install llvm@20` and symlinked to `/opt/homebrew/bin/clang-format-20`.

---

## Task 1: Bump minimum deployment target to macOS 26.0

**Files:**
- Modify: `CMakeLists.txt:10`

**Interfaces:** None — this is a build-configuration-only change with no code interface.

- [ ] **Step 1: Change the deployment target**

In `CMakeLists.txt`, line 10 currently reads:

```cmake
set(MACOS_SUPPORT_MINIMUM 11.0)
```

Change it to:

```cmake
set(MACOS_SUPPORT_MINIMUM 26.0)
```

- [ ] **Step 2: Re-configure and build to verify**

```bash
cd /Volumes/SanDisk/projects/transmission
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -t transmission-mac
```

Expected: configure succeeds, build succeeds (exit code 0), and the `ld: warning: building for macOS-11.0, but linking with dylib ... built for newer version 26.0` warnings seen in the previous baseline build are gone (the link step now targets 26.0 to match the Homebrew dylibs).

- [ ] **Step 3: Commit**

```bash
cd /Volumes/SanDisk/projects/transmission
git add CMakeLists.txt
git commit -m "build: raise minimum macOS deployment target to 26.0

Personal fork targets a single machine on macOS 26 (Tahoe) only, to use
native Liquid Glass AppKit APIs (NSGlassEffectView, macos(26.0)+)."
```

---

## Task 2: Add the LiquidGlass helper class

**Files:**
- Create: `macosx/LiquidGlass.h`
- Create: `macosx/LiquidGlass.mm`
- Modify: `macosx/CMakeLists.txt:124` (insert after `InfoWindowController.mm`, before `main.mm`)

**Interfaces:**
- Consumes: nothing (standalone new class, only `<AppKit/AppKit.h>`).
- Produces (for Task 3):
  - `+ (void)[LiquidGlass applyToWindowBackground:(NSWindow*)window scrollView:(NSScrollView*)scrollView]`
  - `+ (void)[LiquidGlass applyToAccessoryViewController:(NSTitlebarAccessoryViewController*)controller]`

This task adds the class to the build but does not call it from anywhere yet — it is inert until Task 3 wires it in, so verification here is compile-only.

- [ ] **Step 1: Create `macosx/LiquidGlass.h`**

```objc
// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// Wraps existing chrome views in macOS 26 Liquid Glass (NSGlassEffectView).
/// New code only — never edits a XIB — so upstream's XIB changes never
/// conflict with this file.
@interface LiquidGlass : NSObject

/// Wraps `window`'s content view in glass and turns off `scrollView`'s own
/// background painting, so the glass shows through around and behind it.
+ (void)applyToWindowBackground:(NSWindow*)window scrollView:(NSScrollView*)scrollView API_AVAILABLE(macos(26.0));

/// Replaces `controller`'s view with a glass-wrapped copy of itself. Call
/// before passing `controller` to -[NSWindow addTitlebarAccessoryViewController:].
+ (void)applyToAccessoryViewController:(NSTitlebarAccessoryViewController*)controller API_AVAILABLE(macos(26.0));

@end

NS_ASSUME_NONNULL_END
```

- [ ] **Step 2: Create `macosx/LiquidGlass.mm`**

```objc
// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import "LiquidGlass.h"

@implementation LiquidGlass

+ (NSGlassEffectView*)glassWrapView:(NSView*)contentView
{
    NSGlassEffectView* glass = [[NSGlassEffectView alloc] initWithFrame:contentView.frame];
    glass.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    glass.contentView = contentView;

    contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [contentView.leadingAnchor constraintEqualToAnchor:glass.leadingAnchor],
        [contentView.trailingAnchor constraintEqualToAnchor:glass.trailingAnchor],
        [contentView.topAnchor constraintEqualToAnchor:glass.topAnchor],
        [contentView.bottomAnchor constraintEqualToAnchor:glass.bottomAnchor],
    ]];

    return glass;
}

+ (void)applyToWindowBackground:(NSWindow*)window scrollView:(NSScrollView*)scrollView
{
    NSView* originalContent = window.contentView;
    if (originalContent == nil || [originalContent isKindOfClass:[NSGlassEffectView class]])
    {
        return;
    }

    window.contentView = [self glassWrapView:originalContent];
    scrollView.drawsBackground = NO;
}

+ (void)applyToAccessoryViewController:(NSTitlebarAccessoryViewController*)controller
{
    NSView* originalView = controller.view;
    if ([originalView isKindOfClass:[NSGlassEffectView class]])
    {
        return;
    }

    controller.view = [self glassWrapView:originalView];
}

@end
```

- [ ] **Step 3: Register the new files in `macosx/CMakeLists.txt`**

Find this block (around line 122-125):

```cmake
        InfoViewController.h
        InfoWindowController.h
        InfoWindowController.mm
        main.mm
```

Change it to:

```cmake
        InfoViewController.h
        InfoWindowController.h
        InfoWindowController.mm
        LiquidGlass.h
        LiquidGlass.mm
        main.mm
```

- [ ] **Step 4: Build to verify it compiles**

```bash
cd /Volumes/SanDisk/projects/transmission
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -t transmission-mac
```

Expected: build succeeds (exit code 0). `LiquidGlass.mm` appears in the Ninja build log as a compiled object. No behavior change yet — nothing calls the new class.

- [ ] **Step 5: Commit**

```bash
cd /Volumes/SanDisk/projects/transmission
git add macosx/LiquidGlass.h macosx/LiquidGlass.mm macosx/CMakeLists.txt
git commit -m "feat(macos): add LiquidGlass helper for wrapping views in NSGlassEffectView

Standalone new class, not yet wired into Controller.mm — kept as its own
commit so the helper and its call sites review independently."
```

---

## Task 3: Wire LiquidGlass into Controller.mm and verify visually

**Files:**
- Modify: `macosx/Controller.mm:52` (import)
- Modify: `macosx/Controller.mm:815` (window background)
- Modify: `macosx/Controller.mm:5111-5138` (`updateMainWindow`, both accessory bars)

**Interfaces:**
- Consumes: `LiquidGlass.applyToWindowBackground:scrollView:` and `LiquidGlass.applyToAccessoryViewController:` from Task 2.
- Produces: nothing further downstream — this is the final task in the tier.

- [ ] **Step 1: Add the import**

In `macosx/Controller.mm`, line 52 currently reads:

```objc
#import "FilterBarController.h"
```

Change it to:

```objc
#import "FilterBarController.h"
#import "LiquidGlass.h"
```

- [ ] **Step 2: Wrap the status bar accessory view before it's added to the window**

In `macosx/Controller.mm`, inside `-updateMainWindow` (around line 5111), this block currently reads:

```objc
    if (self.fStatusBar == nil)
    {
        self.fStatusBar = [[StatusBarController alloc] initWithLib:self.fLib];
        self.fStatusBar.layoutAttribute = NSLayoutAttributeBottom;
        self.fStatusBar.automaticallyAdjustsSize = NO;

        [self.fWindow addTitlebarAccessoryViewController:self.fStatusBar];
    }
```

Change it to:

```objc
    if (self.fStatusBar == nil)
    {
        self.fStatusBar = [[StatusBarController alloc] initWithLib:self.fLib];
        self.fStatusBar.layoutAttribute = NSLayoutAttributeBottom;
        self.fStatusBar.automaticallyAdjustsSize = NO;

        [LiquidGlass applyToAccessoryViewController:self.fStatusBar];
        [self.fWindow addTitlebarAccessoryViewController:self.fStatusBar];
    }
```

- [ ] **Step 3: Wrap the filter bar accessory view before it's added to the window**

Immediately below in the same method, this block currently reads:

```objc
    if (self.fFilterBar == nil)
    {
        self.fFilterBar = [[FilterBarController alloc] init];
        self.fFilterBar.layoutAttribute = NSLayoutAttributeBottom;
        self.fFilterBar.automaticallyAdjustsSize = NO;

        [self.fWindow addTitlebarAccessoryViewController:self.fFilterBar];
    }
```

Change it to:

```objc
    if (self.fFilterBar == nil)
    {
        self.fFilterBar = [[FilterBarController alloc] init];
        self.fFilterBar.layoutAttribute = NSLayoutAttributeBottom;
        self.fFilterBar.automaticallyAdjustsSize = NO;

        [LiquidGlass applyToAccessoryViewController:self.fFilterBar];
        [self.fWindow addTitlebarAccessoryViewController:self.fFilterBar];
    }
```

- [ ] **Step 4: Wrap the window background after `updateMainWindow` runs**

In `-awakeFromNib`, line 815 currently reads:

```objc
    [self updateMainWindow];
```

Change it to:

```objc
    [self updateMainWindow];

    [LiquidGlass applyToWindowBackground:self.fWindow scrollView:self.fTableView.enclosingScrollView];
```

- [ ] **Step 5: Build**

```bash
cd /Volumes/SanDisk/projects/transmission
cmake --build build -t transmission-mac
```

Expected: build succeeds (exit code 0).

- [ ] **Step 6: Launch and manually verify**

```bash
open /Volumes/SanDisk/projects/transmission/build/macosx/Transmission.app
```

Check, with your own eyes on the running app:
1. Toolbar renders as Liquid Glass (this comes free from the SDK/deployment target — confirms Task 1 took effect).
2. Filter bar (search/filter row — toggle it on if hidden, via the menu command that flips the `"FilterBar"` default) renders as glass, not a flat opaque bar.
3. Status bar (download/upload totals row — toggle via the `"StatusBar"` default) renders as glass.
4. The area around/behind the torrent list (window background, and the bottom strip with the action/speed-limit/clear-completed buttons, which lives inside `window.contentView`) shows glass, not a flat fill.
5. Torrent list rows themselves stay legible — text and progress bars readable (Tier 2 intentionally leaves row backgrounds opaque/system-drawn).
6. Resize the window and scroll the torrent list: confirm the glass views track the window size (no visible seam, no view stuck at its original launch size). If a glass view does NOT resize with its parent, that means implicit autoresizing-mask-to-constraint translation didn't propagate as expected — fix by adding explicit width/height constraints pinning `glass` itself to its superview in `LiquidGlass.mm`, in the same style already used for `contentView`'s pins.
7. Note (don't fix yet) any visible seam/double-blur where the window-background glass meets the filter-bar or status-bar glass — this is the known Tier 2 caveat from the design spec (glass views aren't merged via `NSGlassEffectContainerView` in this iteration); flag it for the Tier 3 follow-up if it looks bad.

- [ ] **Step 7: Commit**

```bash
cd /Volumes/SanDisk/projects/transmission
git add macosx/Controller.mm
git commit -m "feat(macos): apply Liquid Glass to toolbar, filter bar, status bar, and window background

Wires the LiquidGlass helper into the three points where Controller.mm
creates these views, so upstream's Controller.mm changes only conflict on
these three lines at worst."
```
