const std = @import("std");
const c = @import("pcre2_c");
const ch = @import("pcre2_capture_history");
const options = @import("pcre2_test_options");

const width = options.code_unit_width;
const Unit = switch (width) {
    8 => u8,
    16 => u16,
    32 => u32,
    else => @compileError("unsupported code unit width"),
};
const Api = ch.Api(width);

// translate-c cannot expand PCRE2_SUFFIX, so name the width-suffixed symbols directly.
const sfx = std.fmt.comptimePrint("_{d}", .{width});
const Code = @field(c, "pcre2_code" ++ sfx);
const MatchData = @field(c, "pcre2_match_data" ++ sfx);
const compile = @field(c, "pcre2_compile" ++ sfx);
const code_free = @field(c, "pcre2_code_free" ++ sfx);
const match_data_create_from_pattern = @field(c, "pcre2_match_data_create_from_pattern" ++ sfx);
const match_data_free = @field(c, "pcre2_match_data_free" ++ sfx);
const pcre2_match = @field(c, "pcre2_match" ++ sfx);
const get_capture_event_pointer = @field(c, "pcre2_get_capture_event_pointer" ++ sfx);
const substring_number_from_name = @field(c, "pcre2_substring_number_from_name" ++ sfx);

fn units(comptime s: []const u8) [s.len]Unit {
    var out: [s.len]Unit = undefined;
    for (s, 0..) |byte, i| out[i] = byte;
    return out;
}

const Compiled = struct {
    code: *Code,
    md: *MatchData,

    fn init(comptime pattern: []const u8) !Compiled {
        const p = units(pattern);
        var err: c_int = 0;
        var off: usize = 0;
        const code = compile(&p, p.len, 0, &err, &off, null) orelse return error.Compile;
        const md = match_data_create_from_pattern(code, null) orelse return error.OutOfMemory;
        return .{ .code = code, .md = md };
    }

    fn deinit(self: Compiled) void {
        match_data_free(self.md);
        code_free(self.code);
    }

    fn match(self: Compiled, subject: []const Unit, opts: u32) c_int {
        return pcre2_match(self.code, subject.ptr, subject.len, 0, opts, self.md, null);
    }
};

test "CaptureEvent matches the C struct layout" {
    try std.testing.expectEqual(@sizeOf(c.pcre2_capture_event), @sizeOf(ch.CaptureEvent));
    try std.testing.expectEqual(@alignOf(c.pcre2_capture_event), @alignOf(ch.CaptureEvent));
    try std.testing.expectEqual(@offsetOf(c.pcre2_capture_event, "group"), @offsetOf(ch.CaptureEvent, "group"));
    try std.testing.expectEqual(@offsetOf(c.pcre2_capture_event, "start"), @offsetOf(ch.CaptureEvent, "start"));
    try std.testing.expectEqual(@offsetOf(c.pcre2_capture_event, "end"), @offsetOf(ch.CaptureEvent, "end"));
    try std.testing.expectEqual(@as(u32, c.PCRE2_CAPTURE_HISTORY), ch.option);
}

test "events borrows the match data's history without copying" {
    const re = try Compiled.init("(a)+");
    defer re.deinit();
    const subject = units("aaa");
    try std.testing.expectEqual(@as(c_int, 2), re.match(&subject, ch.option));
    const events = Api.events(re.md);
    try std.testing.expectEqualSlices(ch.CaptureEvent, &.{
        .{ .group = 1, .start = 0, .end = 1 },
        .{ .group = 1, .start = 1, .end = 2 },
        .{ .group = 1, .start = 2, .end = 3 },
    }, events);
    const raw: [*]const c.pcre2_capture_event = get_capture_event_pointer(re.md);
    try std.testing.expectEqual(@intFromPtr(raw), @intFromPtr(events.ptr));
}

test "events is empty after no match, without history, and on fresh match data" {
    const re = try Compiled.init("(a)+b?");
    defer re.deinit();
    try std.testing.expectEqual(@as(usize, 0), Api.events(re.md).len);
    const aaa = units("aaa");
    _ = re.match(&aaa, ch.option);
    const zzz = units("zzz");
    try std.testing.expectEqual(@as(c_int, c.PCRE2_ERROR_NOMATCH), re.match(&zzz, ch.option));
    try std.testing.expectEqual(@as(usize, 0), Api.events(re.md).len);
    _ = re.match(&aaa, ch.option);
    try std.testing.expectEqual(@as(c_int, 2), re.match(&aaa, 0));
    try std.testing.expectEqual(@as(usize, 0), Api.events(re.md).len);
}

test "group iterator yields one group's spans in order and slices the subject" {
    const re = try Compiled.init("(?=(?<unit>[ACGT]{4})(?:(?>[ACGT]{0,10}?(?<hit>\\k<unit>)))++)");
    defer re.deinit();
    const subject = units("ACGTTACGTTTACGTCCCCCCCCCCCCACGT");
    try std.testing.expectEqual(@as(c_int, 3), re.match(&subject, ch.option));
    const hit_name = units("hit");
    var name_z: [hit_name.len + 1]Unit = undefined;
    @memcpy(name_z[0..hit_name.len], &hit_name);
    name_z[hit_name.len] = 0;
    const hit: u32 = @intCast(substring_number_from_name(re.code, &name_z));
    var it = ch.groupIterator(Api.events(re.md), hit);
    var starts: [4]usize = undefined;
    var n: usize = 0;
    while (it.next()) |ev| : (n += 1) {
        starts[n] = ev.start;
        try std.testing.expectEqualSlices(Unit, subject[0..4], ev.slice(Unit, &subject));
    }
    try std.testing.expectEqualSlices(usize, &.{ 5, 11 }, starts[0..n]);
}
