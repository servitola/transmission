// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import "LiquidGlass.h"
#import <objc/runtime.h>

@implementation LiquidGlass

static void* kFontSizeAlreadyIncreasedKey = &kFontSizeAlreadyIncreasedKey;
static void* kLegibilityShadowAppliedKey = &kLegibilityShadowAppliedKey;

+ (NSGlassEffectView*)glassWrapView:(NSView*)contentView style:(NSGlassEffectViewStyle)style
{
    NSGlassEffectView* glass = [[NSGlassEffectView alloc] initWithFrame:contentView.frame];
    glass.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    glass.style = style;
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

    // A window defaults to opaque, which can flatten glass compositing.
    // The classic AppKit recipe for a translucent window: not opaque, with
    // a clear window background color.
    window.opaque = NO;
    window.backgroundColor = NSColor.clearColor;

    // The titlebar/toolbar strip is a separate render layer from
    // contentView — without this it stays opaque even once contentView
    // is glass.
    window.titlebarAppearsTransparent = YES;

    // Regular reads as frosted/substantial and stays legible no matter what's
    // behind the window; Clear let raw desktop content (and its varying hue
    // per window region) bleed straight through torrent rows, which looked
    // blotchy and hurt text legibility.
    window.contentView = [self glassWrapView:originalContent style:NSGlassEffectViewStyleRegular];
    scrollView.drawsBackground = NO;

    // NSTableView paints its own opaque backgroundColor under every row,
    // independently of the enclosing scroll view — see the doc comment on
    // -[NSTableView backgroundColor] for this exact two-part recipe.
    NSView* documentView = scrollView.documentView;
    if ([documentView isKindOfClass:[NSTableView class]])
    {
        ((NSTableView*)documentView).backgroundColor = NSColor.clearColor;
    }
}

+ (void)applyToAccessoryViewController:(NSTitlebarAccessoryViewController*)controller
{
    NSView* originalView = controller.view;
    if ([originalView isKindOfClass:[NSGlassEffectView class]])
    {
        return;
    }

    // Same Regular style as the window content behind the torrent list (see
    // -applyToWindowBackground:scrollView:), so chrome and content read as
    // one consistent frosted material instead of two different textures.
    controller.view = [self glassWrapView:originalView style:NSGlassEffectViewStyleRegular];
}

+ (void)applyToRowView:(NSTableRowView*)rowView
{
    // The window content is now Regular-style glass (see
    // -applyToWindowBackground:scrollView:), which is already frosted and
    // legible on its own — rows just need to stay clear so it shows through
    // uniformly instead of adding a second, hand-tinted layer on top.
    rowView.backgroundColor = NSColor.clearColor;
}

+ (void)increaseFontSizeBy:(CGFloat)delta forTextField:(NSTextField*)textField
{
    if (textField == nil || objc_getAssociatedObject(textField, kFontSizeAlreadyIncreasedKey) != nil)
    {
        return;
    }

    textField.font = [NSFont fontWithDescriptor:textField.font.fontDescriptor size:textField.font.pointSize + delta];
    objc_setAssociatedObject(textField, kFontSizeAlreadyIncreasedKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

+ (void)improveLegibilityForTextField:(NSTextField*)textField
{
    if (textField == nil || objc_getAssociatedObject(textField, kLegibilityShadowAppliedKey) != nil)
    {
        return;
    }

    textField.wantsLayer = YES;

    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor.blackColor colorWithAlphaComponent:0.75];
    shadow.shadowOffset = NSMakeSize(0, 0);
    shadow.shadowBlurRadius = 2.5;
    textField.shadow = shadow;

    objc_setAssociatedObject(textField, kLegibilityShadowAppliedKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end
