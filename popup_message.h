#include "cairo.h"
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

////////////////
// CAIRO CODE //
////////////////

void rounded_rectangle(cairo_t *cr, int x, int y, int w, int h, int r, float bg_color[4]) {
	const float pi = 3.141592653589;
	if (r > (w / 2))
		r = w / 2;
	if (r > (h / 2))
		r = h / 2;
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + r, y + r, r, pi, pi * 1.5);
	cairo_arc(cr, x + w - r, y + r, r, pi * 1.5, pi * 2);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, pi / 2);
	cairo_arc(cr, x + r, y + h - r, r, pi / 2, pi);
	cairo_set_source_rgba(cr, bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
	cairo_fill(cr);
	cairo_close_path(cr);
}

////////////////
// PANGO CODE //
////////////////

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font, const char *text, double scale) {
	// Setup attributes
	PangoAttrList *attrs = pango_attr_list_new();
	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));

	// Setup layout
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_single_paragraph_mode(layout, false);
	pango_layout_set_attributes(layout, attrs);

	// Do final setting up
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);

	// Cleanup variables
	pango_font_description_free(desc);
	pango_attr_list_unref(attrs);

	return layout;
}

void get_text_size(cairo_t *cairo, const char *text, double scale, const char *font, int *width,
                   int *height, int *baseline) {
	// Define layout
	PangoLayout *layout = get_pango_layout(cairo, font, text, scale);
	pango_cairo_update_layout(cairo, layout);

	// Get variable values
	pango_layout_get_pixel_size(layout, width, height);
	if (baseline) {
		*baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;
	}

	// Cleanup
	g_object_unref(layout);
}

void pango_print(cairo_t *cairo, const char *font, double scale, const char *text) {
	PangoLayout *layout = get_pango_layout(cairo, font, text, scale);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_get_font_options(cairo, fo);
	pango_cairo_context_set_font_options(pango_layout_get_context(layout), fo);
	cairo_font_options_destroy(fo);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);
}

//////////////////
// WLROOTS CODE //
//////////////////

static void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct text_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool text_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
                                              void **data, uint32_t *format, size_t *stride) {
	struct text_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	if (data != NULL)
		*data = (void *) buffer->data;
	if (format != NULL)
		*format = buffer->format;
	if (stride != NULL)
		*stride = buffer->stride;
	return true;
}

static void text_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl text_buffer_impl = {
        .destroy = text_buffer_destroy,
        .begin_data_ptr_access = text_buffer_begin_data_ptr_access,
        .end_data_ptr_access = text_buffer_end_data_ptr_access,
};

static struct text_buffer *text_buffer_create(uint32_t width, uint32_t height, uint32_t stride) {
	struct text_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer)
		return NULL;

	wlr_buffer_init(&buffer->base, &text_buffer_impl, width, height);
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	buffer->data = malloc(buffer->stride * height);
	if (buffer->data == NULL) {
		free(buffer);
		return NULL;
	}

	return buffer;
}

cairo_subpixel_order_t to_cairo_subpixel_order(const enum wl_output_subpixel subpixel) {
	switch (subpixel) {
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_RGB;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_BGR;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_VRGB;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_VBGR;
	default:
		return CAIRO_SUBPIXEL_ORDER_DEFAULT;
	}
	return CAIRO_SUBPIXEL_ORDER_DEFAULT;
}

struct text_buffer *create_message_texture(const char *string, struct tinywl_output *output) {
	// Create a cairo_t
	cairo_surface_t *dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	if (dummy_surface == NULL)
		// This occurs when we are fuzzing. In that case, do nothing
		return NULL;
	cairo_t *c = cairo_create(dummy_surface);

	// Set font options
	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(
	        fo, to_cairo_subpixel_order(output->wlr_output->subpixel));
	cairo_set_font_options(c, fo);

	// Set width and height variables
	int width, height;
	get_text_size(c, string, output->wlr_output->scale, font_description, &width, &height,
	              NULL);
	width += 2 * text_horizontal_padding;
	height += 2 * text_vertical_padding;
	cairo_surface_destroy(dummy_surface);
	cairo_destroy(c);

	// Create a surface for text
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);

	// Draw rounded background
	rounded_rectangle(cairo, 0, 0, width, height, rounding_radius, message_bg_color);

	// Draw text
	cairo_set_source_rgba(cairo, message_fg_color[0], message_fg_color[1], message_fg_color[2],
	                      message_fg_color[3]);
	cairo_stroke(cairo);
	cairo_move_to(cairo, text_horizontal_padding, text_vertical_padding);
	pango_print(cairo, font_description, output->wlr_output->scale, string);
	cairo_surface_flush(surface);

	// Draw cairo surface to wlroots buffer
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct text_buffer *buf = text_buffer_create(width, height, stride);
	void *data_ptr;
	if (!wlr_buffer_begin_data_ptr_access(&buf->base, WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
	                                      &data_ptr, NULL, NULL)) {
		wlr_log(WLR_ERROR, "Failed to get pointer access to message buffer");
		return NULL;
	}
	unsigned char *data = cairo_image_surface_get_data(surface);
	memcpy(data_ptr, data, stride * height);
	wlr_buffer_end_data_ptr_access(&buf->base);

	// Cleanup cairo
	cairo_surface_destroy(surface);
	cairo_destroy(cairo);

	return buf;
}

bool message_hide(struct tinywl_server *server) {
	if (server->message->message && server->message->type != TinywlMsgNone) {
		wlr_scene_buffer_set_buffer(server->message->message, NULL);
		server->message->type = TinywlMsgNone;
		return true;
	}
	return false;
}

void message_print(struct tinywl_server *server, const char *text, enum tinywl_message_type type) {
	struct tinywl_output *output = wl_container_of(server->outputs.next, output, link);
	message_hide(server);

	server->message->buffer = create_message_texture(text, output);
	if (!server->message->buffer) {
		wlr_log(WLR_ERROR, "Could not create message texture");
		return;
	}
	server->message->type = type;

	double scale = output->wlr_output->scale;
	int width = server->message->buffer->base.width / scale;
	int height = server->message->buffer->base.height / scale;
	int x = (output->wlr_output->width - width) / 2;
	int y = (output->wlr_output->height - height) / 2;

	server->message->message =
	        wlr_scene_buffer_create(server->layers[LyrMessage], &server->message->buffer->base);
	wlr_scene_node_raise_to_top(&server->message->message->node);
	wlr_scene_node_set_enabled(&server->message->message->node, true);
	struct wlr_box output_box;
	wlr_output_layout_get_box(output->server->output_layout, output->wlr_output, &output_box);
	wlr_scene_buffer_set_dest_size(server->message->message, width, height);
	wlr_scene_node_set_position(&server->message->message->node, x + output_box.x,
	                            y + output_box.y);
}
