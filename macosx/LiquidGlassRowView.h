// This file Copyright © Transmission authors and contributors.
// It may be used under the MIT (SPDX: MIT) license.
// License text can be found in the licenses/ folder.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// A row view that paints selection as a translucent accent tint instead
/// of NSOutlineView's default opaque source-list fill, so the glass shows
/// through a selected row instead of disappearing behind a solid block.
@interface LiquidGlassRowView : NSTableRowView

@end

NS_ASSUME_NONNULL_END
