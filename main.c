#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

//////////////////////////////////
//      STRUCTS AND ENUMS       //
// (annotated for brevity sake) //
//////////////////////////////////

// scene layers
enum { TinywlLyrClients, TinywlLyrMessage, TinywlLyrDragIcon, NUM_TINYWL_LAYERS };

enum tinywl_cursor_mode {
	TINYWL_CURSOR_PASSTHROUGH,
	TINYWL_CURSOR_MOVE,
	TINYWL_CURSOR_RESIZE,
	TINYWL_CURSOR_PRESSED,
};

enum tinywl_message_type {
	TinywlMsgNone,
	TinywlMsgHello,
	TinywlMsgRun, // TODO
	TinywlMsgClientsList,
};

struct tinywl_text_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

struct tinywl_message {
	struct tinywl_text_buffer buffer;
	struct wlr_scene_buffer *message;
	struct wl_surface *surface;
	enum tinywl_message_type type;
};

struct tinywl_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_tree *layers[NUM_TINYWL_LAYERS];

	bool ignoreNextAltRelease;
	struct tinywl_message message;

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list views;
	struct tinywl_view *focused_view;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener new_virtual_keyboard;
	struct wl_listener request_cursor;
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum tinywl_cursor_mode cursor_mode;
	struct tinywl_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
};

struct tinywl_output {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener destroy;
};

struct tinywl_view {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	int x, y, width, height;
};

struct tinywl_keyboard {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

// Allow these files to access above code
#include <config.h>
#include <popup_message.h>

//////////////////////
// HELPER FUNCTIONS //
//////////////////////

static void run(char *cmdTxt) {
	// Simply runs a command in a forked process
	if (fork() == 0) {
		exit(system(cmdTxt));
	}
}

static struct tinywl_view *getPrevView(struct tinywl_server *server, bool wrap) {
	struct tinywl_view *view;
	if (server->focused_view->link.prev != &server->views)
		return wl_container_of(server->focused_view->link.prev, view, link);
	else if (wrap)
		// wrap past sentinal node
		return wl_container_of(server->focused_view->link.prev->prev, view, link);
	return server->focused_view;
}

static struct tinywl_view *getNextView(struct tinywl_server *server, bool wrap) {
	struct tinywl_view *view;
	if (server->focused_view->link.next != &server->views)
		return wl_container_of(server->focused_view->link.next, view, link);
	else if (wrap)
		// wrap past sentinal node
		return wl_container_of(server->focused_view->link.next->next, view, link);
	return server->focused_view;
}

static inline struct wlr_output *getMonitorViewIsOn(struct tinywl_view *view) {
	return wlr_output_layout_output_at(view->server->output_layout, view->x, view->y);
}

static struct tinywl_view *getPopupViewFromParent(struct tinywl_view *parent_view) {
	struct tinywl_view *popup_view;
	wl_list_for_each (popup_view, &parent_view->server->views, link)
		if (popup_view->xdg_toplevel->parent == parent_view->xdg_toplevel)
			return popup_view;
	return parent_view;
}

static struct tinywl_view *getParentViewFromPopup(struct tinywl_view *popup_view) {
	struct tinywl_view *parent_view;
	wl_list_for_each (parent_view, &popup_view->server->views, link)
		if (popup_view->xdg_toplevel->parent == parent_view->xdg_toplevel)
			return parent_view;
	return NULL;
}

static inline bool viewUsesWholeScreen(struct tinywl_view *view) {
	return view->xdg_toplevel->current.fullscreen || view->xdg_toplevel->current.maximized;
}

static void move_view(struct tinywl_view *view, int newX, int newY) {
	view->x = newX;
	view->y = newY;
	wlr_scene_node_set_position(&view->scene_tree->node, newX, newY);
}

static void resize_view(struct tinywl_view *view, int newWidth, int newHeight) {
	view->width = newWidth;
	view->height = newHeight;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, newWidth, newHeight);
}

static void displayClientList(struct tinywl_server *server) {
	if (wl_list_length(&server->views) == 0)
		message_print(server, "No clients open", TinywlMsgClientsList);
	else {
		char message[800] = "Clients:";

		struct tinywl_view *view;
		wl_list_for_each (view, &server->views, link) {
			if (view == server->focused_view)
				strcat(message, "\n -> ");
			else
				strcat(message, "\n    ");
			strcat(message, view->xdg_toplevel->title);
		}

		message_print(server, message, TinywlMsgClientsList);
	}
}

static void reset_cursor_mode(struct tinywl_server *server) {
	// Reset the cursor mode to passthrough.
	server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	server->grabbed_view = NULL;
}

static struct tinywl_view *desktop_view_at(struct tinywl_server *server, double lx, double ly,
                                           struct wlr_surface **surface, double *sx, double *sy) {
	// This returns the topmost node in the scene at the given layout
	// coords. we only care about surface nodes as we are specifically
	// looking for a surface in the surface tree of a tinywl_view.
	struct wlr_scene_node *node =
	        wlr_scene_node_at(&server->layers[TinywlLyrClients]->node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	// Find the node corresponding to the tinywl_view at the root of this
	// surface tree, it is the only one for which we set the data field.
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

static void process_motion(struct tinywl_server *server, uint32_t time) {
	// If their is the possibility of the client underneath the cursor changing EG: you close a
	// client, then this function should be ran; it considers which client is under the cursor
	// and then sends the cursor event to that client and if no clients are under the cursor it
	// resets the cursor image.
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct tinywl_view *view =
	        desktop_view_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (!view && !seat->drag && server->cursor_mode != TINYWL_CURSOR_PRESSED) {
		// If there's no view under the cursor, set the cursor image to
		// the default. This is what makes the cursor image appear when
		// you move it around the screen, not over any views.
		wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr",
		                                     server->cursor);
	}
	if (server->cursor_mode == TINYWL_CURSOR_PRESSED && !seat->drag) {
		// Send pointer events to the view which the mouse was pressed on
		sx = server->cursor->x - server->grab_x;
		sy = server->cursor->y - server->grab_y;
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else if (surface) {
		// Send pointer enter and motion events.
		//
		// The enter event gives the surface "pointer focus", which is
		// distinct from keyboard focus. You get pointer focus by moving
		// the pointer over a client.
		//
		// Note that wlroots will avoid sending duplicate enter/motion
		// events if the surface has already has pointer focus or if the
		// client is already aware of the coordinates passed.
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		// Clear pointer focus so future button events and such are not
		// sent to the last client to have the cursor over it.
		wlr_seat_pointer_clear_focus(seat);
	}
}

static bool maximizeView(struct tinywl_view *view) {
	struct wlr_output *monitor = getMonitorViewIsOn(view);
	if (monitor) { // If the window is within the bounds of any monitor
		struct wlr_output_layout_output *monitor_pos =
		        wlr_output_layout_get(view->server->output_layout, monitor);

		wlr_scene_node_set_position(&view->scene_tree->node, monitor_pos->x,
		                            monitor_pos->y);
		wlr_xdg_toplevel_set_size(view->xdg_toplevel, monitor->width, monitor->height);
	}
	return monitor ? true : false;
}

static bool unmaximizeView(struct tinywl_view *view) {
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, view->width, view->height);
	return true;
}

static void focus_view(struct tinywl_view *parent_view) {
	if (parent_view) {
		struct tinywl_server *server = parent_view->server;
		struct wlr_seat *seat = server->seat;
		struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

		// Set parent view and view
		struct tinywl_view *view = getPopupViewFromParent(parent_view);
		if (parent_view->xdg_toplevel->parent)
			parent_view = getParentViewFromPopup(view);

		// Don't re-focus an already focused surface.
		if (prev_surface == view->xdg_toplevel->base->surface)
			return;

		if (prev_surface) {
			// Deactivate the previously focused surface. This lets the
			// client know it no longer has focus and the client will
			// repaint accordingly, e.g. stop displaying a caret.
			struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
			        seat->keyboard_state.focused_surface);
			assert(previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
			wlr_xdg_toplevel_set_activated(previous->toplevel, false);
		}
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

		// Move the view to the front
		wlr_scene_node_raise_to_top(&parent_view->scene_tree->node);
		server->focused_view = view;

		// Activate the new surface
		wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

		// Tell the seat to have the keyboard enter this surface. wlroots will
		// keep track of this and automatically send key events to the
		// appropriate clients without additional work on your part.
		if (keyboard != NULL) {
			wlr_seat_keyboard_notify_enter(seat, view->xdg_toplevel->base->surface,
			                               keyboard->keycodes, keyboard->num_keycodes,
			                               &keyboard->modifiers);
		}
		process_motion(server, 0);
	}
}

//////////////////////
// MANAGING CLIENTS //
//////////////////////

static void killfocused(struct tinywl_server *server) {
	// Note that this kills the client that has keyboard focus rathor then pointer focus
	if (server->focused_view)
		wlr_xdg_toplevel_send_close(server->focused_view->xdg_toplevel);
}

static void createClientSideDecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *dec = data;
	wlr_xdg_toplevel_decoration_v1_set_mode(dec,
	                                        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	// Called when the surface is mapped, or ready to display on-screen.
	struct tinywl_view *view = wl_container_of(listener, view, map);

	// Get location to place view for it to be centered
	struct wlr_box view_box;
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &view_box);

	if (view->xdg_toplevel->parent) {
		struct wlr_box parent_box;
		wlr_xdg_surface_get_geometry(view->xdg_toplevel->parent->base, &parent_box);
		view->x = (parent_box.width - view_box.width) / 2;
		view->y = (parent_box.height - view_box.height) / 2;
	} else {
		struct wlr_output *output = wlr_output_layout_output_at(view->server->output_layout,
		                                                        view->server->cursor->x,
		                                                        view->server->cursor->y);
		view->x = (output->width - view_box.width) / 2;
		view->y = (output->height - view_box.height) / 2;
	}

	if (makeWindowsTile) {
		wlr_xdg_toplevel_set_tiled(view->xdg_toplevel, WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
		                                                       WLR_EDGE_LEFT |
		                                                       WLR_EDGE_RIGHT);
	}

	// Actually center it
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);

	// Insert the view into the list of views
	if (view->server->focused_view)
		wl_list_insert(&view->server->focused_view->link, &view->link);
	else
		wl_list_insert(&view->server->views, &view->link);

	focus_view(view);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	// Called when the surface is unmapped, and should no longer be shown.
	struct tinywl_view *view = wl_container_of(listener, view, unmap);

	// Reset the cursor mode if the grabbed view was unmapped.
	if (view == view->server->grabbed_view) {
		reset_cursor_mode(view->server);
	}

	// If there are views that can be focused but aren't, then focus them
	if (view->server->focused_view == view && wl_list_length(&view->server->views) > 1) {
		struct tinywl_view *view_to_be_focused = getPrevView(view->server, true);
		wl_list_remove(&view->link);
		// We do not need to process motion if we will change the focused view as
		// the focus view function already does that for us
		focus_view(view_to_be_focused);
	} else {
		wl_list_remove(&view->link);
		process_motion(view->server, 0);
		view->server->focused_view = NULL;
	}
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	// Called when the surface is destroyed and should never be shown again.
	struct tinywl_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);

	free(view);
}

static void begin_interactive(struct tinywl_view *view, enum tinywl_cursor_mode mode,
                              uint32_t edges) {
	// This function sets up an interactive move or resize operation, where
	// the compositor stops propegating pointer events to clients and
	// instead consumes them itself, to move or resize clients.
	if (view) {
		struct tinywl_server *server = view->server;
		struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
		if (view->xdg_toplevel->base->surface !=
		    wlr_surface_get_root_surface(focused_surface)) {
			// Deny move/resize requests from clients without pointer focus
			return;
		}

		if (mode == TINYWL_CURSOR_MOVE && view->xdg_toplevel->parent)
			// If we are moving a popup then instead move the parent
			view = getParentViewFromPopup(view);

		server->grabbed_view = view;
		server->cursor_mode = mode;

		if (mode == TINYWL_CURSOR_MOVE) {
			server->grab_x = server->cursor->x - view->x;
			server->grab_y = server->cursor->y - view->y;
		} else {
			struct wlr_box geo_box;
			wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);

			double border_x = (view->x + geo_box.x) +
			                  ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
			double border_y = (view->y + geo_box.y) +
			                  ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
			server->grab_x = server->cursor->x - border_x;
			server->grab_y = server->cursor->y - border_y;

			server->grab_geobox = geo_box;
			server->grab_geobox.x += view->x;
			server->grab_geobox.y += view->y;

			server->resize_edges = edges;
		}
	}
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
	// This event is raised when a client would like to begin an interactive
	// move, typically because the user clicked on their client-side
	// decorations. Note that a more sophisticated compositor should check
	// the provided serial against a list of button press serials sent to
	// this client, to prevent the client from requesting this whenever they
	// want.
	struct tinywl_view *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, TINYWL_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	// This event is raised when a client would like to begin an interactive
	// resize, typically because the user clicked on their client-side
	// decorations. Note that a more sophisticated compositor should check
	// the provided serial against a list of button press serials sent to
	// this client, to prevent the client from requesting this whenever they
	// want.
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct tinywl_view *view = wl_container_of(listener, view, request_resize);
	begin_interactive(view, TINYWL_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	// This event is raised when a client would like to maximize itself,
	// typically because the user clicked on the maximize button on
	// client-side decorations.
	struct tinywl_view *view = wl_container_of(listener, view, request_maximize);

	if ((view->xdg_toplevel->requested.maximized && maximizeView(view)) ||
	    (!view->xdg_toplevel->requested.maximized && unmaximizeView(view)))
		wlr_xdg_toplevel_set_maximized(view->xdg_toplevel,
		                               view->xdg_toplevel->requested.maximized);

	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	struct tinywl_view *view = wl_container_of(listener, view, request_fullscreen);

	if ((view->xdg_toplevel->requested.fullscreen && maximizeView(view)) ||
	    (!view->xdg_toplevel->requested.fullscreen))
		wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel,
		                                view->xdg_toplevel->requested.fullscreen);

	if (!view->xdg_toplevel->requested.fullscreen && !view->xdg_toplevel->current.maximized)
		unmaximizeView(view);

	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	// This event is raised when wlr_xdg_shell receives a new xdg surface
	// from a client, either a toplevel (application window) or popup.
	struct tinywl_server *server = wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	// We must add xdg popups to the scene graph so they get rendered. The
	// wlroots scene graph provides a helper for this, but to use it we must
	// provide the proper parent scene node of the xdg popup. To enable
	// this, we always set the user data field of xdg_surfaces to the
	// corresponding scene node.
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent =
		        wlr_xdg_surface_from_wlr_surface(xdg_surface->popup->parent);
		struct wlr_scene_tree *parent_tree = parent->data;
		xdg_surface->data = wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
		return;
	}
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	// Allocate a tinywl_view for this surface
	struct tinywl_view *view = calloc(1, sizeof(struct tinywl_view));
	view->server = server;
	view->xdg_toplevel = xdg_surface->toplevel;

	if (view->xdg_toplevel->parent != 0)
		// if the view is a popup then create it as part of its parent
		view->scene_tree = wlr_scene_xdg_surface_create(
		        view->xdg_toplevel->parent->base->data, view->xdg_toplevel->base);
	else
		// otherwise create it as part of the typical scene tree
		view->scene_tree = wlr_scene_xdg_surface_create(server->layers[TinywlLyrClients],
		                                                view->xdg_toplevel->base);
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;

	// Listen to the various events it can emit
	view->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);

	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	// cotd
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;

	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);

	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);

	view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
}

////////////////////////////////////////
// MANAGING KEYBOARDS AND KEYBINDINGS //
////////////////////////////////////////

static bool handle_altbinding(struct tinywl_server *server, xkb_keysym_t sym) {
	// Here we handle compositor keybindings where you press alt and nother key.
	// This is when the compositor is processing keys, rather than passing them
	// on to the client for its own processing. Note: returning true indicates
	// the keybinding has been handled and will not be passed to the client,
	// otherwise the keybinding will be passed to the client
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_q:
		killfocused(server);
		break;
	case XKB_KEY_Return:
		run(term_cmd);
		break;
	case XKB_KEY_x:
		run("systemctl suspend");
		break;
	case XKB_KEY_a:
		displayClientList(server);
		break;
	case XKB_KEY_f:
		if (server->focused_view) {
			if (!viewUsesWholeScreen(server->focused_view)) {
				maximizeView(server->focused_view);
				wlr_xdg_toplevel_set_maximized(server->focused_view->xdg_toplevel,
				                               true);
			} else {
				unmaximizeView(server->focused_view);
				wlr_xdg_toplevel_set_fullscreen(server->focused_view->xdg_toplevel,
				                                false);
				wlr_xdg_toplevel_set_maximized(server->focused_view->xdg_toplevel,
				                               false);
			}
			wlr_xdg_surface_schedule_configure(
			        server->focused_view->xdg_toplevel->base);
		}
		break;
	case XKB_KEY_w:
		// Cycle to the previous view
		if (wl_list_length(&server->views) > 1) {
			struct tinywl_view *prev_view = getPrevView(server, wrapClientPicker);
			wlr_scene_node_raise_to_top(&prev_view->scene_tree->node);
			server->focused_view = prev_view;
		}
		displayClientList(server);
		break;
	case XKB_KEY_s:
		// Cycle to the next view
		if (wl_list_length(&server->views) > 1) {
			struct tinywl_view *next_view = getNextView(server, wrapClientPicker);
			wlr_scene_node_raise_to_top(&next_view->scene_tree->node);
			server->focused_view = next_view;
		}
		displayClientList(server);
		break;
	case XKB_KEY_e:
		run(filemanager_cmd);
		break;
	case XKB_KEY_b:
		run(browser_cmd);
		break;
	case XKB_KEY_h:
		run("xdg-open "
		    "https://github.com/godalming123/tinytile/blob/0.16.x/readme.md#usage");
		break;
	default:
		return false;
	}
	return true;
}

static bool handle_logobinding(struct tinywl_server *server, xkb_keysym_t sym) {
	switch (sym) {
	case XKB_KEY_w:
		if (wl_list_length(&server->views) > 1) {
			struct wl_list *previous = server->focused_view->link.prev->prev;
			wl_list_remove(&server->focused_view->link);
			wl_list_insert(previous, &server->focused_view->link);
		}
		displayClientList(server);
		break;
	case XKB_KEY_s:
		if (wl_list_length(&server->views) > 1) {
			struct wl_list *next = server->focused_view->link.next;
			wl_list_remove(&server->focused_view->link);
			wl_list_insert(next, &server->focused_view->link);
		}
		displayClientList(server);
		break;
	default:
		return false;
	}
	return true;
}

static bool handle_altctrl_binding(struct tinywl_server *server, xkb_keysym_t sym) {
	if (server->focused_view && !viewUsesWholeScreen(server->focused_view)) {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(server->focused_view->xdg_toplevel->base, &geo_box);
		switch (sym) {
		case XKB_KEY_w:
			resize_view(server->focused_view, geo_box.width,
			            geo_box.height - pixelsToMoveWindows);
			break;
		case XKB_KEY_a:
			resize_view(server->focused_view, geo_box.width - pixelsToMoveWindows,
			            geo_box.height);
			break;
		case XKB_KEY_s:
			resize_view(server->focused_view, geo_box.width,
			            geo_box.height + pixelsToMoveWindows);
			break;
		case XKB_KEY_d:
			resize_view(server->focused_view, geo_box.width + pixelsToMoveWindows,
			            geo_box.height);
			break;
		default:
			return false;
		}
	}
	return true;
}

static bool handle_altshift_biding(struct tinywl_server *server, xkb_keysym_t sym) {
	if (server->focused_view && !viewUsesWholeScreen(server->focused_view))
		switch (sym) {
		case XKB_KEY_W:
			move_view(server->focused_view, server->focused_view->x,
			          server->focused_view->y - pixelsToMoveWindows);
			break;
		case XKB_KEY_A:
			move_view(server->focused_view,
			          server->focused_view->x - pixelsToMoveWindows,
			          server->focused_view->y);
			break;
		case XKB_KEY_S:
			move_view(server->focused_view, server->focused_view->x,
			          server->focused_view->y + pixelsToMoveWindows);
			break;
		case XKB_KEY_D:
			move_view(server->focused_view,
			          server->focused_view->x + pixelsToMoveWindows,
			          server->focused_view->y);
			break;
		default:
			return false;
		}
	return true;
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	// This event is raised when a modifier key, such as shift
	// or alt, is pressed. We simply communicate this to the
	// client.
	struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	// A seat can only have one keyboard, but this is a
	// limitation of the Wayland protocol - not wlroots. We
	// assign all connected keyboards to the same seat. You can
	// swap out the underlying wlr_keyboard like this and
	// wlr_seat handles this transparently.
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	// Send modifiers to the client.
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
	                                   &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	// This event is raised when a key is pressed or released.
	struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct tinywl_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	// Translate libinput keycode -> xkbcommon
	uint32_t keycode = event->keycode + 8;
	// Get a list of keysyms based on the keymap for this
	// keyboard
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		if ((modifiers & WLR_MODIFIER_ALT) && keycode != 64) {
			// if the alt modifier is being held and you
			// press a key that is not alt then we need
			// to ignore the next keyrelease
			server->ignoreNextAltRelease = true;
		}
		switch (modifiers) {
		case WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL:
			// In wlroots we must handle switching
			// virtual terminals ourselves
			for (unsigned int _ = 0; _ < 12; _++) {
				if (syms[nsyms - 1] == (XKB_KEY_XF86Switch_VT_1 + _)) {
					wlr_session_change_vt(
					        wlr_backend_get_session(server->backend), (_ + 1));
					return;
				}
			}
			if (handle_altctrl_binding(server, syms[nsyms - 1]))
				// If we fail to process it and
				// switch TTY attempt to process it
				// as a compositor keybinding
				return;
			break;
		case WLR_MODIFIER_ALT:
			// If alt is held down, we attempt to
			// process it as a compositor keybinding.
			if (handle_altbinding(server, syms[nsyms - 1]))
				// If we succeeded in processing it
				// the we should not pass the event
				// to the client
				return;
			break;
		case WLR_MODIFIER_LOGO:
			// If the logo key is held down, do the same
			if (handle_logobinding(server, syms[nsyms - 1]))
				return;
			break;
		case WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT:
			// and the same for alt and shift
			if (handle_altshift_biding(server, syms[nsyms - 1]))
				return;
			break;
		default:
			if (!modifiers && syms[nsyms - 1] == XKB_KEY_Escape && message_hide(server))
				// If we press escape and we are able to
				// hide a popup message then we do not need
				// to process the keypress as a compositor
				// keybinding
				return;
		}
	} else if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		// if the alt modifier is being held and you release
		// it
		if (((modifiers == WLR_MODIFIER_LOGO && syms[0] == XKB_KEY_Super_L) ||
		     (modifiers == WLR_MODIFIER_ALT && syms[0] == XKB_KEY_Alt_L)) &&
		    server->message.type == TinywlMsgClientsList) {
			// If we are displaying the clients list and
			// we release alt or super (meaning we have
			// picked our client) then focus it and hide
			// the clients list
			message_hide(server);
			focus_view(server->focused_view);
		}
		if (modifiers == WLR_MODIFIER_ALT && syms[0] == XKB_KEY_Alt_L) {
			if (!server->ignoreNextAltRelease) {
				if (server->message.type != TinywlMsgHello) {
					time_t t = time(0);

					char message[800] = "⏳️ ";
					strcat(message, ctime(&t));
					strcat(message,
					       "🔍 Type to search (TODO)\n"
					       "?  Use the alt + h keybinding for more help");

					message_print(server, message, TinywlMsgHello);
				} else {
					message_hide(server);
				}
			}
			server->ignoreNextAltRelease = false;
			return;
		}
	}

	// Otherwise, we pass it along to the client.
	wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, +event->state);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	// This event is raised by the keyboard base
	// wlr_input_device to signal the destruction of the
	// wlr_keyboard. It will no longer receive events and should
	// be destroyed.
	struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void server_new_keyboard(struct tinywl_server *server, struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct tinywl_keyboard *keyboard = calloc(1, sizeof(struct tinywl_keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	// We need to prepare an XKB keymap and assign it to the
	// keyboard.
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(
	        context, &(struct xkb_rule_names){.layout = keyboardLayout, .options = ""},
	        XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	// Here we set up listeners for keyboard events.
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	// And add the keyboard to our list of keyboards
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void createVirtualKeyboard(struct wl_listener *listener, void *data) {
	struct tinywl_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *keyboard = data;
	server_new_keyboard(server, keyboard);
}

///////////////////////
// MANAGING POINTERS //
///////////////////////

static void server_new_pointer(struct tinywl_server *server, struct wlr_input_device *device) {
	// We don't do anything special with pointers. All of our
	// pointer handling is proxied through wlr_cursor.
	struct wlr_pointer *pointer = wlr_pointer_from_input_device(device);
	if (wlr_input_device_is_libinput(&pointer->base)) {
		struct libinput_device *libinput_device =
		        (struct libinput_device *) wlr_libinput_get_device_handle(&pointer->base);

		if (libinput_device_config_scroll_has_natural_scroll(libinput_device))
			libinput_device_config_scroll_set_natural_scroll_enabled(libinput_device,
			                                                         0);

		// If the device is a trackpad
		if (libinput_device_config_tap_get_finger_count(libinput_device)) {
			libinput_device_config_tap_set_enabled(libinput_device, 1);
			libinput_device_config_tap_set_drag_enabled(libinput_device, 1);
			libinput_device_config_tap_set_drag_lock_enabled(libinput_device, 1);

			libinput_device_config_tap_set_button_map(libinput_device,
			                                          LIBINPUT_CONFIG_TAP_MAP_LRM);
			if (libinput_device_config_scroll_has_natural_scroll(libinput_device))
				libinput_device_config_scroll_set_natural_scroll_enabled(
				        libinput_device, 1);
		}

		if (libinput_device_config_dwt_is_available(libinput_device))
			libinput_device_config_dwt_set_enabled(libinput_device, 1);

		if (libinput_device_config_left_handed_is_available(libinput_device))
			libinput_device_config_left_handed_set(libinput_device, 0);

		if (libinput_device_config_middle_emulation_is_available(libinput_device))
			libinput_device_config_middle_emulation_set_enabled(libinput_device, 0);

		if (libinput_device_config_scroll_get_methods(libinput_device) !=
		    LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method(libinput_device,
			                                         LIBINPUT_CONFIG_SCROLL_2FG);

		if (libinput_device_config_click_get_methods(libinput_device) !=
		    LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method(
			        libinput_device, LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);

		if (libinput_device_config_send_events_get_modes(libinput_device))
			libinput_device_config_send_events_set_mode(
			        libinput_device, LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);

		if (libinput_device_config_accel_is_available(libinput_device)) {
			libinput_device_config_accel_set_profile(
			        libinput_device, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
			libinput_device_config_accel_set_speed(libinput_device, 0.75);
		}
	}
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct tinywl_server *server = wl_container_of(listener, server, request_cursor);
	// This event is raised by the seat when a client provides a
	// cursor image
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;

	// This can be sent by any client, so we check to make sure
	// this one is actually has pointer focus first.
	if (focused_client == event->seat_client) {
		// Once we've vetted the client, we can tell the
		// cursor to use the provided surface as the cursor
		// image. It will set the hardware cursor on the
		// output that it's currently on and continue to do
		// so as the cursor moves between outputs.
		wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x,
		                       event->hotspot_y);
	}
}

static void process_cursor_resize(struct tinywl_server *server, uint32_t time) {
	// Resizing the grabbed view can be a little bit
	// complicated, because we could be resizing from any corner
	// or edge. This not only resize the view on one or two
	// axes, but can also move the view if you resize from the
	// top or left edges (or top-left corner).
	//
	// Note that I took some shortcuts here. In a more
	// fleshed-out compositor, you'd wait for the client to
	// prepare a buffer at the new size, then commit any
	// movement that was prepared.
	struct tinywl_view *view = server->grabbed_view;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
	move_view(view, new_left - geo_box.x, new_top - geo_box.y);
	resize_view(view, new_right - new_left, new_bottom - new_top);
}

static void update_drag_icon_position(struct tinywl_server *server) {
	struct wlr_drag_icon *icon;
	if (server->seat->drag && (icon = server->seat->drag->icon))
		wlr_scene_node_set_position(icon->data, server->cursor->x + icon->surface->sx,
		                            server->cursor->y + icon->surface->sy);
}

static void process_cursor_motion(struct tinywl_server *server, uint32_t time) {
	// If the mode is non-passthrough, then process the motion.
	if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
		move_view(server->grabbed_view, server->cursor->x - server->grab_x,
		          server->cursor->y - server->grab_y);
		return;
	} else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	// Update drag icons position if any
	update_drag_icon_position(server);

	// Otherwise, find the view under the pointer and send the
	// event along. This code is seperated into a function
	// because it can be called when for example the
	// pointerfocused client in unmapped
	process_motion(server, time);
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	// This event is forwarded by the cursor when a pointer
	// emits a _relative_ pointer motion event (i.e. a delta)
	struct tinywl_server *server = wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	// The cursor doesn't move unless we tell it to. The cursor
	// automatically handles constraining the motion to the
	// output layout, as well as any special configuration
	// applied for the specific input device which generated the
	// event. You can pass NULL for the device if you want to
	// move the cursor around without any input.
	wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);

	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	// This event is forwarded by the cursor when a pointer
	// emits an _absolute_ motion event, from 0..1 on each axis.
	// This happens, for example, when wlroots is running under
	// a Wayland client rather than KMS+DRM, and you move the
	// mouse over the client. You could enter the client from
	// any edge, so we have to warp the mouse there. There is
	// also some hardware which emits these events.
	struct tinywl_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	// This event is forwarded by the cursor when a pointer
	// emits a button event.
	struct tinywl_server *server = wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct tinywl_view *view =
	        desktop_view_at(server, server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_PRESSED) {
		focus_view(view);

		// If you are pressing alt while you press mouse
		// buttons
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
		if (keyboard && wlr_keyboard_get_modifiers(keyboard) == WLR_MODIFIER_ALT) {
			// Then we can start to move/resize the
			// client the cursor is on
			server->ignoreNextAltRelease = true;
			switch (event->button) {
			case BTN_LEFT:
				begin_interactive(view, TINYWL_CURSOR_MOVE, 0);
				return;
			case BTN_RIGHT:
				begin_interactive(view, TINYWL_CURSOR_RESIZE,
				                  WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
				return;
			}
		}
	}

	wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

	if (event->state == WLR_BUTTON_RELEASED) {
		// If you released any buttons, we exit interactive
		// move/resize mode.
		reset_cursor_mode(server);
	} else if (view) {
		// we set the grabbed view so that when you are
		// holding your cursor and you move it off the
		// client the client still reveives mouse events
		server->cursor_mode = TINYWL_CURSOR_PRESSED;
		server->grab_x = server->cursor->x - sx;
		server->grab_y = server->cursor->y - sy;
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	// This event is forwarded by the cursor when a pointer
	// emits an axis event, for example when you move the scroll
	// wheel.
	struct tinywl_server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	// Notify the client with pointer focus of the axis event.
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
	                             event->delta, event->delta_discrete, event->source);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
	// This event is forwarded by the cursor when a pointer
	// emits an frame event. Frame events are sent after regular
	// pointer events to group multiple events together. For
	// instance, two axis events may happen at the same time, in
	// which case a frame event won't be sent in between.
	struct tinywl_server *server = wl_container_of(listener, server, cursor_frame);
	// Notify the client with pointer focus of the frame event.
	wlr_seat_pointer_notify_frame(server->seat);
}

/////////////////////////
// EVENTS FOR MONITORS //
/////////////////////////

static void output_frame(struct wl_listener *listener, void *data) {
	// This function is called every time an output is ready to
	// display a frame, generally at the output's refresh rate
	// (e.g. 60Hz).
	struct tinywl_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;

	struct wlr_scene_output *scene_output =
	        wlr_scene_get_scene_output(scene, output->wlr_output);

	// Render the scene if needed and commit the output
	wlr_scene_output_commit(scene_output);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	struct tinywl_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	// This event is raised by the backend when a new output
	// (aka a display or monitor) becomes available.
	struct tinywl_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	// Configures the output created by the backend to use our
	// allocator and our renderer. Must be done once, before
	// commiting the output
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	// Some backends don't have modes. DRM+KMS does, and we need
	// to set a mode before we can use the output. The mode is a
	// tuple of (width, height, refresh rate), and each monitor
	// supports only a specific set of modes. We just pick the
	// monitor's preferred mode, a more sophisticated compositor
	// would let the user configure it.
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	// Allocates and configures our state for this output
	struct tinywl_output *output = calloc(1, sizeof(struct tinywl_output));
	output->wlr_output = wlr_output;
	output->server = server;

	// Sets up a listener for the frame notify event.
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	// Sets up a listener for the destroy notify event.
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	// Adds this to the output layout. The add_auto function
	// arranges outputs from left-to-right in the order they
	// appear. A more sophisticated compositor would let the
	// user configure the arrangement of outputs in the layout.
	//
	// The output layout utility automatically adds a wl_output
	// global to the display, which Wayland clients can see to
	// find out information about the output (such as DPI, scale
	// factor, manufacturer, etc).
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}

//////////////////////////
// OTHER WLROOTS EVENTS //
//////////////////////////

static void server_new_input(struct wl_listener *listener, void *data) {
	// This event is raised by the backend when a new input
	// device becomes available.
	struct tinywl_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}

	// We need to let the wlr_seat know what our capabilities
	// are, which is communiciated to the client. In TinyWL we
	// always have a cursor, even if there are no pointer
	// devices, so we always include that capability.
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	// This event is raised by the seat when a client wants to
	// set the selection, usually when the user copies
	// something. wlroots allows compositors to ignore such
	// requests if they so choose, but in tinywl we always honor
	struct wlr_seat_request_set_selection_event *event = data;
	struct tinywl_server *server = wl_container_of(listener, server, request_set_selection);
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void destroyDragIcon(struct wl_listener *listener, void *data) {
	struct wlr_drag_icon *icon = data;
	wlr_scene_node_destroy(icon->data);
}

static void request_start_drag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;
	struct tinywl_server *server = wl_container_of(listener, server, request_start_drag);

	if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin, event->serial))
		wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void seat_start_drag(struct wl_listener *listener, void *data) {
	struct wlr_drag *drag = data;
	struct tinywl_server *server = wl_container_of(listener, server, start_drag);

	if (!drag->icon)
		return;

	drag->icon->data = wlr_scene_subsurface_tree_create(server->layers[TinywlLyrDragIcon],
	                                                    drag->icon->surface);
	update_drag_icon_position(server);
	static struct wl_listener drag_icon_destroy = {.notify = destroyDragIcon};
	wl_signal_add(&drag->icon->events.destroy, &drag_icon_destroy);
}

///////////////
// MAIN CODE //
///////////////

int main(int argc, char *argv[]) {
	if (argc > 1) {
		if (!strcmp(argv[1], "about")) {
			printf("A fork of tinywl with some "
			       "additional features based on "
			       "wlroots "
			       "0.16\n");
			return EXIT_SUCCESS;
		}
		fprintf(stderr,
		        "%s is not a valid option, use the `about` "
		        "command to see some info\n",
		        argv[1]);
		return EXIT_FAILURE;
	}

	struct tinywl_server server;

	// This is a wlrotos utility for managing logs
	wlr_log_init(WLR_DEBUG, NULL);

	// The Wayland display is managed by libwayland. It handles
	// accepting clients from the Unix socket, manging Wayland
	// globals, and so on.
	server.wl_display = wl_display_create();

	// The backend is a wlroots feature which abstracts the
	// underlying input and output hardware. The autocreate
	// option will choose the most suitable backend based on the
	// current environment, such as opening an X11 window if an
	// X11 server is running.
	server.backend = wlr_backend_autocreate(server.wl_display);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_backend");
		return EXIT_FAILURE;
	}

	// Autocreates a renderer, either Pixman, GLES2 or Vulkan
	// for us. The user can also specify a renderer using the
	// WLR_RENDERER env var. The renderer is responsible for
	// defining the various pixel formats it supports for shared
	// memory, this configures that for clients.
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_renderer");
		return EXIT_FAILURE;
	}
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	// Autocreates an allocator for us. The allocator is the
	// bridge between the renderer and the backend. It handles
	// the buffer creation, allowing wlroots to render onto the
	// screen
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (server.allocator == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_allocator");
		return 1;
	}

	// This creates some hands-off wlroots interfaces:
	//  - The compositor is necessary for clients to allocate
	//  surfaces
	//  - The subcompositor allows to assign the role of
	//  subsurfaces to surfaces
	//  - The screencopy manager allows for a protocol to
	//  copy/record:
	//     - a portion of a screen
	//     - a client
	//     - every screen at ounce into one image
	//  - The data device manager handles the clipboard
	// Each of these wlroots interfaces has room for you to dig
	// your fingers in and play with their behavior if you want.
	// Note that the clients cannot set the selection directly
	// without compositor approval, see the handling of the
	// request_set_selection event below.
	wlr_compositor_create(server.wl_display, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_screencopy_manager_v1_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

	// Creates an output layout, which a wlroots utility for
	// working with an arrangement of screens in a physical
	// layout.
	server.output_layout = wlr_output_layout_create();

	// Configure a listener to be notified when new outputs are
	// available on the backend.
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	// Create a scene graph. This is a wlroots abstraction that
	// handles all rendering and damage tracking. All the
	// compositor author needs to do is add things that should
	// be rendered to the scene graph at the proper positions
	// and then call wlr_scene_output_commit() to render a frame
	// if necessary.
	server.scene = wlr_scene_create();
	for (int _ = 0; _ < NUM_TINYWL_LAYERS; _++)
		server.layers[_] = wlr_scene_tree_create(&server.scene->tree);
	wlr_scene_attach_output_layout(server.scene, server.output_layout);

	// Set up xdg-shell version 3. The xdg-shell is a Wayland
	// protocol which is used for application clients.
	wl_list_init(&server.views);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

	// Setup decoration to be either server or client side
	if (disableClientSideDecorations) {
		server.xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(server.wl_display);
		wlr_server_decoration_manager_set_default_mode(
		        wlr_server_decoration_manager_create(server.wl_display),
		        WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

		static struct wl_listener new_xdg_decoration = {.notify =
		                                                        createClientSideDecoration};
		wl_signal_add(&server.xdg_decoration_mgr->events.new_toplevel_decoration,
		              &new_xdg_decoration);
	}

	// Creates a cursor, which is a wlroots utility for tracking
	// the cursor image shown on screen.
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	// Creates an xcursor manager, another wlroots utility which
	// loads up Xcursor themes to source cursor images from and
	// makes sure that cursor images are available at all scale
	// factors on the screen (necessary for HiDPI support). We
	// add a cursor theme at scale factor 1 to begin with.
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, cursorSize);
	wlr_xcursor_manager_load(server.cursor_mgr, 1);

	// wlr_cursor *only* displays an image on screen. It does
	// not move around when the pointer moves. However, we can
	// attach input devices to it, and it will generate
	// aggregate events for all of them. In these events, we can
	// choose how we want to process them, forwarding them to
	// clients and moving the cursor around. More comments are
	// also sprinkled throughout the notify functions above.
	server.cursor_mode = TINYWL_CURSOR_PASSTHROUGH;

	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);

	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);

	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);

	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);

	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	// Configures a seat, which is a single "seat" at which a
	// user sits and operates the computer. This conceptually
	// includes up to one keyboard, pointer, touch, and drawing
	// tablet device. We also rig up a listener to let us know
	// when new input devices are available on the backend.
	wl_list_init(&server.keyboards);

	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);

	struct wlr_virtual_keyboard_manager_v1 *virtualKeyboardMgr =
	        wlr_virtual_keyboard_manager_v1_create(server.wl_display);
	server.new_virtual_keyboard.notify = createVirtualKeyboard;
	wl_signal_add(&virtualKeyboardMgr->events.new_virtual_keyboard,
	              &server.new_virtual_keyboard);

	server.seat = wlr_seat_create(server.wl_display, "seat0");

	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);

	server.request_start_drag.notify = request_start_drag;
	wl_signal_add(&server.seat->events.request_start_drag, &server.request_start_drag);

	server.start_drag.notify = seat_start_drag;
	wl_signal_add(&server.seat->events.start_drag, &server.start_drag);

	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

	// Add a Unix socket to the Wayland display.
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	// Start the backend. This will enumerate outputs and
	// inputs, become the DRM master, etc
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	// Set the WAYLAND_DISPLAY environment variable to our
	// socket
	setenv("WAYLAND_DISPLAY", socket, true);

	// Set some variables because c initialises it to a random
	// value
	server.ignoreNextAltRelease = false;
	server.focused_view = NULL;
	server.message.type = TinywlMsgNone;

	// Run the Wayland event loop. This does not return until
	// you exit the compositor. Starting the backend rigged up
	// all of the necessary event loop configuration to listen
	// to libinput events, DRM events, generate frame events at
	// the refresh rate, and so on.
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(server.wl_display);

	// Once wl_display_run returns, we shut down the server.
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);

	// Exit
	return EXIT_SUCCESS;
}
