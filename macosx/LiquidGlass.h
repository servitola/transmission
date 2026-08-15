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

/// Gives `rowView` a translucent (not fully clear) neutral tint so the glass
/// blur still shows behind it but card text keeps a consistent, legible
/// backdrop instead of raw desktop content bleeding straight through. Call
/// from a table/outline view's -tableView:didAddRowView:forRow: delegate
/// method (-outlineView:... for an NSOutlineView), since that is the
/// documented customization point for per-row background color and runs for
/// every row as it's created or recycled — a one-time pass over currently
/// visible rows would miss rows added later by scrolling.
+ (void)applyToRowView:(NSTableRowView*)rowView API_AVAILABLE(macos(26.0));

/// Increases `textField`'s font size by `delta` points, once. Safe to call
/// repeatedly on the same instance (e.g. from a view-based table's cell
/// setup, which reruns on a reused cell view every refresh) — a bare
/// `pointSize + delta` on every call would compound without this guard.
+ (void)increaseFontSizeBy:(CGFloat)delta forTextField:(nullable NSTextField*)textField;

/// Adds a soft dark halo behind `textField`'s text so it stays legible
/// against whatever ends up behind the glass — a directional drop shadow
/// only helps when the backdrop is reliably darker on one side, but glass
/// shows arbitrary desktop content, so this uses a centered, blurred
/// shadow instead (the same trick as subtitles over video). Uses
/// NSView.shadow (persists across -setStringValue:, unlike a shadow
/// attribute on an NSAttributedString, which -setStringValue: would wipe
/// on the next cell refresh).
+ (void)improveLegibilityForTextField:(nullable NSTextField*)textField;

@end

NS_ASSUME_NONNULL_END
