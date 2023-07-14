const std = @import("std");

const ScanProtocolsStep = @import("deps/zig-wayland/build.zig").ScanProtocolsStep;

pub fn build(b: *std.build.Builder) void {
    const exe = b.addExecutable("tinytile", "src/main.zig");
    exe.setTarget(b.standardTargetOptions(.{}));
    exe.setBuildMode(b.standardReleaseOptions());

    const scanner = ScanProtocolsStep.create(b);
    scanner.addSystemProtocol("stable/xdg-shell/xdg-shell.xml");

    const wayland = std.build.Pkg{
        .name = "wayland",
        .source = .{ .generated = &scanner.result },
    };
    const xkbcommon = std.build.Pkg{
        .name = "xkbcommon",
        .source = .{ .path = "deps/zig-xkbcommon/src/xkbcommon.zig" },
    };
    const pixman = std.build.Pkg{
        .name = "pixman",
        .source = .{ .path = "deps/zig-pixman/pixman.zig" },
    };
    const wlroots = std.build.Pkg{
        .name = "wlroots",
        .source = .{ .path = "deps/zig-wlroots/src/wlroots.zig" },
        .dependencies = &[_]std.build.Pkg{ wayland, xkbcommon, pixman },
    };

    // These must be manually kept in sync with the versions wlroots supports
    // until wlroots gives the option to request a specific version.
    scanner.generate("wl_compositor", 4);
    scanner.generate("wl_subcompositor", 1);
    scanner.generate("wl_shm", 1);
    scanner.generate("wl_output", 4);
    scanner.generate("wl_seat", 7);
    scanner.generate("wl_data_device_manager", 3);
    scanner.generate("xdg_wm_base", 2);

    exe.linkLibC();
    exe.addPackage(wayland);
    exe.linkSystemLibrary("wayland-server");
    exe.step.dependOn(&scanner.step);
    // TODO: remove when https://github.com/ziglang/zig/issues/131 is implemented
    scanner.addCSource(exe);

    exe.addPackage(xkbcommon);
    exe.linkSystemLibrary("xkbcommon");

    exe.addPackage(wlroots);
    exe.linkSystemLibrary("wlroots");
    exe.linkSystemLibrary("pixman-1");

    exe.install();
}
