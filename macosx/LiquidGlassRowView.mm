// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import "LiquidGlassRowView.h"

@implementation LiquidGlassRowView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        // NSOutlineView defaults to SourceList, which draws its own opaque
        // fill and never calls -drawSelectionInRect: — Regular is the style
        // that hands drawing to us.
        self.selectionHighlightStyle = NSTableViewSelectionHighlightStyleRegular;
    }
    return self;
}

- (void)drawSelectionInRect:(NSRect)dirtyRect
{
    CGFloat const alpha = self.isEmphasized ? 0.35 : 0.2;
    NSColor* tint = [NSColor.controlAccentColor colorWithAlphaComponent:alpha];

    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 1.0, 1.0) xRadius:8.0 yRadius:8.0];
    [tint setFill];
    [path fill];
}

@end
