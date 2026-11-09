/* snihost.c - StatusNotifierItem (SNI) Watcher & Host for Window Maker Dock
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 2026 Window Maker Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "wconfig.h"

#ifdef USE_SNI

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <dbus/dbus.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wraster.h>

#include "WindowMaker.h"
#include "screen.h"
#include "dock.h"
#include "appicon.h"
#include "icon.h"
#include "stacking.h"
#include "misc.h"
#include "menu.h"
#include "pixmap.h"
#include "framewin.h"
#include "snihost.h"

#define SNI_WATCHER_BUS_NAME_KDE "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_BUS_NAME_FDO "org.freedesktop.StatusNotifierWatcher"
#define SNI_WATCHER_OBJ_PATH     "/StatusNotifierWatcher"
#define SNI_WATCHER_IFACE_KDE    "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_IFACE_FDO    "org.freedesktop.StatusNotifierWatcher"

#define SNI_ITEM_IFACE_KDE       "org.kde.StatusNotifierItem"
#define SNI_ITEM_IFACE_FDO       "org.freedesktop.StatusNotifierItem"
#define SNI_ITEM_DEFAULT_PATH    "/StatusNotifierItem"

#define DBUS_PROPS_IFACE         "org.freedesktop.DBus.Properties"
#define DBUS_INTROSPECT_IFACE    "org.freedesktop.DBus.Introspectable"

typedef struct SNIItem {
	char *service_name;
	char *sender_unique;
	char *obj_path;
	char *full_id;
	char *id;
	char *title;
	char *status;
	char *icon_name;
	char *attention_icon_name;
	char *icon_theme_path;
	char *tooltip_title;
	char *tooltip_body;
	char *balloon_text;
	char *resolved_icon_name;
	char *resolved_icon_path;
	char *menu_path;
	WMenu *menu;
	WAppIcon *aicon;
	DBusPendingCall *pending;
	DBusPendingCall *layout_pending;
	struct SNIItem *next;
} SNIItem;

typedef struct SNIHostState {
	WScreen *scr;
	DBusConnection *conn;
	WMHandlerID input_handler;
	char *host_bus_name;
	SNIItem *items;
	Bool pointer_grabbed;
	Bool keyboard_grabbed;
	Bool in_dispatch;
} SNIHostState;

/* The tray is a per-session singleton: only screen 0 binds the watcher and
 * runs wSnihostShutdown(); other screens must not create a second host. */
static SNIHostState *sni_state = NULL;

static const char *sni_watcher_introspect_xml =
	"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-Bus Object Introspection 1.0//EN\"\n"
	"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
	"<node>\n"
	"  <interface name=\"org.kde.StatusNotifierWatcher\">\n"
	"    <method name=\"RegisterStatusNotifierItem\">\n"
	"      <arg name=\"service\" type=\"s\" direction=\"in\"/>\n"
	"    </method>\n"
	"    <method name=\"RegisterStatusNotifierHost\">\n"
	"      <arg name=\"service\" type=\"s\" direction=\"in\"/>\n"
	"    </method>\n"
	"    <property name=\"RegisteredStatusNotifierItems\" type=\"as\" access=\"read\"/>\n"
	"    <property name=\"IsStatusNotifierHostRegistered\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"ProtocolVersion\" type=\"i\" access=\"read\"/>\n"
	"    <signal name=\"StatusNotifierItemRegistered\">\n"
	"      <arg type=\"s\"/>\n"
	"    </signal>\n"
	"    <signal name=\"StatusNotifierItemUnregistered\">\n"
	"      <arg type=\"s\"/>\n"
	"    </signal>\n"
	"    <signal name=\"StatusNotifierHostRegistered\"/>\n"
	"    <signal name=\"StatusNotifierHostUnregistered\"/>\n"
	"  </interface>\n"
	"</node>\n";

static void sni_request_properties(SNIItem *item);
static void sni_request_dbusmenu_sync(SNIItem *item);

static void sni_replace_str(char **dst, const char *src)
{
	if (*dst) {
		wfree(*dst);
		*dst = NULL;
	}
	if (src && src[0] != '\0')
		*dst = wstrdup(src);
}

static char *sni_strip_html(const char *src)
{
	char *out;
	size_t i = 0, j = 0;
	int in_tag = 0;

	if (!src)
		return NULL;

	out = wmalloc(strlen(src) + 1);
	while (src[i] != '\0') {
		if (src[i] == '<') {
			if (strncasecmp(&src[i], "<br>", 4) == 0 ||
			    strncasecmp(&src[i], "<br/>", 5) == 0 ||
			    strncasecmp(&src[i], "<br />", 6) == 0) {
				out[j++] = '\n';
			}
			in_tag = 1;
			i++;
			continue;
		}
		if (src[i] == '>') {
			in_tag = 0;
			i++;
			continue;
		}
		if (!in_tag)
			out[j++] = src[i];
		i++;
	}
	out[j] = '\0';
	return out;
}

static void sni_update_balloon_text(SNIItem *item)
{
	char *clean_body = NULL;
	const char *primary = NULL;

	if (item->balloon_text) {
		wfree(item->balloon_text);
		item->balloon_text = NULL;
	}

	if (item->tooltip_title && item->tooltip_title[0] != '\0')
		primary = item->tooltip_title;
	else if (item->title && item->title[0] != '\0')
		primary = item->title;
	else if (item->id && item->id[0] != '\0')
		primary = item->id;
	else
		primary = item->service_name;

	if (item->tooltip_body && item->tooltip_body[0] != '\0')
		clean_body = sni_strip_html(item->tooltip_body);

	if (primary && clean_body && clean_body[0] != '\0' && strcmp(primary, clean_body) != 0) {
		size_t len = strlen(primary) + strlen(clean_body) + 2;
		item->balloon_text = wmalloc(len);
		snprintf(item->balloon_text, len, "%s\n%s", primary, clean_body);
	} else if (primary) {
		item->balloon_text = wstrdup(primary);
	}

	if (clean_body)
		wfree(clean_body);
}

static RImage *sni_scale_for_dock(RImage *img)
{
	int max_size = wPreferences.icon_size - 4;
	int target_w, target_h;
	RImage *scaled;

	if (!img)
		return NULL;

	if (max_size < 16)
		max_size = 16;

	if ((int)img->width == max_size && (int)img->height == max_size)
		return img;

	/* Scale down oversized icons, or scale up tiny (<= 24px) tray icons
	 * so they remain legible inside a 64x64 Window Maker dock tile. */
	if ((int)img->width > max_size || (int)img->height > max_size ||
	    ((int)img->width <= 32 && (int)img->height <= 32 && max_size >= 44)) {
		int target_box = ((int)img->width <= 32 && max_size >= 48) ? (wPreferences.icon_size * 3 / 4) : max_size;

		if (img->width >= img->height) {
			target_w = target_box;
			target_h = (int)(img->height * target_box / img->width);
		} else {
			target_h = target_box;
			target_w = (int)(img->width * target_box / img->height);
		}
		if (target_w < 1)
			target_w = 1;
		if (target_h < 1)
			target_h = 1;

		scaled = RSmoothScaleImage(img, target_w, target_h);
		if (scaled) {
			RReleaseImage(img);
			return scaled;
		}
	}

	return img;
}

/*
 * Parse SNI IconPixmap signature a(iiay):
 * Array of structs (int32 width, int32 height, array<byte> argb32_be).
 */
static RImage *sni_extract_icon_pixmap(DBusMessageIter *array_iter)
{
	DBusMessageIter elem_iter;
	int target_size = wPreferences.icon_size - 4;
	int best_w = 0, best_h = 0;
	const unsigned char *best_bytes = NULL;
	int best_score = 1000000;
	RImage *img;
	int i, total_pixels;

	if (dbus_message_iter_get_arg_type(array_iter) != DBUS_TYPE_ARRAY)
		return NULL;

	dbus_message_iter_recurse(array_iter, &elem_iter);
	while (dbus_message_iter_get_arg_type(&elem_iter) == DBUS_TYPE_STRUCT) {
		DBusMessageIter struct_iter, byte_iter;
		dbus_int32_t w = 0, h = 0;
		const unsigned char *bytes = NULL;
		int n_bytes = 0;

		dbus_message_iter_recurse(&elem_iter, &struct_iter);
		if (dbus_message_iter_get_arg_type(&struct_iter) != DBUS_TYPE_INT32)
			goto next_elem;
		dbus_message_iter_get_basic(&struct_iter, &w);

		if (!dbus_message_iter_next(&struct_iter) ||
		    dbus_message_iter_get_arg_type(&struct_iter) != DBUS_TYPE_INT32)
			goto next_elem;
		dbus_message_iter_get_basic(&struct_iter, &h);

		if (!dbus_message_iter_next(&struct_iter) ||
		    dbus_message_iter_get_arg_type(&struct_iter) != DBUS_TYPE_ARRAY ||
		    dbus_message_iter_get_element_type(&struct_iter) != DBUS_TYPE_BYTE)
			goto next_elem;

		dbus_message_iter_recurse(&struct_iter, &byte_iter);
		dbus_message_iter_get_fixed_array(&byte_iter, &bytes, &n_bytes);

		if (w > 0 && h > 0 && w <= 512 && h <= 512 && bytes != NULL && n_bytes >= w * h * 4) {
			int diff = abs((int)w - target_size);
			/* Prefer icons >= 24px over tiny 16px icons */
			if (w < 24)
				diff += 64;
			if (diff < best_score) {
				best_score = diff;
				best_w = w;
				best_h = h;
				best_bytes = bytes;
			}
		}

next_elem:
		dbus_message_iter_next(&elem_iter);
	}

	if (!best_bytes || best_w <= 0 || best_h <= 0)
		return NULL;

	img = RCreateImage(best_w, best_h, True);
	if (!img)
		return NULL;

	total_pixels = best_w * best_h;
	for (i = 0; i < total_pixels; i++) {
		unsigned char a = best_bytes[i * 4 + 0];
		unsigned char r = best_bytes[i * 4 + 1];
		unsigned char g = best_bytes[i * 4 + 2];
		unsigned char b = best_bytes[i * 4 + 3];

		img->data[i * 4 + 0] = r;
		img->data[i * 4 + 1] = g;
		img->data[i * 4 + 2] = b;
		img->data[i * 4 + 3] = a;
	}

	return sni_scale_for_dock(img);
}

static RImage *sni_try_load_icon_file(WScreen *scr, const char *path)
{
	RImage *img;

	if (!path || path[0] == '\0' || access(path, R_OK) != 0)
		return NULL;

	img = RLoadImage(scr->rcontext, path, 0);
	if (!img)
		return NULL;

	return sni_scale_for_dock(img);
}

static void sni_cache_resolved(SNIItem *item, const char *icon_name, const char *path)
{
	if (!item)
		return;
	sni_replace_str(&item->resolved_icon_name, icon_name);
	sni_replace_str(&item->resolved_icon_path, path);
}

static RImage *sni_resolve_icon_name(SNIItem *item, WScreen *scr, const char *icon_name,
                                     const char *theme_path)
{
	static const char *const sizes[] = { "48x48", "64x64", "32x32", "24x24", "22x22", "16x16" };
	/* "status" first: tray icons are almost always status-type */
	static const char *const cats[] = { "status", "apps", "panel", "devices", "actions" };
	static const char *const exts[] = { ".png", ".xpm", "" };
	char candidate[1024];
	char *found;
	RImage *img;
	size_t s, c, e;

	if (!icon_name || icon_name[0] == '\0')
		return NULL;

	/* Cache: the lookup below is dozens of access()/RLoadImage attempts, so
	 * when the icon name has not changed reuse the path found last time. */
	if (item && item->resolved_icon_name && item->resolved_icon_path &&
	    strcmp(item->resolved_icon_name, icon_name) == 0) {
		img = sni_try_load_icon_file(scr, item->resolved_icon_path);
		if (img)
			return img;
	}

	/* 1. Direct absolute path */
	if (icon_name[0] == '/') {
		img = sni_try_load_icon_file(scr, icon_name);
		if (img) {
			sni_cache_resolved(item, icon_name, icon_name);
			return img;
		}
	}

	/* 2. Custom IconThemePath provided by the SNI client */
	if (theme_path && theme_path[0] != '\0') {
		for (e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
			snprintf(candidate, sizeof(candidate), "%s/%s%s", theme_path, icon_name, exts[e]);
			img = sni_try_load_icon_file(scr, candidate);
			if (img) {
				sni_cache_resolved(item, icon_name, candidate);
				return img;
			}
		}
		for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
			for (c = 0; c < sizeof(cats) / sizeof(cats[0]); c++) {
				for (e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
					snprintf(candidate, sizeof(candidate), "%s/hicolor/%s/%s/%s%s",
					         theme_path, sizes[s], cats[c], icon_name, exts[e]);
					img = sni_try_load_icon_file(scr, candidate);
					if (img) {
						sni_cache_resolved(item, icon_name, candidate);
						return img;
					}
				}
			}
		}
	}

	/* 3. Window Maker configured icon_path */
	for (e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
		snprintf(candidate, sizeof(candidate), "%s%s", icon_name, exts[e]);
		found = FindImage(wPreferences.icon_path, candidate);
		if (found) {
			img = sni_try_load_icon_file(scr, found);
			if (img) {
				sni_cache_resolved(item, icon_name, found);
				wfree(found);
				return img;
			}
			wfree(found);
		}
	}

	/* 4. Standard freedesktop hicolor / pixmaps directories */
	for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		for (c = 0; c < sizeof(cats) / sizeof(cats[0]); c++) {
			for (e = 0; e < 2; e++) {
				snprintf(candidate, sizeof(candidate), "/usr/share/icons/hicolor/%s/%s/%s%s",
				         sizes[s], cats[c], icon_name, exts[e]);
				img = sni_try_load_icon_file(scr, candidate);
				if (img) {
					sni_cache_resolved(item, icon_name, candidate);
					return img;
				}
			}
		}
	}
	for (e = 0; e < 2; e++) {
		snprintf(candidate, sizeof(candidate), "/usr/share/pixmaps/%s%s", icon_name, exts[e]);
		img = sni_try_load_icon_file(scr, candidate);
		if (img) {
			sni_cache_resolved(item, icon_name, candidate);
			return img;
		}
	}

	return NULL;
}

static void sni_parse_tooltip(SNIItem *item, DBusMessageIter *variant_iter)
{
	int arg_type = dbus_message_iter_get_arg_type(variant_iter);

	if (arg_type == DBUS_TYPE_STRING) {
		const char *str = NULL;
		dbus_message_iter_get_basic(variant_iter, &str);
		sni_replace_str(&item->tooltip_title, str);
		sni_replace_str(&item->tooltip_body, NULL);
	} else if (arg_type == DBUS_TYPE_STRUCT) {
		/* ToolTip struct: (s a(iiay) s s) -> (icon_name, icon_pixmap, title, body) */
		DBusMessageIter struct_iter;
		const char *tt_title = NULL;
		const char *tt_body = NULL;

		dbus_message_iter_recurse(variant_iter, &struct_iter);
		/* skip icon_name (s) */
		if (!dbus_message_iter_next(&struct_iter))
			return;
		/* skip icon_pixmap (a(iiay)) */
		if (!dbus_message_iter_next(&struct_iter))
			return;
		if (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_STRING)
			dbus_message_iter_get_basic(&struct_iter, &tt_title);
		if (dbus_message_iter_next(&struct_iter) &&
		    dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_STRING)
			dbus_message_iter_get_basic(&struct_iter, &tt_body);

		sni_replace_str(&item->tooltip_title, tt_title);
		sni_replace_str(&item->tooltip_body, tt_body);
	}
}

static void sni_on_get_all_reply(DBusPendingCall *pending, void *user_data)
{
	SNIItem *item = (SNIItem *)user_data;
	DBusMessage *reply;
	DBusMessageIter root_iter, dict_iter;
	RImage *icon_pixmap_img = NULL;
	RImage *attn_pixmap_img = NULL;
	RImage *final_img = NULL;
	int needs_attention = 0;

	if (item->pending == pending) {
		dbus_pending_call_unref(item->pending);
		item->pending = NULL;
	}

	reply = dbus_pending_call_steal_reply(pending);
	if (!reply)
		return;

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
		dbus_message_unref(reply);
		return;
	}

	if (!dbus_message_iter_init(reply, &root_iter) ||
	    dbus_message_iter_get_arg_type(&root_iter) != DBUS_TYPE_ARRAY) {
		dbus_message_unref(reply);
		return;
	}

	dbus_message_iter_recurse(&root_iter, &dict_iter);
	while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry_iter, var_iter;
		const char *key = NULL;
		int vtype;

		dbus_message_iter_recurse(&dict_iter, &entry_iter);
		if (dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_STRING)
			goto next_entry;
		dbus_message_iter_get_basic(&entry_iter, &key);

		if (!dbus_message_iter_next(&entry_iter) ||
		    dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_VARIANT)
			goto next_entry;

		dbus_message_iter_recurse(&entry_iter, &var_iter);
		vtype = dbus_message_iter_get_arg_type(&var_iter);

		if (strcmp(key, "Id") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->id, val);
		} else if (strcmp(key, "Title") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->title, val);
		} else if (strcmp(key, "Status") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->status, val);
		} else if (strcmp(key, "IconName") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->icon_name, val);
		} else if (strcmp(key, "AttentionIconName") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->attention_icon_name, val);
		} else if (strcmp(key, "IconThemePath") == 0 && vtype == DBUS_TYPE_STRING) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->icon_theme_path, val);
		} else if (strcmp(key, "Menu") == 0 &&
		           (vtype == DBUS_TYPE_STRING || vtype == DBUS_TYPE_OBJECT_PATH)) {
			const char *val = NULL;
			dbus_message_iter_get_basic(&var_iter, &val);
			sni_replace_str(&item->menu_path, val);
		} else if (strcmp(key, "ToolTip") == 0) {
			sni_parse_tooltip(item, &var_iter);
		} else if (strcmp(key, "IconPixmap") == 0 && vtype == DBUS_TYPE_ARRAY) {
			if (icon_pixmap_img)
				RReleaseImage(icon_pixmap_img);
			icon_pixmap_img = sni_extract_icon_pixmap(&var_iter);
		} else if (strcmp(key, "AttentionIconPixmap") == 0 && vtype == DBUS_TYPE_ARRAY) {
			if (attn_pixmap_img)
				RReleaseImage(attn_pixmap_img);
			attn_pixmap_img = sni_extract_icon_pixmap(&var_iter);
		}

next_entry:
		dbus_message_iter_next(&dict_iter);
	}

	dbus_message_unref(reply);

	sni_update_balloon_text(item);

	if (!item->aicon || !sni_state || !sni_state->scr) {
		if (icon_pixmap_img)
			RReleaseImage(icon_pixmap_img);
		if (attn_pixmap_img)
			RReleaseImage(attn_pixmap_img);
		return;
	}

	if (item->id && item->id[0] != '\0') {
		if (item->aicon->wm_instance)
			wfree(item->aicon->wm_instance);
		item->aicon->wm_instance = wstrdup(item->id);
	}

	needs_attention = (item->status && strcasecmp(item->status, "NeedsAttention") == 0);

	if (needs_attention) {
		if (attn_pixmap_img) {
			final_img = attn_pixmap_img;
			attn_pixmap_img = NULL;
		} else if (item->attention_icon_name) {
			final_img = sni_resolve_icon_name(item, sni_state->scr, item->attention_icon_name, item->icon_theme_path);
		}
	}

	if (!final_img) {
		if (icon_pixmap_img) {
			final_img = icon_pixmap_img;
			icon_pixmap_img = NULL;
		} else if (item->icon_name) {
			final_img = sni_resolve_icon_name(item, sni_state->scr, item->icon_name, item->icon_theme_path);
		}
	}

	if (icon_pixmap_img)
		RReleaseImage(icon_pixmap_img);
	if (attn_pixmap_img)
		RReleaseImage(attn_pixmap_img);

	if (final_img) {
		set_icon_image_from_image(item->aicon->icon, final_img);
		update_icon_pixmap(item->aicon->icon);
	} else {
		wIconUpdate(item->aicon->icon);
	}

	wAppIconPaint(item->aicon);
}

static void sni_flush_and_dispatch(void)
{
	if (!sni_state || !sni_state->conn || sni_state->in_dispatch)
		return;

	dbus_connection_flush(sni_state->conn);
	sni_state->in_dispatch = True;
	while (dbus_connection_get_dispatch_status(sni_state->conn) == DBUS_DISPATCH_DATA_REMAINS) {
		dbus_connection_dispatch(sni_state->conn);
	}
	sni_state->in_dispatch = False;
}

static void sni_request_properties(SNIItem *item)
{
	DBusMessage *msg;
	DBusPendingCall *pending = NULL;
	const char *iface = SNI_ITEM_IFACE_KDE;
	const char *dest;

	if (!sni_state || !sni_state->conn || !item)
		return;

	if (item->pending) {
		dbus_pending_call_cancel(item->pending);
		dbus_pending_call_unref(item->pending);
		item->pending = NULL;
	}

	dest = (item->service_name && item->service_name[0] != '\0') ? item->service_name : item->sender_unique;
	if (!dest || !item->obj_path)
		return;

	msg = dbus_message_new_method_call(dest, item->obj_path, DBUS_PROPS_IFACE, "GetAll");
	if (!msg)
		return;

	dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(sni_state->conn, msg, &pending, 5000) && pending) {
		item->pending = pending;
		dbus_pending_call_set_notify(pending, sni_on_get_all_reply, item, NULL);
	}
	dbus_message_unref(msg);
	sni_flush_and_dispatch();
}

static void sni_emit_watcher_signal(const char *sig_name, const char *arg_str)
{
	DBusMessage *sig;

	if (!sni_state || !sni_state->conn)
		return;

	sig = dbus_message_new_signal(SNI_WATCHER_OBJ_PATH, SNI_WATCHER_IFACE_KDE, sig_name);
	if (!sig)
		return;

	if (arg_str)
		dbus_message_append_args(sig, DBUS_TYPE_STRING, &arg_str, DBUS_TYPE_INVALID);

	dbus_connection_send(sni_state->conn, sig, NULL);
	dbus_message_unref(sig);
	sni_flush_and_dispatch();
}

static void sni_free_item(SNIItem *item, Bool detach_dock)
{
	if (!item)
		return;

	if (item->pending) {
		dbus_pending_call_cancel(item->pending);
		dbus_pending_call_unref(item->pending);
		item->pending = NULL;
	}

	if (item->layout_pending) {
		dbus_pending_call_cancel(item->layout_pending);
		dbus_pending_call_unref(item->layout_pending);
		item->layout_pending = NULL;
	}

	if (item->aicon) {
		WAppIcon *aicon = item->aicon;
		item->aicon = NULL;

		if (detach_dock && aicon->dock) {
			aicon->running = 0;
			aicon->lock = 0;
			wDockDetach(aicon->dock, aicon);
		} else if (detach_dock) {
			wAppIconDestroy(aicon);
		}
	}

	if (item->service_name)
		wfree(item->service_name);
	if (item->sender_unique)
		wfree(item->sender_unique);
	if (item->obj_path)
		wfree(item->obj_path);
	if (item->full_id)
		wfree(item->full_id);
	if (item->id)
		wfree(item->id);
	if (item->title)
		wfree(item->title);
	if (item->status)
		wfree(item->status);
	if (item->icon_name)
		wfree(item->icon_name);
	if (item->attention_icon_name)
		wfree(item->attention_icon_name);
	if (item->icon_theme_path)
		wfree(item->icon_theme_path);
	if (item->tooltip_title)
		wfree(item->tooltip_title);
	if (item->tooltip_body)
		wfree(item->tooltip_body);
	if (item->balloon_text)
		wfree(item->balloon_text);
	if (item->resolved_icon_name)
		wfree(item->resolved_icon_name);
	if (item->resolved_icon_path)
		wfree(item->resolved_icon_path);
	if (item->menu_path)
		wfree(item->menu_path);
	if (item->menu)
		wMenuDestroy(item->menu, True);

	wfree(item);
}

static SNIItem *sni_find_item_by_aicon(WAppIcon *aicon)
{
	SNIItem *it;

	if (!sni_state || !aicon)
		return NULL;

	for (it = sni_state->items; it != NULL; it = it->next) {
		if (it->aicon == aicon)
			return it;
	}
	return NULL;
}

static void sni_register_item(const char *service_arg, const char *sender)
{
	char *bus_name = NULL;
	char *obj_path = NULL;
	char *full_id = NULL;
	size_t full_len;
	SNIItem *it;
	WScreen *scr;
	int x = 0, y = 0;

	if (!sni_state || !service_arg || service_arg[0] == '\0')
		return;

	scr = sni_state->scr;

	if (service_arg[0] == '/') {
		if (!sender || sender[0] == '\0')
			return;
		bus_name = wstrdup(sender);
		obj_path = wstrdup(service_arg);
	} else {
		const char *slash = strchr(service_arg, '/');
		if (slash) {
			size_t blen = (size_t)(slash - service_arg);
			bus_name = wmalloc(blen + 1);
			memcpy(bus_name, service_arg, blen);
			bus_name[blen] = '\0';
			obj_path = wstrdup(slash);
		} else {
			bus_name = wstrdup(service_arg);
			obj_path = wstrdup(SNI_ITEM_DEFAULT_PATH);
		}
	}

	full_len = strlen(bus_name) + strlen(obj_path) + 1;
	full_id = wmalloc(full_len);
	snprintf(full_id, full_len, "%s%s", bus_name, obj_path);

	/* Check if already registered */
	for (it = sni_state->items; it != NULL; it = it->next) {
		if (strcmp(it->full_id, full_id) == 0 ||
		    (sender && it->sender_unique && strcmp(it->sender_unique, sender) == 0 &&
		     strcmp(it->obj_path, obj_path) == 0)) {
			wfree(bus_name);
			wfree(obj_path);
			wfree(full_id);
			sni_request_properties(it);
			return;
		}
	}

	it = wmalloc(sizeof(SNIItem));
	it->service_name = bus_name;
	it->sender_unique = sender ? wstrdup(sender) : wstrdup(bus_name);
	it->obj_path = obj_path;
	it->full_id = full_id;
	it->balloon_text = wstrdup(bus_name);

	if (scr && scr->dock && wDockFindFreeSlot(scr->dock, &x, &y)) {
		WAppIcon *aicon = wAppIconCreateForDock(scr, NULL, it->service_name,
		                                        "StatusNotifierItem", TILE_NORMAL);
		if (aicon) {
			aicon->sni = 1;
			aicon->lock = 1;
			if (wDockAttachIcon(scr->dock, aicon, x, y, True)) {
				if (!scr->dock->collapsed)
					XMapWindow(dpy, aicon->icon->core->window);
				it->aicon = aicon;
			} else {
				wAppIconDestroy(aicon);
			}
		}
	} else {
		wwarning("SNI: could not find a free Dock slot for %s", it->full_id);
	}

	it->next = sni_state->items;
	sni_state->items = it;

	sni_emit_watcher_signal("StatusNotifierItemRegistered", it->full_id);
	sni_request_properties(it);
}

static void sni_unregister_by_owner(const char *owner_name)
{
	SNIItem **prev_ptr;

	if (!sni_state || !owner_name || owner_name[0] == '\0')
		return;

	prev_ptr = &sni_state->items;
	while (*prev_ptr != NULL) {
		SNIItem *cur = *prev_ptr;
		if ((cur->service_name && strcmp(cur->service_name, owner_name) == 0) ||
		    (cur->sender_unique && strcmp(cur->sender_unique, owner_name) == 0)) {
			char *unreg_id = wstrdup(cur->full_id);
			*prev_ptr = cur->next;
			sni_free_item(cur, True);
			sni_emit_watcher_signal("StatusNotifierItemUnregistered", unreg_id);
			wfree(unreg_id);
		} else {
			prev_ptr = &cur->next;
		}
	}
}

static void sni_append_registered_items_variant(DBusMessageIter *iter)
{
	DBusMessageIter var_iter, arr_iter;
	SNIItem *it;

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "as", &var_iter);
	dbus_message_iter_open_container(&var_iter, DBUS_TYPE_ARRAY, "s", &arr_iter);
	for (it = sni_state ? sni_state->items : NULL; it != NULL; it = it->next) {
		dbus_message_iter_append_basic(&arr_iter, DBUS_TYPE_STRING, &it->full_id);
	}
	dbus_message_iter_close_container(&var_iter, &arr_iter);
	dbus_message_iter_close_container(iter, &var_iter);
}

static void sni_append_bool_variant(DBusMessageIter *iter, dbus_bool_t val)
{
	DBusMessageIter var_iter;

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "b", &var_iter);
	dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_BOOLEAN, &val);
	dbus_message_iter_close_container(iter, &var_iter);
}

static void sni_append_int32_variant(DBusMessageIter *iter, dbus_int32_t val)
{
	DBusMessageIter var_iter;

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "i", &var_iter);
	dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_INT32, &val);
	dbus_message_iter_close_container(iter, &var_iter);
}

static DBusHandlerResult sni_watcher_message_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const char *iface = dbus_message_get_interface(msg);
	const char *member = dbus_message_get_member(msg);

	(void)user_data;

	if (!member)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_is_method_call(msg, DBUS_INTROSPECT_IFACE, "Introspect")) {
		DBusMessage *reply = dbus_message_new_method_return(msg);
		if (reply) {
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &sni_watcher_introspect_xml,
			                         DBUS_TYPE_INVALID);
			dbus_connection_send(conn, reply, NULL);
			dbus_message_unref(reply);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_method_call(msg, DBUS_PROPS_IFACE, "Get")) {
		const char *prop_iface = NULL;
		const char *prop_name = NULL;
		DBusError err;
		DBusMessage *reply;
		DBusMessageIter iter;

		dbus_error_init(&err);
		if (!dbus_message_get_args(msg, &err,
		                           DBUS_TYPE_STRING, &prop_iface,
		                           DBUS_TYPE_STRING, &prop_name,
		                           DBUS_TYPE_INVALID)) {
			dbus_error_free(&err);
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		reply = dbus_message_new_method_return(msg);
		if (!reply)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		dbus_message_iter_init_append(reply, &iter);
		if (strcmp(prop_name, "IsStatusNotifierHostRegistered") == 0) {
			sni_append_bool_variant(&iter, TRUE);
		} else if (strcmp(prop_name, "ProtocolVersion") == 0) {
			sni_append_int32_variant(&iter, 0);
		} else if (strcmp(prop_name, "RegisteredStatusNotifierItems") == 0) {
			sni_append_registered_items_variant(&iter);
		} else {
			dbus_message_unref(reply);
			reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
		}
		if (reply) {
			dbus_connection_send(conn, reply, NULL);
			dbus_message_unref(reply);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_method_call(msg, DBUS_PROPS_IFACE, "GetAll")) {
		DBusMessage *reply = dbus_message_new_method_return(msg);
		DBusMessageIter iter, dict_iter, entry_iter;
		const char *k_host = "IsStatusNotifierHostRegistered";
		const char *k_ver = "ProtocolVersion";
		const char *k_items = "RegisteredStatusNotifierItems";

		if (!reply)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		dbus_message_iter_init_append(reply, &iter);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

		dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
		dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &k_host);
		sni_append_bool_variant(&entry_iter, TRUE);
		dbus_message_iter_close_container(&dict_iter, &entry_iter);

		dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
		dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &k_ver);
		sni_append_int32_variant(&entry_iter, 0);
		dbus_message_iter_close_container(&dict_iter, &entry_iter);

		dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
		dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &k_items);
		sni_append_registered_items_variant(&entry_iter);
		dbus_message_iter_close_container(&dict_iter, &entry_iter);

		dbus_message_iter_close_container(&iter, &dict_iter);
		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!iface || (strcmp(iface, SNI_WATCHER_IFACE_KDE) == 0 ||
	               strcmp(iface, SNI_WATCHER_IFACE_FDO) == 0)) {
		if (strcmp(member, "RegisterStatusNotifierItem") == 0) {
			const char *service = NULL;
			DBusError err;

			dbus_error_init(&err);
			if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &service, DBUS_TYPE_INVALID)) {
				DBusMessage *reply = dbus_message_new_method_return(msg);
				if (reply) {
					dbus_connection_send(conn, reply, NULL);
					dbus_message_unref(reply);
				}
				sni_register_item(service, dbus_message_get_sender(msg));
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			dbus_error_free(&err);
		} else if (strcmp(member, "RegisterStatusNotifierHost") == 0) {
			DBusMessage *reply = dbus_message_new_method_return(msg);
			if (reply) {
				dbus_connection_send(conn, reply, NULL);
				dbus_message_unref(reply);
			}
			sni_emit_watcher_signal("StatusNotifierHostRegistered", NULL);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult sni_bus_filter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	(void)conn;
	(void)user_data;

	if (!sni_state)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	/* Track client disconnects via NameOwnerChanged */
	if (dbus_message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged")) {
		const char *name = NULL;
		const char *old_owner = NULL;
		const char *new_owner = NULL;
		DBusError err;

		dbus_error_init(&err);
		if (dbus_message_get_args(msg, &err,
		                          DBUS_TYPE_STRING, &name,
		                          DBUS_TYPE_STRING, &old_owner,
		                          DBUS_TYPE_STRING, &new_owner,
		                          DBUS_TYPE_INVALID)) {
			if (new_owner && new_owner[0] == '\0') {
				if (name && name[0] != '\0')
					sni_unregister_by_owner(name);
				if (old_owner && old_owner[0] != '\0')
					sni_unregister_by_owner(old_owner);
			}
		} else {
			dbus_error_free(&err);
		}
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	/* Track menu layout changes. If the menu is currently mapped on screen
	 * (e.g. nm-applet rebuilding its GtkMenu after a background Wi-Fi scan
	 * triggered by AboutToShow), keep the WMenu open and asynchronously
	 * refresh its entries' dbus_id values in-place so clicking an entry never
	 * sends a stale ID.
	 *
	 * If the menu is unmapped, do NOT destroy it here: this signal handler can
	 * run from a D-Bus dispatch re-entered while the menu is being closed (the
	 * LayoutUpdated landing exactly during the WMenu entry callback, after
	 * closeCascade() has already unmapped it but while menuMouseDown() still
	 * dereferences desc->parent after the callback). Freeing it then leaves
	 * menuMouseDown() with a dangling pointer and crashes in wMenuMove().
	 * The next right-click always rebuilds the menu from a fresh GetLayout, and
	 * the unmapped tree is torn down by wSnihostPostButtonPress() / the next
	 * open, so leaving it cached is safe. */
	if (dbus_message_is_signal(msg, "com.canonical.dbusmenu", "LayoutUpdated")) {
		const char *sender = dbus_message_get_sender(msg);
		const char *path = dbus_message_get_path(msg);
		SNIItem *it;

		if (sender) {
			for (it = sni_state->items; it; it = it->next) {
				if (it->sender_unique && strcmp(it->sender_unique, sender) == 0 &&
				    (!path || !it->menu_path || strcmp(it->menu_path, path) == 0) &&
				    it->menu) {
					if (it->menu->flags.mapped)
						sni_request_dbusmenu_sync(it);
					break;
				}
			}
		}
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	/* Listen for dynamic property update signals from StatusNotifierItems */
	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL) {
		const char *iface = dbus_message_get_interface(msg);
		const char *member = dbus_message_get_member(msg);
		const char *sender = dbus_message_get_sender(msg);
		const char *path = dbus_message_get_path(msg);

		if (iface && member && sender &&
		    (strcmp(iface, SNI_ITEM_IFACE_KDE) == 0 || strcmp(iface, SNI_ITEM_IFACE_FDO) == 0)) {
			if (strcmp(member, "NewIcon") == 0 ||
			    strcmp(member, "NewTitle") == 0 ||
			    strcmp(member, "NewToolTip") == 0 ||
			    strcmp(member, "NewAttentionIcon") == 0 ||
			    strcmp(member, "NewStatus") == 0) {
				SNIItem *it;
				for (it = sni_state->items; it != NULL; it = it->next) {
					if ((it->sender_unique && strcmp(it->sender_unique, sender) == 0) &&
					    (!path || strcmp(it->obj_path, path) == 0)) {
						sni_request_properties(it);
					}
				}
			}
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void sni_dbus_input_proc(int fd, int mask, void *clientData)
{
	(void)fd;
	(void)mask;
	(void)clientData;

	if (!sni_state || !sni_state->conn)
		return;

	if (!dbus_connection_read_write(sni_state->conn, 0)) {
		if (sni_state->input_handler) {
			WMDeleteInputHandler(sni_state->input_handler);
			sni_state->input_handler = NULL;
		}
		return;
	}

	sni_state->in_dispatch = True;
	while (dbus_connection_get_dispatch_status(sni_state->conn) == DBUS_DISPATCH_DATA_REMAINS) {
		dbus_connection_dispatch(sni_state->conn);
	}
	sni_state->in_dispatch = False;
	dbus_connection_flush(sni_state->conn);
	XFlush(dpy);
}

static void sni_send_item_coord_method(WAppIcon *aicon, const char *method, int x, int y)
{
	SNIItem *item;
	DBusMessage *msg;
	const char *dest;
	dbus_int32_t dx = x, dy = y;

	item = sni_find_item_by_aicon(aicon);
	if (!item || !sni_state || !sni_state->conn)
		return;

	dest = (item->service_name && item->service_name[0] != '\0') ? item->service_name : item->sender_unique;
	if (!dest || !item->obj_path)
		return;

	msg = dbus_message_new_method_call(dest, item->obj_path, SNI_ITEM_IFACE_KDE, method);
	if (!msg)
		return;

	dbus_message_set_no_reply(msg, TRUE);
	dbus_message_append_args(msg, DBUS_TYPE_INT32, &dx, DBUS_TYPE_INT32, &dy, DBUS_TYPE_INVALID);
	dbus_connection_send(sni_state->conn, msg, NULL);
	dbus_message_unref(msg);
	sni_flush_and_dispatch();
}

/* --- DBusMenu (com.canonical.dbusmenu) host --- */

#define DBUSMENU_IFACE "com.canonical.dbusmenu"

typedef struct SNIMenuCtx {
	SNIItem *item;
	int dbus_id;
	int toggle_type;	/* 0 = none, 1 = checkmark, 2 = radio */
	int toggle_state;	/* -1 = absent, 0 = off, 1 = on */
} SNIMenuCtx;

typedef struct SNILayoutReq {
	SNIItem *item;
	int x, y;
} SNILayoutReq;

static void sni_dbusmenu_event(SNIItem *item, int id, const char *event_id, int new_state);

static void sni_release_grabs(void)
{
	if (!sni_state)
		return;

	if (sni_state->pointer_grabbed) {
		XUngrabPointer(dpy, CurrentTime);
		sni_state->pointer_grabbed = False;
	}
	if (sni_state->keyboard_grabbed) {
		XUngrabKeyboard(dpy, CurrentTime);
		sni_state->keyboard_grabbed = False;
	}
	XFlush(dpy);
}

static void sni_grab_menu(WMenu *menu)
{
	if (!sni_state || !menu || !menu->frame || !menu->frame->core)
		return;

	XSync(dpy, False);
	if (!sni_state->pointer_grabbed) {
		if (XGrabPointer(dpy, menu->frame->core->window, True,
		                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime) == GrabSuccess)
			sni_state->pointer_grabbed = True;
	}
	if (!sni_state->keyboard_grabbed) {
		if (XGrabKeyboard(dpy, menu->frame->core->window, True,
		                  GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
			sni_state->keyboard_grabbed = True;
	}
}

static void sni_menu_callback(WMenu *menu, WMenuEntry *entry)
{
	SNIMenuCtx *ctx = entry->clientdata;
	int new_state = -1;

	(void)menu;
	if (ctx) {
		/* For toggle entries, pass the new state as the Event variant data:
		 * a radio becomes selected, a checkmark toggles. Plain entries send
		 * an empty string variant. */
		if (ctx->toggle_type == 2)
			new_state = 1;
		else if (ctx->toggle_type == 1)
			new_state = (ctx->toggle_state == 1) ? 0 : 1;

		sni_dbusmenu_event(ctx->item, ctx->dbus_id, "clicked", new_state);
	}

	/* Do NOT touch the menu here: menuMouseDown() is still on the call
	 * stack and closes the menu itself (close_on_exit), then keeps
	 * dereferencing it (selectEntry/insertEntry/...). Closing or destroying
	 * it here caused a use-after-free crash on every leaf activation. */
}

static void sni_menu_ctx_free(void *data)
{
	wfree(data);
}

static char *sni_strip_mnemonic(const char *src)
{
	char *dst, *d;
	const char *s;

	if (!src)
		return NULL;

	dst = wmalloc(strlen(src) + 1);
	d = dst;
	for (s = src; *s; s++) {
		if (*s == '_') {
			if (*(s + 1) == '_') {
				*d++ = '_';
				s++;
			}
			continue;
		}
		*d++ = *s;
	}
	*d = '\0';
	return dst;
}

static void sni_dbusmenu_parse_props(DBusMessageIter *props_iter, char **label_out,
                                     int *type_out, char **icon_name_out,
                                     int *toggle_type_out, int *toggle_state_out,
                                     int *enabled_out, int *visible_out)
{
	DBusMessageIter entry_iter;

	*label_out = NULL;
	*type_out = 0;		/* 0 = standard, 1 = separator */
	*icon_name_out = NULL;
	*toggle_type_out = 0;	/* 0 = none, 1 = checkmark, 2 = radio */
	*toggle_state_out = -1;	/* -1 = absent, 0 = off, 1 = on, 2 = indeterminate */
	*enabled_out = 1;
	*visible_out = 1;

	dbus_message_iter_recurse(props_iter, &entry_iter);
	while (dbus_message_iter_get_arg_type(&entry_iter) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter e_iter, v_iter;
		const char *key = NULL;

		dbus_message_iter_recurse(&entry_iter, &e_iter);
		dbus_message_iter_get_basic(&e_iter, &key);
		if (dbus_message_iter_next(&e_iter) &&
		    dbus_message_iter_get_arg_type(&e_iter) == DBUS_TYPE_VARIANT) {
			dbus_message_iter_recurse(&e_iter, &v_iter);
			if (strcmp(key, "label") == 0 &&
			    dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_STRING) {
				const char *v = NULL;
				dbus_message_iter_get_basic(&v_iter, &v);
				if (v && v[0]) {
					if (*label_out)
						wfree(*label_out);
					*label_out = sni_strip_mnemonic(v);
				}
			} else if (strcmp(key, "type") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_STRING) {
				const char *v = NULL;
				dbus_message_iter_get_basic(&v_iter, &v);
				if (v && strcmp(v, "separator") == 0)
					*type_out = 1;
			} else if (strcmp(key, "icon-name") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_STRING) {
				const char *v = NULL;
				dbus_message_iter_get_basic(&v_iter, &v);
				if (v && v[0]) {
					if (*icon_name_out)
						wfree(*icon_name_out);
					*icon_name_out = wstrdup(v);
				}
			} else if (strcmp(key, "toggle-type") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_STRING) {
				const char *v = NULL;
				dbus_message_iter_get_basic(&v_iter, &v);
				if (v && strcmp(v, "radio") == 0)
					*toggle_type_out = 2;
				else if (v && strcmp(v, "checkmark") == 0)
					*toggle_type_out = 1;
			} else if (strcmp(key, "toggle-state") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_INT32) {
				dbus_int32_t v = 0;
				dbus_message_iter_get_basic(&v_iter, &v);
				*toggle_state_out = v;
			} else if (strcmp(key, "enabled") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_BOOLEAN) {
				dbus_bool_t v = TRUE;
				dbus_message_iter_get_basic(&v_iter, &v);
				*enabled_out = v ? 1 : 0;
			} else if (strcmp(key, "visible") == 0 &&
			           dbus_message_iter_get_arg_type(&v_iter) == DBUS_TYPE_BOOLEAN) {
				dbus_bool_t v = TRUE;
				dbus_message_iter_get_basic(&v_iter, &v);
				*visible_out = v ? 1 : 0;
			}
		}
		dbus_message_iter_next(&entry_iter);
	}
}

static void sni_entry_set_icon_toggle(SNIItem *item, WMenuEntry *entry,
                                      const char *icon_name, int toggle_type,
                                      int toggle_state, int enabled)
{
	WScreen *scr = item->aicon->icon->core->screen_ptr;

	entry->flags.enabled = enabled ? 1 : 0;

	if (icon_name && icon_name[0]) {
		RImage *img = sni_resolve_icon_name(NULL, scr, icon_name, item->icon_theme_path);

		if (img) {
			int max_size = WMFontHeight(scr->menu_entry_font) + 2;

			if (max_size < 12)
				max_size = 12;
			img = wIconValidateIconSize(img, max_size);
		}
		if (img) {
			WPixmap *pix = wmalloc(sizeof(WPixmap));

			memset(pix, 0, sizeof(WPixmap));
			if (RConvertImageMask(scr->rcontext, img, &pix->image, &pix->mask, 128)) {
				pix->width = img->width;
				pix->height = img->height;
				pix->depth = scr->w_depth;
				entry->icon = pix;
			} else {
				wfree(pix);
			}
			RReleaseImage(img);
		}
	}

	if (toggle_type != 0) {
		entry->flags.indicator = 1;
		entry->flags.indicator_on = (toggle_state == 1);
		entry->flags.indicator_type = (toggle_type == 2) ? MI_DIAMOND : MI_CHECK;
	}
}

static void sni_build_dbusmenu(SNIItem *item, WMenu *menu, DBusMessageIter *children_iter)
{
	DBusMessageIter elem_iter;

	dbus_message_iter_recurse(children_iter, &elem_iter);
	while (dbus_message_iter_get_arg_type(&elem_iter) == DBUS_TYPE_VARIANT) {
		DBusMessageIter var_iter, struct_iter, props_iter;
		dbus_int32_t id = 0;
		char *label = NULL;
		char *icon_name = NULL;
		int type = 0;
		int toggle_type = 0;
		int toggle_state = -1;
		int enabled = 1;
		int visible = 1;
		int has_children = 0;

		dbus_message_iter_recurse(&elem_iter, &var_iter);
		dbus_message_iter_recurse(&var_iter, &struct_iter);

		if (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_INT32)
			dbus_message_iter_get_basic(&struct_iter, &id);

		if (dbus_message_iter_next(&struct_iter) &&
		    dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_ARRAY) {
			props_iter = struct_iter;
			sni_dbusmenu_parse_props(&props_iter, &label, &type, &icon_name,
			                         &toggle_type, &toggle_state, &enabled, &visible);
		}

		if (dbus_message_iter_next(&struct_iter) &&
		    dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_ARRAY) {
			DBusMessageIter child_check;

			dbus_message_iter_recurse(&struct_iter, &child_check);
			if (dbus_message_iter_get_arg_type(&child_check) == DBUS_TYPE_VARIANT)
				has_children = 1;
		}

		if (!visible) {
			if (label)
				wfree(label);
			if (icon_name)
				wfree(icon_name);
			dbus_message_iter_next(&elem_iter);
			continue;
		}

		if (type == 1) {
			/* separator */
			wMenuAddCallback(menu, "", NULL, NULL);
		} else if (has_children) {
			WMenuEntry *entry;
			WMenu *sub;

			/* Submenu parent: do NOT attach sni_menu_callback! In Window Maker's
			 * selectEntry() (src/menu.c:1394-1404), if a cascade parent entry has
			 * a callback, it is invoked *when opening the submenu* before mapping
			 * menu->cascades[entry->cascade]. Attaching sni_menu_callback to a
			 * submenu parent would unmap/close the menu as soon as the submenu opens! */
			entry = wMenuAddCallback(menu, label ? label : "", NULL, NULL);
			sni_entry_set_icon_toggle(item, entry, icon_name, toggle_type, toggle_state, enabled);
			sub = wMenuCreate(item->aicon->icon->core->screen_ptr, NULL, False);
			sni_build_dbusmenu(item, sub, &struct_iter);
			wMenuEntrySetCascade(menu, entry, sub);
		} else {
			SNIMenuCtx *ctx = wmalloc(sizeof(SNIMenuCtx));
			WMenuEntry *entry;

			ctx->item = item;
			ctx->dbus_id = id;
			ctx->toggle_type = toggle_type;
			ctx->toggle_state = toggle_state;
			entry = wMenuAddCallback(menu, label ? label : "", sni_menu_callback, ctx);
			entry->free_cdata = sni_menu_ctx_free;
			sni_entry_set_icon_toggle(item, entry, icon_name, toggle_type, toggle_state, enabled);
		}

		if (label)
			wfree(label);
		if (icon_name)
			wfree(icon_name);

		dbus_message_iter_next(&elem_iter);
	}
}

/* Synchronize dbus_id and toggle state in-place on an already mapped WMenu
 * when the DBusMenu server emits LayoutUpdated while the menu is open (for
 * example, nm-applet recreating its GtkMenuItem widgets after a Wi-Fi scan
 * triggered by AboutToShow). */
static void sni_sync_dbusmenu_ids(WMenu *menu, DBusMessageIter *children_iter)
{
	DBusMessageIter elem_iter;
	int idx = 0;

	if (!menu || !children_iter)
		return;

	dbus_message_iter_recurse(children_iter, &elem_iter);
	while (dbus_message_iter_get_arg_type(&elem_iter) == DBUS_TYPE_VARIANT) {
		DBusMessageIter var_iter, struct_iter, props_iter;
		dbus_int32_t id = 0;
		char *label = NULL;
		char *icon_name = NULL;
		int type = 0;
		int toggle_type = 0;
		int toggle_state = -1;
		int enabled = 1;
		int visible = 1;
		int has_children = 0;
		int match_idx = -1;
		int i;

		dbus_message_iter_recurse(&elem_iter, &var_iter);
		dbus_message_iter_recurse(&var_iter, &struct_iter);

		if (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_INT32)
			dbus_message_iter_get_basic(&struct_iter, &id);

		if (dbus_message_iter_next(&struct_iter) &&
		    dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_ARRAY) {
			props_iter = struct_iter;
			sni_dbusmenu_parse_props(&props_iter, &label, &type, &icon_name,
			                         &toggle_type, &toggle_state, &enabled, &visible);
		}

		if (dbus_message_iter_next(&struct_iter) &&
		    dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_ARRAY) {
			DBusMessageIter child_check;

			dbus_message_iter_recurse(&struct_iter, &child_check);
			if (dbus_message_iter_get_arg_type(&child_check) == DBUS_TYPE_VARIANT)
				has_children = 1;
		}

		if (!visible) {
			if (label)
				wfree(label);
			if (icon_name)
				wfree(icon_name);
			dbus_message_iter_next(&elem_iter);
			continue;
		}

		for (i = idx; i < menu->entry_no; i++) {
			WMenuEntry *e = menu->entries[i];
			const char *elabel = e->text ? e->text : "";
			const char *dlabel = (type == 1) ? "" : (label ? label : "");
			int e_has_sub = (e->cascade >= 0 && menu->cascades &&
			                 e->cascade < menu->cascade_no &&
			                 menu->cascades[e->cascade] != NULL);

			if (e_has_sub == has_children && strcmp(elabel, dlabel) == 0) {
				match_idx = i;
				break;
			}
		}

		if (match_idx < 0 && type != 1 && label && label[0] != '\0') {
			for (i = 0; i < idx && i < menu->entry_no; i++) {
				WMenuEntry *e = menu->entries[i];
				const char *elabel = e->text ? e->text : "";
				int e_has_sub = (e->cascade >= 0 && menu->cascades &&
				                 e->cascade < menu->cascade_no &&
				                 menu->cascades[e->cascade] != NULL);

				if (e_has_sub == has_children && strcmp(elabel, label) == 0) {
					match_idx = i;
					break;
				}
			}
		}

		if (match_idx >= 0) {
			WMenuEntry *e = menu->entries[match_idx];

			e->flags.enabled = enabled ? 1 : 0;
			if (has_children) {
				if (e->cascade >= 0 && menu->cascades &&
				    e->cascade < menu->cascade_no &&
				    menu->cascades[e->cascade]) {
					sni_sync_dbusmenu_ids(menu->cascades[e->cascade], &struct_iter);
				}
			} else if (e->clientdata) {
				SNIMenuCtx *ctx = (SNIMenuCtx *)e->clientdata;

				ctx->dbus_id = id;
				ctx->toggle_type = toggle_type;
				ctx->toggle_state = toggle_state;
				if (toggle_type != 0) {
					e->flags.indicator = 1;
					e->flags.indicator_on = (toggle_state == 1);
					e->flags.indicator_type = (toggle_type == 2) ? MI_DIAMOND : MI_CHECK;
				}
			}
			if (match_idx >= idx)
				idx = match_idx + 1;
		}

		if (label)
			wfree(label);
		if (icon_name)
			wfree(icon_name);

		dbus_message_iter_next(&elem_iter);
	}
}

static void sni_on_sync_layout_reply(DBusPendingCall *pending, void *user_data)
{
	SNIItem *item = (SNIItem *)user_data;
	DBusMessage *reply;
	DBusMessageIter root_iter, layout_iter;

	reply = dbus_pending_call_steal_reply(pending);
	if (item->layout_pending == pending) {
		dbus_pending_call_unref(item->layout_pending);
		item->layout_pending = NULL;
	}
	if (!reply)
		return;

	if (!item->menu || !item->menu->flags.mapped ||
	    dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ||
	    !dbus_message_iter_init(reply, &root_iter) ||
	    dbus_message_iter_get_arg_type(&root_iter) != DBUS_TYPE_UINT32) {
		dbus_message_unref(reply);
		return;
	}

	if (!dbus_message_iter_next(&root_iter) ||
	    dbus_message_iter_get_arg_type(&root_iter) != DBUS_TYPE_STRUCT) {
		dbus_message_unref(reply);
		return;
	}

	dbus_message_iter_recurse(&root_iter, &layout_iter);
	if (!dbus_message_iter_next(&layout_iter) ||
	    !dbus_message_iter_next(&layout_iter) ||
	    dbus_message_iter_get_arg_type(&layout_iter) != DBUS_TYPE_ARRAY) {
		dbus_message_unref(reply);
		return;
	}

	sni_sync_dbusmenu_ids(item->menu, &layout_iter);
	dbus_message_unref(reply);
}

static void sni_request_dbusmenu_sync(SNIItem *item)
{
	DBusMessage *msg;
	DBusPendingCall *pending = NULL;
	const char *dest;
	dbus_int32_t parent_id = 0;
	dbus_int32_t depth = -1;
	const char *prop_names[] = {
		"label", "type", "enabled", "visible",
		"children-display", "icon-name", "toggle-type", "toggle-state"
	};
	const char **props = prop_names;

	if (!sni_state || !sni_state->conn || !item || !item->menu || !item->menu->flags.mapped)
		return;

	if (item->layout_pending) {
		dbus_pending_call_cancel(item->layout_pending);
		dbus_pending_call_unref(item->layout_pending);
		item->layout_pending = NULL;
	}

	dest = (item->service_name && item->service_name[0]) ? item->service_name : item->sender_unique;
	if (!dest || !item->menu_path)
		return;

	msg = dbus_message_new_method_call(dest, item->menu_path, DBUSMENU_IFACE, "GetLayout");
	if (!msg)
		return;

	dbus_message_append_args(msg,
	                         DBUS_TYPE_INT32, &parent_id,
	                         DBUS_TYPE_INT32, &depth,
	                         DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &props, 8,
	                         DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(sni_state->conn, msg, &pending, 5000) && pending) {
		item->layout_pending = pending;
		dbus_pending_call_set_notify(pending, sni_on_sync_layout_reply, item, NULL);
	}
	dbus_message_unref(msg);
	sni_flush_and_dispatch();
}

static void sni_on_get_layout_reply(DBusPendingCall *pending, void *user_data)
{
	SNILayoutReq *req = (SNILayoutReq *)user_data;
	SNIItem *item = req->item;
	DBusMessage *reply;
	DBusMessageIter root_iter, layout_iter;

	reply = dbus_pending_call_steal_reply(pending);
	if (item->layout_pending == pending) {
		dbus_pending_call_unref(item->layout_pending);
		item->layout_pending = NULL;
	}
	if (!reply)
		return;

	if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR ||
	    !dbus_message_iter_init(reply, &root_iter) ||
	    dbus_message_iter_get_arg_type(&root_iter) != DBUS_TYPE_UINT32) {
		dbus_message_unref(reply);
		return;
	}

	/* GetLayout returns: u revision, (i id, a{sv} props, av children) layout */
	if (!dbus_message_iter_next(&root_iter) ||
	    dbus_message_iter_get_arg_type(&root_iter) != DBUS_TYPE_STRUCT) {
		dbus_message_unref(reply);
		return;
	}

	/* root_iter now points at (i id, a{sv} props, av children) */
	dbus_message_iter_recurse(&root_iter, &layout_iter);
	/* skip id */
	if (!dbus_message_iter_next(&layout_iter) ||
	    !dbus_message_iter_next(&layout_iter) ||
	    dbus_message_iter_get_arg_type(&layout_iter) != DBUS_TYPE_ARRAY) {
		dbus_message_unref(reply);
		return;
	}
	/* layout_iter now points at av children */

	if (item->aicon) {
		WScreen *scr = item->aicon->icon->core->screen_ptr;
		WMenu *menu = wMenuCreate(scr, NULL, False);
		int mx = req->x, my = req->y;

		if (!menu || !menu->frame) {
			if (menu)
				wMenuDestroy(menu, True);
			dbus_message_unref(reply);
			return;
		}

		sni_build_dbusmenu(item, menu, &layout_iter);
		wMenuRealize(menu);

		/* Keep the menu on-screen: the dock sits on the right edge, so open
		 * the menu to the left of the pointer when it would overflow, and
		 * mark open_to_left so cascade submenus open to the left onto the
		 * screen even when wPreferences.wrap_menus is False. */
		{
			int mw = menu->frame->core->width;
			int mh = menu->frame->core->height;

			if (mx + mw >= scr->scr_width - 2) {
				mx = scr->scr_width - mw - 3;
				menu->flags.open_to_left = 1;
			} else if (mx > scr->scr_width / 2) {
				menu->flags.open_to_left = 1;
			}
			if (my + mh >= scr->scr_height - 2)
				my = scr->scr_height - mh - 3;
			if (mx < 0)
				mx = 0;
			if (my < 0)
				my = 0;
		}

		/* Close any previously open SNI menu before mapping the new one */
		wSnihostCloseMenus();
		item->menu = menu;
		wMenuMapAt(menu, mx, my, False);
		/* Grab the pointer and keyboard so a click outside the menu or
		 * pressing Escape reaches us and closes it. keyboardMenu() would
		 * block here (it is a modal WMMaskEvent loop), so use plain
		 * non-blocking grabs. */
		sni_grab_menu(menu);
	}

	dbus_message_unref(reply);
}

static void sni_request_dbusmenu_layout(SNIItem *item, int x, int y)
{
	DBusMessage *about_msg, *msg;
	DBusPendingCall *pending = NULL;
	SNILayoutReq *req;
	const char *dest;
	dbus_int32_t parent_id = 0;
	dbus_int32_t depth = -1;
	const char *prop_names[] = {
		"label", "type", "enabled", "visible",
		"children-display", "icon-name", "toggle-type", "toggle-state"
	};
	const char **props = prop_names;

	if (!sni_state || !sni_state->conn || !item)
		return;

	if (item->layout_pending) {
		dbus_pending_call_cancel(item->layout_pending);
		dbus_pending_call_unref(item->layout_pending);
		item->layout_pending = NULL;
	}

	dest = (item->service_name && item->service_name[0]) ? item->service_name : item->sender_unique;
	if (!dest || !item->menu_path)
		return;

	/* Notify the DBusMenu server that root menu (id=0) is about to be shown
	 * so dynamic menus (Ayatana/Electron/Qt) populate their items first. */
	about_msg = dbus_message_new_method_call(dest, item->menu_path, DBUSMENU_IFACE, "AboutToShow");
	if (about_msg) {
		dbus_message_set_no_reply(about_msg, TRUE);
		dbus_message_append_args(about_msg, DBUS_TYPE_INT32, &parent_id, DBUS_TYPE_INVALID);
		dbus_connection_send(sni_state->conn, about_msg, NULL);
		dbus_message_unref(about_msg);
	}

	msg = dbus_message_new_method_call(dest, item->menu_path, DBUSMENU_IFACE, "GetLayout");
	if (!msg) {
		sni_flush_and_dispatch();
		return;
	}

	dbus_message_append_args(msg,
	                         DBUS_TYPE_INT32, &parent_id,
	                         DBUS_TYPE_INT32, &depth,
	                         DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &props, 8,
	                         DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(sni_state->conn, msg, &pending, 5000) && pending) {
		req = wmalloc(sizeof(SNILayoutReq));
		req->item = item;
		req->x = x;
		req->y = y;
		item->layout_pending = pending;
		dbus_pending_call_set_notify(pending, sni_on_get_layout_reply, req, wfree);
	}
	dbus_message_unref(msg);
	sni_flush_and_dispatch();
}

static void sni_dbusmenu_event(SNIItem *item, int id, const char *event_id, int new_state)
{
	DBusMessage *msg;
	DBusMessageIter iter;
	const char *dest;
	dbus_int32_t iid = id;
	const char *eid = event_id;
	dbus_uint32_t ts = 0;

	if (!sni_state || !sni_state->conn || !item)
		return;

	dest = (item->service_name && item->service_name[0]) ? item->service_name : item->sender_unique;
	if (!dest || !item->menu_path)
		return;

	msg = dbus_message_new_method_call(dest, item->menu_path, DBUSMENU_IFACE, "Event");
	if (!msg)
		return;

	dbus_message_set_no_reply(msg, TRUE);
	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &iid);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &eid);
	{
		DBusMessageIter var_iter;

		/* Toggle/radio entries carry the new state as an int32 variant;
		 * plain entries send an empty string variant. */
		if (new_state >= 0) {
			dbus_int32_t ns = new_state;

			dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "i", &var_iter);
			dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_INT32, &ns);
		} else {
			const char *empty = "";

			dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var_iter);
			dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_STRING, &empty);
		}
		dbus_message_iter_close_container(&iter, &var_iter);
	}
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &ts);

	dbus_connection_send(sni_state->conn, msg, NULL);
	dbus_message_unref(msg);
	/* Flush only: this runs from the WMenu entry callback, while
	 * menuMouseDown() still holds a pointer to the menu (desc->parent) and
	 * uses it after the callback returns (closeCascade() + wMenuMove()). If
	 * we dispatched incoming messages here, a LayoutUpdated arriving at this
	 * moment would hit sni_bus_filter() with the menu already unmapped by
	 * closeCascade() and free it (wMenuDestroy), leaving menuMouseDown() with
	 * a dangling pointer -> general protection fault in wMenuMove(). The
	 * Event is no_reply, so flushing is enough; the main loop dispatches the
	 * pending signals later, once the menu is no longer in use. */
	dbus_connection_flush(sni_state->conn);
}

static void sni_show_dbusmenu(SNIItem *item, int x, int y)
{
	/* If the item does not export a DBusMenu, ask it to show its own menu */
	if (!item->menu_path || item->menu_path[0] == '\0' ||
	    strcmp(item->menu_path, "/NO_DBUSMENU") == 0) {
		sni_send_item_coord_method(item->aicon, "ContextMenu", x, y);
		return;
	}

	sni_request_dbusmenu_layout(item, x, y);
}

void wSnihostActivate(WAppIcon *aicon, int x, int y)
{
	sni_send_item_coord_method(aicon, "Activate", x, y);
}

void wSnihostSecondaryActivate(WAppIcon *aicon, int x, int y)
{
	sni_send_item_coord_method(aicon, "SecondaryActivate", x, y);
}

void wSnihostContextMenu(WAppIcon *aicon, int x, int y)
{
	SNIItem *item = sni_find_item_by_aicon(aicon);

	if (item)
		sni_show_dbusmenu(item, x, y);
}

static Bool sni_menu_contains_tree(WMenu *root, WMenu *target)
{
	int i;

	if (!root || !target)
		return False;
	if (root == target || root->brother == target)
		return True;
	for (i = 0; root->cascades && i < root->cascade_no; i++) {
		if (sni_menu_contains_tree(root->cascades[i], target))
			return True;
	}
	return False;
}

void wSnihostCloseMenus(void)
{
	SNIItem *it;

	if (!sni_state)
		return;

	for (it = sni_state->items; it; it = it->next) {
		if (it->menu) {
			WMenu *menu = it->menu;

			it->menu = NULL;
			wMenuDestroy(menu, True);
		}
	}

	sni_release_grabs();
}

void wSnihostHandleButtonPress(XEvent *event)
{
	SNIItem *it, *open_item = NULL;
	WMenu *under;

	if (!sni_state || !sni_state->scr || !event)
		return;

	for (it = sni_state->items; it; it = it->next) {
		if (it->menu && it->menu->flags.mapped) {
			open_item = it;
			break;
		}
	}

	if (!open_item) {
		if (sni_state->pointer_grabbed || sni_state->keyboard_grabbed)
			wSnihostCloseMenus();
		return;
	}

	under = wMenuUnderPointer(sni_state->scr);
	if (under && sni_menu_contains_tree(open_item->menu, under)) {
		/* Click is inside the open SNI menu (or one of its cascades):
		 * release the outer pointer/keyboard grabs, target the inner
		 * menu window directly (avoiding the root-menu delayed_select
		 * timer on frame->core->window), and mark send_event = True
		 * so menuMouseDown() tracks the click/drag. */
		sni_release_grabs();
		if (under->menu) {
			event->xbutton.window = under->menu->window;
			event->xbutton.subwindow = None;
		}
		event->xany.send_event = True;
		return;
	}

	/* Click is outside the SNI menu: close the menu and release the grabs. */
	wSnihostCloseMenus();
}

void wSnihostPostButtonPress(void)
{
	SNIItem *it, *open_item = NULL;

	if (!sni_state)
		return;

	for (it = sni_state->items; it; it = it->next) {
		if (it->menu) {
			open_item = it;
			break;
		}
	}

	if (!open_item)
		return;

	if (open_item->menu->flags.mapped) {
		/* The menu remained mapped after menuMouseDown (e.g. the user
		 * clicked a cascade entry to open a submenu): re-establish the
		 * pointer and keyboard grabs so subsequent clicks inside the
		 * submenu, clicks outside, or Escape are captured. */
		sni_grab_menu(open_item->menu);
	} else {
		/* menuMouseDown unmapped the menu after activating a leaf entry
		 * or closing the cascade: destroy the WMenu and release grabs. */
		wSnihostCloseMenus();
	}
}

Bool wSnihostHandleKeyPress(XEvent *event)
{
	SNIItem *it, *open_item = NULL;
	KeySym ksym;

	if (!sni_state || !sni_state->scr || !event)
		return False;

	for (it = sni_state->items; it; it = it->next) {
		if (it->menu && it->menu->flags.mapped) {
			open_item = it;
			break;
		}
	}

	if (!open_item) {
		if (sni_state->pointer_grabbed || sni_state->keyboard_grabbed)
			wSnihostCloseMenus();
		return False;
	}

	ksym = XLookupKeysym(&event->xkey, 0);
	if (ksym == XK_Escape)
		wSnihostCloseMenus();

	return True;
}

void wSnihostScroll(WAppIcon *aicon, int delta, const char *orientation)
{
	SNIItem *item;
	DBusMessage *msg;
	const char *dest;
	dbus_int32_t d = delta;
	const char *orient = orientation ? orientation : "vertical";

	item = sni_find_item_by_aicon(aicon);
	if (!item || !sni_state || !sni_state->conn)
		return;

	dest = (item->service_name && item->service_name[0] != '\0') ? item->service_name : item->sender_unique;
	if (!dest || !item->obj_path)
		return;

	msg = dbus_message_new_method_call(dest, item->obj_path, SNI_ITEM_IFACE_KDE, "Scroll");
	if (!msg)
		return;

	dbus_message_set_no_reply(msg, TRUE);
	dbus_message_append_args(msg, DBUS_TYPE_INT32, &d, DBUS_TYPE_STRING, &orient, DBUS_TYPE_INVALID);
	dbus_connection_send(sni_state->conn, msg, NULL);
	dbus_message_unref(msg);
	sni_flush_and_dispatch();
}

const char *wSnihostGetBalloonText(WAppIcon *aicon)
{
	SNIItem *item = sni_find_item_by_aicon(aicon);

	if (!item)
		return NULL;

	return item->balloon_text;
}

void wSnihostInit(WScreen *scr)
{
	DBusError err;
	DBusConnection *conn;
	DBusObjectPathVTable vtable;
	char host_name[128];
	int fd = -1;
	int ret;

	/* System tray is a per-session singleton; bind to screen 0 only */
	if (!scr || scr->screen != 0 || sni_state != NULL)
		return;

	dbus_error_init(&err);
	conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err) || !conn) {
		wwarning("SNI: could not connect to D-Bus session bus: %s",
		         dbus_error_is_set(&err) ? err.message : "unknown error");
		dbus_error_free(&err);
		return;
	}

	dbus_connection_set_exit_on_disconnect(conn, FALSE);

	if (!dbus_connection_get_unix_fd(conn, &fd) || fd < 0) {
		wwarning("SNI: could not obtain D-Bus file descriptor");
		dbus_connection_close(conn);
		dbus_connection_unref(conn);
		return;
	}

	memset(&vtable, 0, sizeof(vtable));
	vtable.message_function = sni_watcher_message_handler;

	if (!dbus_connection_register_object_path(conn, SNI_WATCHER_OBJ_PATH, &vtable, NULL)) {
		wwarning("SNI: could not register %s object path", SNI_WATCHER_OBJ_PATH);
		dbus_connection_close(conn);
		dbus_connection_unref(conn);
		return;
	}

	ret = dbus_bus_request_name(conn, SNI_WATCHER_BUS_NAME_KDE,
	                            DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                            &err);
	if (dbus_error_is_set(&err)) {
		wwarning("SNI: could not request %s: %s", SNI_WATCHER_BUS_NAME_KDE, err.message);
		dbus_error_free(&err);
	}
	(void)ret;

	dbus_bus_request_name(conn, SNI_WATCHER_BUS_NAME_FDO,
	                      DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                      NULL);

	snprintf(host_name, sizeof(host_name), "org.kde.StatusNotifierHost-%d", (int)getpid());
	dbus_bus_request_name(conn, host_name, DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL);

	dbus_bus_add_match(conn,
	                   "type='signal',sender='org.freedesktop.DBus',"
	                   "interface='org.freedesktop.DBus',member='NameOwnerChanged'",
	                   NULL);
	dbus_bus_add_match(conn,
	                   "type='signal',interface='org.kde.StatusNotifierItem'",
	                   NULL);
	dbus_bus_add_match(conn,
	                   "type='signal',interface='org.freedesktop.StatusNotifierItem'",
	                   NULL);
	dbus_bus_add_match(conn,
	                   "type='signal',interface='com.canonical.dbusmenu'",
	                   NULL);

	dbus_connection_add_filter(conn, sni_bus_filter, NULL, NULL);

	sni_state = wmalloc(sizeof(SNIHostState));
	sni_state->scr = scr;
	sni_state->conn = conn;
	sni_state->host_bus_name = wstrdup(host_name);
	sni_state->items = NULL;
	sni_state->input_handler = WMAddInputHandler(fd, WIReadMask, sni_dbus_input_proc, scr);

	sni_emit_watcher_signal("StatusNotifierHostRegistered", NULL);
	sni_flush_and_dispatch();
}

void wSnihostShutdown(WScreen *scr)
{
	SNIItem *it, *next;

	if (!sni_state || sni_state->scr != scr)
		return;

	if (sni_state->input_handler) {
		WMDeleteInputHandler(sni_state->input_handler);
		sni_state->input_handler = NULL;
	}

	sni_release_grabs();

	it = sni_state->items;
	sni_state->items = NULL;
	while (it) {
		next = it->next;
		sni_free_item(it, True);
		it = next;
	}

	if (sni_state->conn) {
		dbus_connection_remove_filter(sni_state->conn, sni_bus_filter, NULL);
		dbus_connection_unregister_object_path(sni_state->conn, SNI_WATCHER_OBJ_PATH);
		dbus_connection_close(sni_state->conn);
		dbus_connection_unref(sni_state->conn);
		sni_state->conn = NULL;
	}

	if (sni_state->host_bus_name)
		wfree(sni_state->host_bus_name);

	wfree(sni_state);
	sni_state = NULL;

	dbus_shutdown();
}

#endif /* USE_SNI */
