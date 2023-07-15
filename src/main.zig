const std = @import("std");
const os = std.os;

const wl = @import("wayland").server.wl;

const wlr = @import("wlroots");
const xkb = @import("xkbcommon");

const gpa = std.heap.c_allocator;

pub fn main() anyerror!void {
    wlr.log.init(.debug);

    var server: Server = undefined;
    try server.init();
    defer server.deinit();

    var buf: [11]u8 = undefined;
    const socket = try server.wl_server.addSocketAuto(&buf);

    if (os.argv.len >= 2) {
        const cmd = std.mem.span(os.argv[1]);
        var child = std.ChildProcess.init(&[_][]const u8{ "/bin/sh", "-c", cmd }, gpa);
        var env_map = try std.process.getEnvMap(gpa);
        defer env_map.deinit();
        try env_map.put("WAYLAND_DISPLAY", socket);
        child.env_map = &env_map;
        try child.spawn();
    }

    try server.backend.start();

    std.log.info("Running compositor on WAYLAND_DISPLAY={s}", .{socket});
    server.wl_server.run();
}

const Server = struct {
    wl_server: *wl.Server,
    backend: *wlr.Backend,
    renderer: *wlr.Renderer,
    allocator: *wlr.Allocator,
    scene: *wlr.Scene,

    output_layout: *wlr.OutputLayout,
    new_output: wl.Listener(*wlr.Output) = wl.Listener(*wlr.Output).init(newOutput),

    xdg_shell: *wlr.XdgShell,
    new_xdg_surface: wl.Listener(*wlr.XdgSurface) = wl.Listener(*wlr.XdgSurface).init(newXdgSurface),
    views: wl.list.Head(View, .link) = undefined,

    seat: *wlr.Seat,
    new_input: wl.Listener(*wlr.InputDevice) = wl.Listener(*wlr.InputDevice).init(newInput),
    request_set_cursor: wl.Listener(*wlr.Seat.event.RequestSetCursor) = wl.Listener(*wlr.Seat.event.RequestSetCursor).init(requestSetCursor),
    request_set_selection: wl.Listener(*wlr.Seat.event.RequestSetSelection) = wl.Listener(*wlr.Seat.event.RequestSetSelection).init(requestSetSelection),
    keyboards: wl.list.Head(Keyboard, .link) = undefined,

    cursor: *wlr.Cursor,
    cursor_mgr: *wlr.XcursorManager,
    cursor_motion: wl.Listener(*wlr.Pointer.event.Motion) = wl.Listener(*wlr.Pointer.event.Motion).init(cursorMotion),
    cursor_motion_absolute: wl.Listener(*wlr.Pointer.event.MotionAbsolute) = wl.Listener(*wlr.Pointer.event.MotionAbsolute).init(cursorMotionAbsolute),
    cursor_button: wl.Listener(*wlr.Pointer.event.Button) = wl.Listener(*wlr.Pointer.event.Button).init(cursorButton),
    cursor_axis: wl.Listener(*wlr.Pointer.event.Axis) = wl.Listener(*wlr.Pointer.event.Axis).init(cursorAxis),
    cursor_frame: wl.Listener(*wlr.Cursor) = wl.Listener(*wlr.Cursor).init(cursorFrame),

    fn init(server: *Server) !void {
        const wl_server = try wl.Server.create();
        const backend = try wlr.Backend.autocreate(wl_server);
        const renderer = try wlr.Renderer.autocreate(backend);
        server.* = .{
            .wl_server = wl_server,
            .backend = backend,
            .renderer = renderer,
            .allocator = try wlr.Allocator.autocreate(backend, renderer),
            .scene = try wlr.Scene.create(),

            .output_layout = try wlr.OutputLayout.create(),
            .xdg_shell = try wlr.XdgShell.create(wl_server, 2),
            .seat = try wlr.Seat.create(wl_server, "default"),
            .cursor = try wlr.Cursor.create(),
            .cursor_mgr = try wlr.XcursorManager.create(null, 24),
        };

        try server.renderer.initServer(wl_server);
        try server.scene.attachOutputLayout(server.output_layout);

        _ = try wlr.Compositor.create(server.wl_server, server.renderer);
        _ = try wlr.Subcompositor.create(server.wl_server);
        _ = try wlr.DataDeviceManager.create(server.wl_server);

        server.backend.events.new_output.add(&server.new_output);

        server.xdg_shell.events.new_surface.add(&server.new_xdg_surface);
        server.views.init();

        server.backend.events.new_input.add(&server.new_input);
        server.seat.events.request_set_cursor.add(&server.request_set_cursor);
        server.seat.events.request_set_selection.add(&server.request_set_selection);
        server.keyboards.init();

        server.cursor.attachOutputLayout(server.output_layout);
        try server.cursor_mgr.load(1);
        server.cursor.events.motion.add(&server.cursor_motion);
        server.cursor.events.motion_absolute.add(&server.cursor_motion_absolute);
        server.cursor.events.button.add(&server.cursor_button);
        server.cursor.events.axis.add(&server.cursor_axis);
        server.cursor.events.frame.add(&server.cursor_frame);
    }

    fn deinit(server: *Server) void {
        server.wl_server.destroyClients();
        server.wl_server.destroy();
    }

    fn newOutput(listener: *wl.Listener(*wlr.Output), wlr_output: *wlr.Output) void {
        const server = @fieldParentPtr(Server, "new_output", listener);

        if (!wlr_output.initRender(server.allocator, server.renderer)) return;

        if (wlr_output.preferredMode()) |mode| {
            wlr_output.setMode(mode);
            wlr_output.enable(true);
            wlr_output.commit() catch return;
        }

        const output = gpa.create(Output) catch {
            std.log.err("failed to allocate new output", .{});
            return;
        };

        output.* = .{
            .server = server,
            .wlr_output = wlr_output,
        };

        wlr_output.events.frame.add(&output.frame);

        server.output_layout.addAuto(wlr_output);
    }

    fn newXdgSurface(listener: *wl.Listener(*wlr.XdgSurface), xdg_surface: *wlr.XdgSurface) void {
        const server = @fieldParentPtr(Server, "new_xdg_surface", listener);

        switch (xdg_surface.role) {
            .toplevel => {
                // Don't add the view to server.views until it is mapped
                const view = gpa.create(View) catch {
                    std.log.err("failed to allocate new view", .{});
                    return;
                };

                view.* = .{
                    .server = server,
                    .xdg_surface = xdg_surface,
                    .scene_tree = server.scene.tree.createSceneXdgSurface(xdg_surface) catch {
                        gpa.destroy(view);
                        std.log.err("failed to allocate new view", .{});
                        return;
                    },
                };
                view.scene_tree.node.data = @ptrToInt(view);
                xdg_surface.data = @ptrToInt(view.scene_tree);

                xdg_surface.events.map.add(&view.map);
                xdg_surface.events.unmap.add(&view.unmap);
                xdg_surface.events.destroy.add(&view.destroy);
            },
            .popup => {
                // These asserts are fine since tinywl.zig doesn't support anything else that can
                // make xdg popups (e.g. layer shell).
                const parent = wlr.XdgSurface.fromWlrSurface(xdg_surface.role_data.popup.parent.?) orelse return;
                const parent_tree = @intToPtr(?*wlr.SceneTree, parent.data) orelse {
                    // The xdg surface user data could be left null due to allocation failure.
                    return;
                };
                const scene_tree = parent_tree.createSceneXdgSurface(xdg_surface) catch {
                    std.log.err("failed to allocate xdg popup node", .{});
                    return;
                };
                xdg_surface.data = @ptrToInt(scene_tree);
            },
            .none => unreachable,
        }
    }

    const ViewAtResult = struct {
        view: *View,
        surface: *wlr.Surface,
        sx: f64,
        sy: f64,
    };

    fn viewAt(server: *Server, lx: f64, ly: f64) ?ViewAtResult {
        var sx: f64 = undefined;
        var sy: f64 = undefined;
        if (server.scene.tree.node.at(lx, ly, &sx, &sy)) |node| {
            if (node.type != .buffer) return null;
            const scene_buffer = wlr.SceneBuffer.fromNode(node);
            const scene_surface = wlr.SceneSurface.fromBuffer(scene_buffer) orelse return null;

            var it: ?*wlr.SceneNode = node;
            while (it) |n| : (it = n.parent) {
                if (@intToPtr(?*View, n.data)) |view| {
                    return ViewAtResult{
                        .view = view,
                        .surface = scene_surface.surface,
                        .sx = sx,
                        .sy = sy,
                    };
                }
            }
        }
        return null;
    }

    fn focusView(server: *Server, view: *View, surface: *wlr.Surface) void {
        if (server.seat.keyboard_state.focused_surface) |previous_surface| {
            if (previous_surface == surface) return;
            if (previous_surface.isXdgSurface()) {
                const xdg_surface = wlr.XdgSurface.fromWlrSurface(previous_surface) orelse return;
                _ = xdg_surface.role_data.toplevel.setActivated(false);
            }
        }

        view.scene_tree.node.raiseToTop();
        view.link.remove();
        server.views.prepend(view);

        _ = view.xdg_surface.role_data.toplevel.setActivated(true);

        const wlr_keyboard = server.seat.getKeyboard() orelse return;
        server.seat.keyboardNotifyEnter(
            surface,
            &wlr_keyboard.keycodes,
            wlr_keyboard.num_keycodes,
            &wlr_keyboard.modifiers,
        );
    }

    fn newInput(listener: *wl.Listener(*wlr.InputDevice), device: *wlr.InputDevice) void {
        const server = @fieldParentPtr(Server, "new_input", listener);
        switch (device.type) {
            .keyboard => Keyboard.create(server, device) catch |err| {
                std.log.err("failed to create keyboard: {}", .{err});
                return;
            },
            .pointer => server.cursor.attachInputDevice(device),
            else => {},
        }

        server.seat.setCapabilities(.{
            .pointer = true,
            .keyboard = server.keyboards.length() > 0,
        });
    }

    fn requestSetCursor(
        listener: *wl.Listener(*wlr.Seat.event.RequestSetCursor),
        event: *wlr.Seat.event.RequestSetCursor,
    ) void {
        const server = @fieldParentPtr(Server, "request_set_cursor", listener);
        if (event.seat_client == server.seat.pointer_state.focused_client)
            server.cursor.setSurface(event.surface, event.hotspot_x, event.hotspot_y);
    }

    fn requestSetSelection(
        listener: *wl.Listener(*wlr.Seat.event.RequestSetSelection),
        event: *wlr.Seat.event.RequestSetSelection,
    ) void {
        const server = @fieldParentPtr(Server, "request_set_selection", listener);
        server.seat.setSelection(event.source, event.serial);
    }

    fn cursorMotion(
        listener: *wl.Listener(*wlr.Pointer.event.Motion),
        event: *wlr.Pointer.event.Motion,
    ) void {
        const server = @fieldParentPtr(Server, "cursor_motion", listener);
        server.cursor.move(event.device, event.delta_x, event.delta_y);
        server.processCursorMotion(event.time_msec);
    }

    fn cursorMotionAbsolute(
        listener: *wl.Listener(*wlr.Pointer.event.MotionAbsolute),
        event: *wlr.Pointer.event.MotionAbsolute,
    ) void {
        const server = @fieldParentPtr(Server, "cursor_motion_absolute", listener);
        server.cursor.warpAbsolute(event.device, event.x, event.y);
        server.processCursorMotion(event.time_msec);
    }

    fn processCursorMotion(server: *Server, time_msec: u32) void {
        if (server.viewAt(server.cursor.x, server.cursor.y)) |res| {
            server.seat.pointerNotifyEnter(res.surface, res.sx, res.sy);
            server.seat.pointerNotifyMotion(time_msec, res.sx, res.sy);
        } else {
            server.cursor_mgr.setCursorImage("left_ptr", server.cursor);
            server.seat.pointerClearFocus();
        }
    }

    fn cursorButton(
        listener: *wl.Listener(*wlr.Pointer.event.Button),
        event: *wlr.Pointer.event.Button,
    ) void {
        const server = @fieldParentPtr(Server, "cursor_button", listener);
        _ = server.seat.pointerNotifyButton(event.time_msec, event.button, event.state);
        if (event.state == .pressed) {
            if (server.viewAt(server.cursor.x, server.cursor.y)) |res| {
                server.focusView(res.view, res.surface);
            }
        }
    }

    fn cursorAxis(
        listener: *wl.Listener(*wlr.Pointer.event.Axis),
        event: *wlr.Pointer.event.Axis,
    ) void {
        const server = @fieldParentPtr(Server, "cursor_axis", listener);
        server.seat.pointerNotifyAxis(
            event.time_msec,
            event.orientation,
            event.delta,
            event.delta_discrete,
            event.source,
        );
    }

    fn cursorFrame(listener: *wl.Listener(*wlr.Cursor), _: *wlr.Cursor) void {
        const server = @fieldParentPtr(Server, "cursor_frame", listener);
        server.seat.pointerNotifyFrame();
    }

    /// Assumes the modifier used for compositor keybinds is pressed
    /// Returns true if the key was handled
    fn handleKeybind(server: *Server, key: xkb.Keysym) bool {
        switch (@enumToInt(key)) {
            // Exit the compositor
            xkb.Keysym.Escape => server.wl_server.terminate(),
            // Closes focused window
            xkb.Keysym.q => @fieldParentPtr(View, "link", server.views.link.next.?).xdg_surface.role_data.toplevel.sendClose(),
            // Focus the next view in the stack, pushing the current top to the back
            xkb.Keysym.F1 => {
                if (server.views.length() < 2) return true;
                const view = @fieldParentPtr(View, "link", server.views.link.prev.?);
                server.focusView(view, view.xdg_surface.surface);
            },
            else => return false,
        }
        return true;
    }
};

const Output = struct {
    server: *Server,
    wlr_output: *wlr.Output,

    frame: wl.Listener(*wlr.Output) = wl.Listener(*wlr.Output).init(frame),

    fn frame(listener: *wl.Listener(*wlr.Output), _: *wlr.Output) void {
        const output = @fieldParentPtr(Output, "frame", listener);

        const scene_output = output.server.scene.getSceneOutput(output.wlr_output).?;
        _ = scene_output.commit();

        var now: os.timespec = undefined;
        os.clock_gettime(os.CLOCK.MONOTONIC, &now) catch @panic("CLOCK_MONOTONIC not supported");
        scene_output.sendFrameDone(&now);
    }
};

const View = struct {
    server: *Server,
    link: wl.list.Link = undefined,
    xdg_surface: *wlr.XdgSurface,
    scene_tree: *wlr.SceneTree,

    map: wl.Listener(void) = wl.Listener(void).init(map),
    unmap: wl.Listener(void) = wl.Listener(void).init(unmap),
    destroy: wl.Listener(void) = wl.Listener(void).init(destroy),

    fn map(listener: *wl.Listener(void)) void {
        const view = @fieldParentPtr(View, "map", listener);
        const monitor = @ptrCast(*wlr.Output, wlr.OutputLayout.outputAt(view.server.output_layout, view.server.cursor.x, view.server.cursor.y));
        const monitor_pos = @ptrCast(*wlr.OutputLayout.Output, wlr.OutputLayout.get(view.server.output_layout, monitor));
        _ = view.xdg_surface.role_data.toplevel.setSize(monitor.width, monitor.height);
        view.scene_tree.node.setPosition(monitor_pos.x, monitor_pos.y);
        view.server.views.prepend(view);
        view.server.focusView(view, view.xdg_surface.surface);
    }

    fn unmap(listener: *wl.Listener(void)) void {
        const view = @fieldParentPtr(View, "unmap", listener);
        view.link.remove();
    }

    fn destroy(listener: *wl.Listener(void)) void {
        const view = @fieldParentPtr(View, "destroy", listener);

        view.map.link.remove();
        view.unmap.link.remove();
        view.destroy.link.remove();

        gpa.destroy(view);
    }
};

const Keyboard = struct {
    server: *Server,
    link: wl.list.Link = undefined,
    device: *wlr.InputDevice,

    modifiers: wl.Listener(*wlr.Keyboard) = wl.Listener(*wlr.Keyboard).init(modifiers),
    key: wl.Listener(*wlr.Keyboard.event.Key) = wl.Listener(*wlr.Keyboard.event.Key).init(key),

    fn create(server: *Server, device: *wlr.InputDevice) !void {
        const keyboard = try gpa.create(Keyboard);
        errdefer gpa.destroy(keyboard);

        keyboard.* = .{
            .server = server,
            .device = device,
        };

        const context = xkb.Context.new(.no_flags) orelse return error.ContextFailed;
        defer context.unref();
        const keymap = xkb.Keymap.newFromNames(context, null, .no_flags) orelse return error.KeymapFailed;
        defer keymap.unref();

        const wlr_keyboard = device.toKeyboard();
        if (!wlr_keyboard.setKeymap(keymap)) return error.SetKeymapFailed;
        wlr_keyboard.setRepeatInfo(25, 600);

        wlr_keyboard.events.modifiers.add(&keyboard.modifiers);
        wlr_keyboard.events.key.add(&keyboard.key);

        server.seat.setKeyboard(wlr_keyboard);
        server.keyboards.append(keyboard);
    }

    fn modifiers(listener: *wl.Listener(*wlr.Keyboard), wlr_keyboard: *wlr.Keyboard) void {
        const keyboard = @fieldParentPtr(Keyboard, "modifiers", listener);
        keyboard.server.seat.setKeyboard(wlr_keyboard);
        keyboard.server.seat.keyboardNotifyModifiers(&wlr_keyboard.modifiers);
    }

    fn key(listener: *wl.Listener(*wlr.Keyboard.event.Key), event: *wlr.Keyboard.event.Key) void {
        const keyboard = @fieldParentPtr(Keyboard, "key", listener);
        const wlr_keyboard = keyboard.device.toKeyboard();

        // Translate libinput keycode -> xkbcommon
        const keycode = event.keycode + 8;

        var handled = false;
        if (wlr_keyboard.getModifiers().alt and event.state == .pressed) {
            for (wlr_keyboard.xkb_state.?.keyGetSyms(keycode)) |sym| {
                if (keyboard.server.handleKeybind(sym)) {
                    handled = true;
                    break;
                }
            }
        }

        if (!handled) {
            keyboard.server.seat.setKeyboard(wlr_keyboard);
            keyboard.server.seat.keyboardNotifyKey(event.time_msec, event.keycode, event.state);
        }
    }
};
