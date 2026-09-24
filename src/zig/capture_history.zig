//! Zig view of PCRE2 committed capture history (fork extension).
//! Events are borrowed from the match data: valid until it is reused or freed.

const std = @import("std");

/// PCRE2_CAPTURE_HISTORY match option.
pub const option: u32 = 0x00080000;

/// One capture-close event on the successful path; layout matches `pcre2_capture_event`.
pub const CaptureEvent = extern struct {
    group: u32,
    start: usize,
    end: usize,

    /// Slice the original subject by this event's code-unit offsets; no copy.
    pub fn slice(self: CaptureEvent, comptime Unit: type, subject: []const Unit) []const Unit {
        return subject[self.start..self.end];
    }
};

/// Width-specific entry points (`_8`, `_16`, `_32`) resolved at link time.
/// Accepts any pointer to match data, so it works with whichever C import the caller uses.
pub fn Api(comptime width: u8) type {
    const suffix = switch (width) {
        8 => "8",
        16 => "16",
        32 => "32",
        else => @compileError("code unit width must be 8, 16 or 32"),
    };
    return struct {
        const count_fn = @extern(*const fn (?*anyopaque) callconv(.c) usize, .{
            .name = "pcre2_get_capture_event_count_" ++ suffix,
        });
        const pointer_fn = @extern(*const fn (?*anyopaque) callconv(.c) ?[*]const CaptureEvent, .{
            .name = "pcre2_get_capture_event_pointer_" ++ suffix,
        });

        /// Borrowed chronological events of the last successful history match; empty otherwise.
        pub fn events(match_data: anytype) []const CaptureEvent {
            const md: ?*anyopaque = @ptrCast(match_data);
            const n = count_fn(md);
            if (n == 0) return &.{};
            return (pointer_fn(md) orelse return &.{})[0..n];
        }
    };
}

/// Zero-allocation filter over one group's events, in capture-close order.
pub const GroupIterator = struct {
    events: []const CaptureEvent,
    group: u32,
    index: usize = 0,

    pub fn next(self: *GroupIterator) ?CaptureEvent {
        while (self.index < self.events.len) {
            const ev = self.events[self.index];
            self.index += 1;
            if (ev.group == self.group) return ev;
        }
        return null;
    }
};

pub fn groupIterator(events: []const CaptureEvent, group: u32) GroupIterator {
    return .{ .events = events, .group = group };
}

test {
    std.testing.refAllDecls(@This());
}
