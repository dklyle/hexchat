/* HexChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <adwaita.h>

#include "../common/hexchat.h"
#include "../common/hexchatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/util.h"
#include "../common/fe.h"
#include "../common/history.h"
#include "../common/userlist.h"
#include "../common/server.h"
#include "../common/url.h"
#include "../common/text.h"

#include "fe-gtk4.h"

/* Global application state */
AdwApplication *hexchat_app = NULL;
GtkWidget *main_window = NULL;
GtkWidget *channel_sidebar = NULL;     /* GtkListBox for channel/chat list */
GtkWidget *content_stack = NULL;       /* GtkStack for session content */

static gboolean done = FALSE;
static gboolean notifications_enabled = TRUE;
static feicon current_tray_icon = FE_ICON_NORMAL;
static int session_id_counter = 0;     /* unique id for GtkStack child names */

/* Forward declarations for marker line functions */
static void set_marker_line (session *sess);
static void draw_marker_line (session *sess);
static void clear_marker_line (session *sess);

/* Forward declaration for notification */
static void show_notification (const char *title, const char *body);

/* Forward declarations for topic/nick callbacks */
static void topic_activate_cb (GtkEntry *entry, gpointer user_data);
static void nick_button_clicked_cb (GtkButton *button, gpointer user_data);

/* Forward declarations for search bar */
static void search_toggle (session *sess);
static void search_do (session *sess, gboolean backward);
static void search_clear (session *sess);
static void search_update_highlights (session *sess);
static void search_update_label (session *sess);
static gboolean window_key_pressed_cb (GtkEventControllerKey *controller,
                                       guint keyval, guint keycode,
                                       GdkModifierType state, gpointer user_data);

/* ===== IRC Formatting Constants ===== */
#define ATTR_BOLD        '\002'
#define ATTR_COLOR       '\003'
#define ATTR_BLINK       '\006'
#define ATTR_BEEP        '\007'
#define ATTR_HIDDEN      '\010'
#define ATTR_TAB         '\011'  /* Tab - used as separator between nick and message */
#define ATTR_RESET       '\017'
#define ATTR_REVERSE     '\026'
#define ATTR_ITALICS     '\035'
#define ATTR_STRIKETHROUGH '\036'
#define ATTR_UNDERLINE   '\037'

/* ===== Color Palette =====
 *
 * The palette matches the GTK2 xtext/palette architecture:
 *   0-15  : mIRC theme colors (customizable per scheme)
 *   16-31 : "Local" colors - mirrors of 0-15, used by text events (%C18 etc.)
 *   32-41 : Special UI colors (see COL_* defines in fe-gtk4.h)
 *
 * Color indices > COL_MAX wrap with (index % MIRC_COLS).
 * mIRC color code 99 is treated as "default" (maps to COL_FG / COL_BG).
 */

/* Default mIRC colors 0-15 (standard mIRC spec, used when no scheme is active) */
static const char *mirc_default_colors[16] = {
	"#FFFFFF", /* 0 white */
	"#000000", /* 1 black */
	"#00007F", /* 2 blue (navy) */
	"#009300", /* 3 green */
	"#FF0000", /* 4 red */
	"#7F0000", /* 5 brown (maroon) */
	"#9C009C", /* 6 purple */
	"#FC7F00", /* 7 orange */
	"#FFFF00", /* 8 yellow */
	"#00FC00", /* 9 light green */
	"#009393", /* 10 cyan (teal) */
	"#00FFFF", /* 11 light cyan (aqua) */
	"#0000FC", /* 12 light blue (royal) */
	"#FF00FF", /* 13 pink (light purple) */
	"#7F7F7F", /* 14 grey */
	"#D2D2D2", /* 15 light grey */
};

/* ===== Color Scheme Definitions =====
 * Ported from src/fe-gtk/palette.c, converted from 16-bit GdkColor to 8-bit CSS hex.
 * Each scheme defines all 42 palette entries:
 *   0-15  : mIRC theme colors
 *   16-31 : Local colors (mirrors of 0-15, used by text events)
 *   32-41 : Special UI colors */

/* Default scheme (Tango-inspired light theme) */
static const char *scheme_default[PALETTE_SIZE] = {
	/* mIRC colors 0-15 */
	"#D3D7CF", /* 0 white */
	"#2E3436", /* 1 black */
	"#3465A4", /* 2 blue */
	"#4E9A06", /* 3 green */
	"#CC0000", /* 4 red */
	"#8F3902", /* 5 brown */
	"#5C3566", /* 6 purple */
	"#CE5C00", /* 7 orange */
	"#C4A000", /* 8 yellow */
	"#73D216", /* 9 light green */
	"#11A879", /* 10 aqua */
	"#58A19D", /* 11 light aqua */
	"#57799E", /* 12 blue */
	"#A04265", /* 13 light purple */
	"#555753", /* 14 grey */
	"#888A85", /* 15 light grey */
	/* Local colors 16-31 (copy of 0-15) */
	"#D3D7CF", /* 16 white */
	"#2E3436", /* 17 black */
	"#3465A4", /* 18 blue */
	"#4E9A06", /* 19 green */
	"#CC0000", /* 20 red */
	"#8F3902", /* 21 light red */
	"#5C3566", /* 22 purple */
	"#CE5C00", /* 23 orange */
	"#C4A000", /* 24 yellow */
	"#73D216", /* 25 green */
	"#11A879", /* 26 aqua */
	"#58A19D", /* 27 light aqua */
	"#57799E", /* 28 blue */
	"#A04265", /* 29 light purple */
	"#555753", /* 30 grey */
	"#888A85", /* 31 light grey */
	/* Special colors 32-41 */
	"#D3D7CF", /* 32 marktext Fore (white) */
	"#204A87", /* 33 marktext Back (blue) */
	"#2529E8", /* 34 foreground */
	"#FAF8F8", /* 35 background */
	"#8F3902", /* 36 marker line (brown) */
	"#3465A4", /* 37 tab New Data (blue) */
	"#4E9A06", /* 38 tab Nick Mentioned (green) */
	"#CE5C00", /* 39 tab New Message (orange) */
	"#888A85", /* 40 away user (grey) */
	"#A40000", /* 41 spell checker (red) */
};

/* Dark scheme */
static const char *scheme_dark[PALETTE_SIZE] = {
	/* mIRC colors 0-15 */
	"#D3D7CF", /* 0 white */
	"#2E3436", /* 1 black */
	"#5799FF", /* 2 blue */
	"#7AC936", /* 3 green */
	"#FF5555", /* 4 red */
	"#CF6A4C", /* 5 light red */
	"#AD7FA8", /* 6 purple */
	"#FFAA00", /* 7 orange */
	"#FFFF55", /* 8 yellow */
	"#55FF55", /* 9 light green */
	"#00D3D3", /* 10 aqua */
	"#8CE8E8", /* 11 light aqua */
	"#5555FF", /* 12 light blue */
	"#FF55FF", /* 13 light purple */
	"#7F7F7F", /* 14 grey */
	"#D0D0D0", /* 15 light grey */
	/* Local colors 16-31 */
	"#D3D7CF", /* 16 white */
	"#2E3436", /* 17 black */
	"#5799FF", /* 18 blue */
	"#7AC936", /* 19 green */
	"#FF5555", /* 20 red */
	"#CF6A4C", /* 21 light red */
	"#AD7FA8", /* 22 purple */
	"#FFAA00", /* 23 orange */
	"#FFFF55", /* 24 yellow */
	"#55FF55", /* 25 light green */
	"#00D3D3", /* 26 aqua */
	"#8CE8E8", /* 27 light aqua */
	"#5555FF", /* 28 light blue */
	"#FF55FF", /* 29 light purple */
	"#7F7F7F", /* 30 grey */
	"#D0D0D0", /* 31 light grey */
	/* Special colors 32-41 */
	"#D3D7CF", /* 32 marktext Fore */
	"#406090", /* 33 marktext Back */
	"#D0D0D0", /* 34 foreground (light grey) */
	"#1E1E2E", /* 35 background (dark) */
	"#FF5555", /* 36 marker line (red) */
	"#5799FF", /* 37 tab New Data (blue) */
	"#7AC936", /* 38 tab Nick Mentioned (green) */
	"#FFAA00", /* 39 tab New Message (orange) */
	"#7F7F7F", /* 40 away user (grey) */
	"#FF5555", /* 41 spell checker (red) */
};

/* Monokai scheme */
static const char *scheme_monokai[PALETTE_SIZE] = {
	/* mIRC colors 0-15 */
	"#F8F8F2", /* 0 white */
	"#272822", /* 1 black */
	"#66D9EF", /* 2 blue */
	"#A6E22E", /* 3 green */
	"#F92672", /* 4 red */
	"#FD971F", /* 5 orange */
	"#AE81FF", /* 6 purple */
	"#FD971F", /* 7 orange */
	"#E6DB74", /* 8 yellow */
	"#A6E22E", /* 9 light green */
	"#A1EFE4", /* 10 aqua */
	"#66D9EF", /* 11 light aqua */
	"#66D9EF", /* 12 light blue */
	"#AE81FF", /* 13 light purple */
	"#75715E", /* 14 grey */
	"#A59F85", /* 15 light grey */
	/* Local colors 16-31 */
	"#F8F8F2", /* 16 white */
	"#272822", /* 17 black */
	"#66D9EF", /* 18 blue */
	"#A6E22E", /* 19 green */
	"#F92672", /* 20 red */
	"#FD971F", /* 21 orange */
	"#AE81FF", /* 22 purple */
	"#FD971F", /* 23 orange */
	"#E6DB74", /* 24 yellow */
	"#A6E22E", /* 25 light green */
	"#A1EFE4", /* 26 aqua */
	"#66D9EF", /* 27 light aqua */
	"#66D9EF", /* 28 light blue */
	"#AE81FF", /* 29 light purple */
	"#75715E", /* 30 grey */
	"#A59F85", /* 31 light grey */
	/* Special colors 32-41 */
	"#F8F8F2", /* 32 marktext Fore */
	"#49483E", /* 33 marktext Back */
	"#F8F8F2", /* 34 foreground */
	"#272822", /* 35 background */
	"#F92672", /* 36 marker line (pink) */
	"#66D9EF", /* 37 tab New Data (blue) */
	"#A6E22E", /* 38 tab Nick Mentioned (green) */
	"#FD971F", /* 39 tab New Message (orange) */
	"#75715E", /* 40 away user (grey) */
	"#F92672", /* 41 spell checker (pink) */
};

/* Solarized Dark scheme */
static const char *scheme_solarized_dark[PALETTE_SIZE] = {
	/* mIRC colors 0-15 */
	"#FDF6E3", /* 0 base3 */
	"#002B36", /* 1 base03 */
	"#268BD2", /* 2 blue */
	"#859900", /* 3 green */
	"#DC322F", /* 4 red */
	"#CB4B16", /* 5 orange */
	"#D33682", /* 6 magenta */
	"#CB4B16", /* 7 orange */
	"#B58900", /* 8 yellow */
	"#859900", /* 9 green */
	"#2AA198", /* 10 cyan */
	"#2AA198", /* 11 cyan */
	"#268BD2", /* 12 blue */
	"#6C71C4", /* 13 violet */
	"#586E75", /* 14 base01 */
	"#839496", /* 15 base0 */
	/* Local colors 16-31 */
	"#FDF6E3", /* 16 */
	"#002B36", /* 17 */
	"#268BD2", /* 18 */
	"#859900", /* 19 */
	"#DC322F", /* 20 */
	"#CB4B16", /* 21 */
	"#D33682", /* 22 */
	"#CB4B16", /* 23 */
	"#B58900", /* 24 */
	"#859900", /* 25 */
	"#2AA198", /* 26 */
	"#2AA198", /* 27 */
	"#268BD2", /* 28 */
	"#6C71C4", /* 29 */
	"#586E75", /* 30 */
	"#839496", /* 31 */
	/* Special colors 32-41 */
	"#93A1A1", /* 32 marktext Fore (base1) */
	"#073642", /* 33 marktext Back (base02) */
	"#839496", /* 34 foreground (base0) */
	"#002B36", /* 35 background (base03) */
	"#DC322F", /* 36 marker line (red) */
	"#268BD2", /* 37 tab New Data (blue) */
	"#859900", /* 38 tab Nick Mentioned (green) */
	"#CB4B16", /* 39 tab New Message (orange) */
	"#586E75", /* 40 away user (base01) */
	"#DC322F", /* 41 spell checker (red) */
};

/* Solarized Light scheme */
static const char *scheme_solarized_light[PALETTE_SIZE] = {
	/* mIRC colors 0-15 */
	"#FDF6E3", /* 0 base3 */
	"#002B36", /* 1 base03 */
	"#268BD2", /* 2 blue */
	"#859900", /* 3 green */
	"#DC322F", /* 4 red */
	"#CB4B16", /* 5 orange */
	"#D33682", /* 6 magenta */
	"#CB4B16", /* 7 orange */
	"#B58900", /* 8 yellow */
	"#859900", /* 9 green */
	"#2AA198", /* 10 cyan */
	"#2AA198", /* 11 cyan */
	"#268BD2", /* 12 blue */
	"#6C71C4", /* 13 violet */
	"#586E75", /* 14 base01 */
	"#839496", /* 15 base0 */
	/* Local colors 16-31 */
	"#FDF6E3", /* 16 */
	"#002B36", /* 17 */
	"#268BD2", /* 18 */
	"#859900", /* 19 */
	"#DC322F", /* 20 */
	"#CB4B16", /* 21 */
	"#D33682", /* 22 */
	"#CB4B16", /* 23 */
	"#B58900", /* 24 */
	"#859900", /* 25 */
	"#2AA198", /* 26 */
	"#2AA198", /* 27 */
	"#268BD2", /* 28 */
	"#6C71C4", /* 29 */
	"#586E75", /* 30 */
	"#839496", /* 31 */
	/* Special colors 32-41 */
	"#586E75", /* 32 marktext Fore (base01) */
	"#EEE8D5", /* 33 marktext Back (base2) */
	"#657B83", /* 34 foreground (base00) */
	"#FDF6E3", /* 35 background (base3) */
	"#DC322F", /* 36 marker line (red) */
	"#268BD2", /* 37 tab New Data (blue) */
	"#859900", /* 38 tab Nick Mentioned (green) */
	"#CB4B16", /* 39 tab New Message (orange) */
	"#93A1A1", /* 40 away user (base1) */
	"#DC322F", /* 41 spell checker (red) */
};

/* Array of scheme pointers (index 0 = Custom = NULL) */
static const char **color_schemes[] = {
	NULL,                    /* 0 = Custom */
	scheme_default,          /* 1 = Default */
	scheme_dark,             /* 2 = Dark */
	scheme_monokai,          /* 3 = Monokai */
	scheme_solarized_dark,   /* 4 = Solarized Dark */
	scheme_solarized_light,  /* 5 = Solarized Light */
};
#define COLOR_SCHEME_COUNT (sizeof(color_schemes) / sizeof(color_schemes[0]))

/* Mutable live color array for the full 42-entry palette.
 * Initialized from mirc_default_colors (0-15) + default scheme (16-41).
 * Overwritten by palette_apply_scheme(). */
static char live_colors[PALETTE_SIZE][8];  /* "#RRGGBB\0" */
static gboolean live_colors_initialized = FALSE;

/* Initialize live_colors from the defaults */
static void
live_colors_init (void)
{
	int i;
	if (live_colors_initialized)
		return;

	/* 0-15: standard mIRC defaults */
	for (i = 0; i < 16; i++)
		g_strlcpy (live_colors[i], mirc_default_colors[i], 8);
	/* 16-31: mirror of 0-15 */
	for (i = 0; i < 16; i++)
		g_strlcpy (live_colors[16 + i], mirc_default_colors[i], 8);
	/* 32-41: special UI colors from the default scheme */
	for (i = 32; i < PALETTE_SIZE; i++)
		g_strlcpy (live_colors[i], scheme_default[i], 8);

	live_colors_initialized = TRUE;
}

/* Get the color string for a given color index.
 * Indices 0-41 come from live_colors[].
 * Index 99 maps to COL_FG (default foreground) / COL_BG (default background).
 * Indices > COL_MAX wrap with (index % 32). */
static const char *
get_color (int index)
{
	if (index < 0)
		return "#000000";
	if (index == 99)
		return live_colors[COL_FG];  /* mIRC "default" color */
	if (index > COL_MAX)
		index = index % MIRC_COLS;
	live_colors_init ();
	return live_colors[index];
}

/* Apply a color scheme to the full palette (all 42 entries).
 * scheme 0 = Custom (no-op), 1-5 = predefined schemes. */
void
palette_apply_scheme (int scheme)
{
	int i;
	const char **scheme_colors;

	if (scheme <= 0 || scheme >= (int)COLOR_SCHEME_COUNT)
		return;

	scheme_colors = color_schemes[scheme];
	if (!scheme_colors)
		return;

	live_colors_init ();
	for (i = 0; i < PALETTE_SIZE; i++)
		g_strlcpy (live_colors[i], scheme_colors[i], 8);
}

/* Get the current color string for a given palette index.
 * Returns a "#RRGGBB" string. */
const char *
palette_get_color (int index)
{
	return get_color (index);
}

/* Set a single live color to a new "#RRGGBB" value and refresh all
 * open sessions so the change is visible immediately.
 * For mIRC colors 0-15, also mirrors the change to 16-31. */
void
palette_set_color (int index, const char *hex_color)
{
	if (index < 0 || index >= PALETTE_SIZE || !hex_color)
		return;

	live_colors_init ();
	g_strlcpy (live_colors[index], hex_color, 8);

	/* Keep 16-31 in sync with 0-15 */
	if (index < 16)
		g_strlcpy (live_colors[index + 16], hex_color, 8);
	else if (index >= 16 && index < 32)
		g_strlcpy (live_colors[index - 16], hex_color, 8);

	palette_refresh_all ();
}

/* Load palette from colors.conf (compatible with GTK2 format).
 * File format: "color_N = RRRR GGGG BBBB" where values are 16-bit hex.
 * Indices 0-31 map directly; special colors 32-41 are stored as 256-265. */
void
palette_load (void)
{
	int i, j, fh;
	char prefname[256];
	struct stat st;
	char *cfg;
	guint16 red, green, blue;

	live_colors_init ();

	fh = hexchat_open_file ("colors.conf", O_RDONLY, 0, 0);
	if (fh == -1)
		return;

	if (fstat (fh, &st) != 0 || st.st_size == 0)
	{
		close (fh);
		return;
	}

	cfg = g_malloc0 (st.st_size + 1);
	if (read (fh, cfg, st.st_size) < 0)
	{
		g_free (cfg);
		close (fh);
		return;
	}

	/* mIRC colors 0-31 */
	for (i = 0; i < 32; i++)
	{
		g_snprintf (prefname, sizeof prefname, "color_%d", i);
		if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			g_snprintf (live_colors[i], 8, "#%02X%02X%02X",
			            (unsigned)(red >> 8), (unsigned)(green >> 8), (unsigned)(blue >> 8));
	}

	/* Special colors 32-41 are stored at indices 256+ */
	for (i = 256, j = 32; j <= COL_MAX; i++, j++)
	{
		g_snprintf (prefname, sizeof prefname, "color_%d", i);
		if (cfg_get_color (cfg, prefname, &red, &green, &blue))
			g_snprintf (live_colors[j], 8, "#%02X%02X%02X",
			            (unsigned)(red >> 8), (unsigned)(green >> 8), (unsigned)(blue >> 8));
	}

	g_free (cfg);
	close (fh);
}

/* Save palette to colors.conf (compatible with GTK2 format).
 * Converts 8-bit CSS hex (#RRGGBB) to 16-bit values for cfg_put_color. */
void
palette_save (void)
{
	int i, j, fh;
	char prefname[256];
	unsigned int r8, g8, b8;

	live_colors_init ();

	fh = hexchat_open_file ("colors.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh == -1)
		return;

	/* mIRC colors 0-31 */
	for (i = 0; i < 32; i++)
	{
		g_snprintf (prefname, sizeof prefname, "color_%d", i);
		if (sscanf (live_colors[i], "#%02X%02X%02X", &r8, &g8, &b8) == 3)
			cfg_put_color (fh, (guint16)(r8 * 257), (guint16)(g8 * 257), (guint16)(b8 * 257), prefname);
	}

	/* Special colors 32-41 stored at indices 256+ */
	for (i = 256, j = 32; j <= COL_MAX; i++, j++)
	{
		g_snprintf (prefname, sizeof prefname, "color_%d", i);
		if (sscanf (live_colors[j], "#%02X%02X%02X", &r8, &g8, &b8) == 3)
			cfg_put_color (fh, (guint16)(r8 * 257), (guint16)(g8 * 257), (guint16)(b8 * 257), prefname);
	}

	close (fh);
}

/* Update existing text buffer tags to reflect the current live_colors.
 * Called after palette_apply_scheme() to make changes visible.
 * Uses the "foreground"/"background" string properties (same as tag
 * creation in fe_gtk4_init_tags) so that GTK properly invalidates
 * the text display cache. */
static void
update_buffer_tags (GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	GtkTextTag *tag;
	char tag_name[32];
	guint i;

	if (!buffer)
		return;

	table = gtk_text_buffer_get_tag_table (buffer);

	/* Update mIRC color tags 0-31 */
	for (i = 0; i < MIRC_COLS; i++)
	{
		const char *color_str = get_color (i);

		g_snprintf (tag_name, sizeof (tag_name), "fg-%02u", i);
		tag = gtk_text_tag_table_lookup (table, tag_name);
		if (tag)
			g_object_set (tag, "foreground", color_str, NULL);

		g_snprintf (tag_name, sizeof (tag_name), "bg-%02u", i);
		tag = gtk_text_tag_table_lookup (table, tag_name);
		if (tag)
			g_object_set (tag, "background", color_str, NULL);
	}

	/* Update special tags that use palette colors */
	tag = gtk_text_tag_table_lookup (table, "reverse");
	if (tag)
		g_object_set (tag,
		              "foreground", get_color (COL_BG),
		              "background", get_color (COL_FG),
		              NULL);

	tag = gtk_text_tag_table_lookup (table, "highlight");
	if (tag)
		g_object_set (tag, "background", get_color (COL_HILIGHT), NULL);

	tag = gtk_text_tag_table_lookup (table, "marker-line");
	if (tag)
		g_object_set (tag, "paragraph-background", get_color (COL_MARKER), NULL);
}

/* Global CSS provider for palette-driven colors (text views, input entries).
 * Uses CSS classes instead of deprecated per-widget style contexts. */
static GtkCssProvider *palette_css_provider = NULL;

/* (Re-)generate the global palette CSS and load it into the provider.
 * Must be called after any palette change. */
static void
palette_update_css (void)
{
	char *css;

	if (!palette_css_provider)
	{
		palette_css_provider = gtk_css_provider_new ();
		gtk_style_context_add_provider_for_display (
			gdk_display_get_default (),
			GTK_STYLE_PROVIDER (palette_css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	css = g_strdup_printf (
		".hexchat-textview text {"
		"  color: %s;"
		"  background-color: %s;"
		"}"
		".hexchat-input {"
		"  color: %s;"
		"  background-color: %s;"
		"}"
		".hexchat-tab-newdata {"
		"  color: %s;"
		"}"
		".hexchat-tab-newmsg {"
		"  color: %s;"
		"}"
		".hexchat-tab-hilight {"
		"  color: %s;"
		"  font-weight: bold;"
		"}"
		".hexchat-nick-away {"
		"  color: %s;"
		"}"
		".hexchat-search-bar {"
		"  padding: 4px 8px;"
		"  background-color: alpha(%s, 0.05);"
		"  border-top: 1px solid alpha(%s, 0.15);"
		"}",
		get_color (COL_FG), get_color (COL_BG),
		get_color (COL_FG), get_color (COL_BG),
		get_color (COL_NEW_DATA),
		get_color (COL_NEW_MSG),
		get_color (COL_HILIGHT),
		get_color (COL_AWAY),
		get_color (COL_FG),
		get_color (COL_FG));

	gtk_css_provider_load_from_string (palette_css_provider, css);
	g_free (css);
}

/* Refresh color tags on all open sessions */
void
palette_refresh_all (void)
{
	GSList *list;
	session *sess;
	session_gui *gui;

	/* Update the global CSS provider (applies to all text views and inputs) */
	palette_update_css ();

	for (list = sess_list; list; list = list->next)
	{
		sess = list->data;
		if (sess && sess->gui)
		{
			gui = sess->gui;
			if (gui->text_buffer)
				update_buffer_tags (gui->text_buffer);
			if (gui->text_view && GTK_IS_WIDGET (gui->text_view))
				gtk_widget_queue_draw (gui->text_view);
		}
	}
}

/* ===== User List Item GObject ===== */

/* Simple GObject to wrap user data for GListStore */
#define USER_ITEM_TYPE (user_item_get_type ())
G_DECLARE_FINAL_TYPE (UserItem, user_item, USER, ITEM, GObject)

struct _UserItem
{
	GObject parent_instance;
	char *nick;
	char prefix[2];
	gboolean is_op;
	gboolean is_hop;
	gboolean is_voice;
	gboolean is_away;
	struct User *user;  /* Pointer to the real User struct */
};

G_DEFINE_TYPE (UserItem, user_item, G_TYPE_OBJECT)

static void
user_item_finalize (GObject *object)
{
	UserItem *self = USER_ITEM (object);
	g_free (self->nick);
	G_OBJECT_CLASS (user_item_parent_class)->finalize (object);
}

static void
user_item_class_init (UserItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = user_item_finalize;
}

static void
user_item_init (UserItem *self)
{
	self->nick = NULL;
	self->prefix[0] = '\0';
	self->prefix[1] = '\0';
	self->is_op = FALSE;
	self->is_hop = FALSE;
	self->is_voice = FALSE;
	self->is_away = FALSE;
	self->user = NULL;
}

static UserItem *
user_item_new (struct User *user)
{
	UserItem *item = g_object_new (USER_ITEM_TYPE, NULL);
	item->nick = g_strdup (user->nick);
	item->prefix[0] = user->prefix[0];
	item->prefix[1] = user->prefix[1];
	item->is_op = user->op;
	item->is_hop = user->hop;
	item->is_voice = user->voice;
	item->is_away = user->away;
	item->user = user;
	return item;
}

static void
user_item_update (UserItem *item, struct User *user)
{
	g_free (item->nick);
	item->nick = g_strdup (user->nick);
	item->prefix[0] = user->prefix[0];
	item->prefix[1] = user->prefix[1];
	item->is_op = user->op;
	item->is_hop = user->hop;
	item->is_voice = user->voice;
	item->is_away = user->away;
	item->user = user;
}

/* ===== Command-line argument parsing ===== */

static char *arg_cfgdir = NULL;
static gint arg_show_autoload = 0;
static gint arg_show_config = 0;
static gint arg_show_version = 0;

static const GOptionEntry gopt_entries[] = 
{
	{"no-auto",    'a', 0, G_OPTION_ARG_NONE,   &arg_dont_autoconnect, N_("Don't auto connect to servers"), NULL},
	{"cfgdir",     'd', 0, G_OPTION_ARG_STRING, &arg_cfgdir, N_("Use a different config directory"), "PATH"},
	{"no-plugins", 'n', 0, G_OPTION_ARG_NONE,   &arg_skip_plugins, N_("Don't auto load any plugins"), NULL},
	{"plugindir",  'p', 0, G_OPTION_ARG_NONE,   &arg_show_autoload, N_("Show plugin/script auto-load directory"), NULL},
	{"configdir",  'u', 0, G_OPTION_ARG_NONE,   &arg_show_config, N_("Show user config directory"), NULL},
	{"url",         0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_url, N_("Open an irc://server:port/channel URL"), "URL"},
	{"version",    'v', 0, G_OPTION_ARG_NONE,   &arg_show_version, N_("Show version information"), NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &arg_urls, N_("Open an irc://server:port/channel?key URL"), "URL"},
	{NULL}
};

int
fe_args (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, gopt_entries, GETTEXT_PACKAGE);
	g_option_context_parse (context, &argc, &argv, &error);

	if (error)
	{
		if (error->message)
			printf ("%s\n", error->message);
		return 1;
	}

	g_option_context_free (context);

	if (arg_show_version)
	{
		printf (PACKAGE_NAME " " PACKAGE_VERSION " (GTK4/libadwaita)\n");
		return 0;
	}

	if (arg_show_autoload)
	{
#ifndef USE_PLUGIN
		printf (PACKAGE_NAME " was built without plugin support\n");
		return 1;
#else
		printf ("%s\n", HEXCHATLIBDIR);
#endif
		return 0;
	}

	if (arg_show_config)
	{
		printf ("%s\n", get_xdir ());
		return 0;
	}

	if (arg_cfgdir)
	{
		g_free (xdir);
		xdir = g_strdup (arg_cfgdir);
		if (xdir[strlen (xdir) - 1] == '/')
			xdir[strlen (xdir) - 1] = 0;
		g_free (arg_cfgdir);
	}

	return -1;
}

/* ===== Application callbacks ===== */

static void
on_activate (GApplication *app, gpointer user_data)
{
	/* Just present the window - it's already created in fe_init() */
	if (main_window)
		gtk_window_present (GTK_WINDOW (main_window));
}

/* ===== Menu Actions ===== */

static void
action_server_list (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	servlist_open (current_sess);
}

static void
action_disconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (current_sess && current_sess->server)
	{
		current_sess->server->disconnect (current_sess, TRUE, -1);
	}
}

static void
action_reconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (current_sess && current_sess->server)
	{
		handle_command (current_sess, "reconnect", FALSE);
	}
}

static void
action_channel_list (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (current_sess && current_sess->server)
	{
		chanlist_opengui (current_sess->server, TRUE);
	}
}

static void
action_new_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	new_ircwindow (NULL, NULL, SESS_SERVER, 0);
}

static void
action_close_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (current_sess)
	{
		fe_close_window (current_sess);
	}
}

static void
action_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	hexchat_exit ();
}

static void
action_preferences (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	prefs_show (GTK_WINDOW (main_window));
}

static void
action_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	AdwDialog *about;
	
	about = adw_about_dialog_new ();
	adw_about_dialog_set_application_name (ADW_ABOUT_DIALOG (about), PACKAGE_NAME);
	adw_about_dialog_set_version (ADW_ABOUT_DIALOG (about), PACKAGE_VERSION);
	adw_about_dialog_set_comments (ADW_ABOUT_DIALOG (about), "IRC client for GTK4");
	adw_about_dialog_set_website (ADW_ABOUT_DIALOG (about), "https://hexchat.github.io");
	adw_about_dialog_set_license_type (ADW_ABOUT_DIALOG (about), GTK_LICENSE_GPL_2_0);
	
	adw_dialog_present (about, main_window);
}

static const GActionEntry app_actions[] = {
	{ "server-list", action_server_list, NULL, NULL, NULL },
	{ "disconnect", action_disconnect, NULL, NULL, NULL },
	{ "reconnect", action_reconnect, NULL, NULL, NULL },
	{ "channel-list", action_channel_list, NULL, NULL, NULL },
	{ "new-tab", action_new_tab, NULL, NULL, NULL },
	{ "close-tab", action_close_tab, NULL, NULL, NULL },
	{ "preferences", action_preferences, NULL, NULL, NULL },
	{ "quit", action_quit, NULL, NULL, NULL },
	{ "about", action_about, NULL, NULL, NULL },
};

/* Create the application menu */
static GMenuModel *
create_app_menu (void)
{
	GMenu *menu;
	GMenu *section;

	menu = g_menu_new ();

	/* HexChat section */
	section = g_menu_new ();
	g_menu_append (section, "Network List...", "app.server-list");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	/* Server section */
	section = g_menu_new ();
	g_menu_append (section, "Disconnect", "app.disconnect");
	g_menu_append (section, "Reconnect", "app.reconnect");
	g_menu_append (section, "Channel List...", "app.channel-list");
	g_menu_append_section (menu, "Server", G_MENU_MODEL (section));
	g_object_unref (section);

	/* Window section */
	section = g_menu_new ();
	g_menu_append (section, "New Server Tab", "app.new-tab");
	g_menu_append (section, "Close Tab", "app.close-tab");
	g_menu_append_section (menu, "Window", G_MENU_MODEL (section));
	g_object_unref (section);

	/* Settings section */
	section = g_menu_new ();
	g_menu_append (section, "Preferences", "app.preferences");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	/* About section */
	section = g_menu_new ();
	g_menu_append (section, "About HexChat", "app.about");
	g_menu_append (section, "Quit", "app.quit");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	return G_MENU_MODEL (menu);
}

/* Track previously selected session for marker line handling */
static session *prev_selected_sess = NULL;

/* Get session from a sidebar row */
static session *
get_session_from_row (GtkListBoxRow *row)
{
	GSList *list;
	session *sess;

	if (!row)
		return NULL;

	for (list = sess_list; list; list = list->next)
	{
		sess = list->data;
		if (sess && sess->gui && ((session_gui *)sess->gui)->sidebar_row == GTK_WIDGET (row))
			return sess;
	}
	return NULL;
}

/* ===== Sidebar right-click context menu ===== */

/* Close/leave action triggered from the sidebar context menu.
 * The target session pointer is stored on the popover via g_object_set_data. */
static void
sidebar_context_close_cb (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkPopover *popover = GTK_POPOVER (user_data);
	session *sess;

	sess = g_object_get_data (G_OBJECT (popover), "hexchat-session");
	if (sess)
		fe_close_window (sess);
}

/* Right-click handler for sidebar rows.
 * Creates a GtkPopoverMenu with a "Close" item anchored to the click point. */
static void
sidebar_row_right_click_cb (GtkGestureClick *gesture, int n_press,
                            double x, double y, gpointer user_data)
{
	GtkWidget *row = GTK_WIDGET (user_data);
	session *sess;
	GMenu *menu;
	GMenu *section;
	GtkWidget *popover;
	GSimpleActionGroup *group;
	GSimpleAction *close_action;
	GdkRectangle rect;

	sess = get_session_from_row (GTK_LIST_BOX_ROW (row));
	if (!sess)
		return;

	/* Build the menu model */
	menu = g_menu_new ();
	section = g_menu_new ();

	if (sess->type == SESS_CHANNEL)
		g_menu_append (section, _("Leave Channel"), "ctx.close-tab");
	else if (sess->type == SESS_DIALOG)
		g_menu_append (section, _("Close Dialog"), "ctx.close-tab");
	else
		g_menu_append (section, _("Close"), "ctx.close-tab");

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	/* Create the popover */
	popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
	g_object_unref (menu);
	gtk_widget_set_parent (popover, row);

	/* Position at the click point */
	rect.x = (int)x;
	rect.y = (int)y;
	rect.width = 1;
	rect.height = 1;
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

	/* Stash the session on the popover so the action callback can find it */
	g_object_set_data (G_OBJECT (popover), "hexchat-session", sess);

	/* Create a local action group scoped to this popover */
	group = g_simple_action_group_new ();
	close_action = g_simple_action_new ("close-tab", NULL);
	g_signal_connect (close_action, "activate",
	                  G_CALLBACK (sidebar_context_close_cb), popover);
	g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (close_action));
	g_object_unref (close_action);
	gtk_widget_insert_action_group (popover, "ctx", G_ACTION_GROUP (group));
	g_object_unref (group);

	/* Clean up the popover when it is closed */
	g_signal_connect (popover, "closed",
	                  G_CALLBACK (gtk_widget_unparent), NULL);

	gtk_popover_popup (GTK_POPOVER (popover));
}

/* Callback when sidebar selection changes */
static void
sidebar_row_selected_cb (GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data)
{
	session *new_sess;

	new_sess = get_session_from_row (row);

	/* Set marker on the previous session before switching */
	if (prev_selected_sess && prev_selected_sess != new_sess)
	{
		set_marker_line (prev_selected_sess);
		draw_marker_line (prev_selected_sess);
	}

	/* Clear marker on the new session (user is now viewing it) */
	if (new_sess && new_sess->gui)
	{
		session_gui *gui = new_sess->gui;
		clear_marker_line (new_sess);

		/* Switch the visible child in the content stack */
		if (content_stack && gui->content_box)
		{
			gtk_stack_set_visible_child (GTK_STACK (content_stack), gui->content_box);
		}

		current_sess = new_sess;
		current_tab = new_sess;
		if (new_sess->server)
			new_sess->server->front_session = new_sess;

		/* Update window title for the new session */
		fe_set_title (new_sess);

		/* Clear tab color since the user is now viewing this tab */
		fe_set_tab_color (new_sess, 0);

		/* Scroll text view to the end so the user sees the latest messages */
		if (gui->text_view && gui->text_buffer)
		{
			GtkTextIter end_iter;
			GtkTextMark *end_mark;

			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			end_mark = gtk_text_buffer_create_mark (gui->text_buffer, NULL, &end_iter, FALSE);
			gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (gui->text_view), end_mark,
			                              0.0, TRUE, 0.0, 1.0);
			gtk_text_buffer_delete_mark (gui->text_buffer, end_mark);
		}

		/* Focus the input entry */
		if (gui->input_entry)
			gtk_widget_grab_focus (gui->input_entry);
	}

	prev_selected_sess = new_sess;
}

/* Handle main window close request */
static gboolean
main_window_close_cb (GtkWindow *window, gpointer user_data)
{
	/* Quit the application */
	hexchat_exit ();
	return FALSE;  /* Allow the close to proceed */
}

/* Window-level key press handler for Ctrl+F and Ctrl+G shortcuts */
static gboolean
window_key_pressed_cb (GtkEventControllerKey *controller,
                       guint keyval,
                       guint keycode,
                       GdkModifierType state,
                       gpointer user_data)
{
	if (!(state & GDK_CONTROL_MASK))
		return FALSE;

	if (keyval == GDK_KEY_f)
	{
		if (current_sess)
			search_toggle (current_sess);
		return TRUE;
	}

	if (keyval == GDK_KEY_g || keyval == GDK_KEY_G)
	{
		if (current_sess && current_sess->gui)
		{
			session_gui *gui = current_sess->gui;
			if (gui->search_bar && gtk_widget_get_visible (gui->search_bar))
			{
				search_do (current_sess, (state & GDK_SHIFT_MASK) != 0);
				return TRUE;
			}
		}
	}

	return FALSE;
}

/* Create the main window and UI structure */
static void
create_main_window (void)
{
	GMenuModel *menu_model;
	GtkWidget *menu_button;

	/* Register application actions */
	g_action_map_add_action_entries (G_ACTION_MAP (hexchat_app),
	                                 app_actions, G_N_ELEMENTS (app_actions),
	                                 NULL);

	/* Create main window */
	main_window = adw_application_window_new (GTK_APPLICATION (hexchat_app));
	gtk_window_set_title (GTK_WINDOW (main_window), PACKAGE_NAME);
	gtk_window_set_default_size (GTK_WINDOW (main_window), 900, 600);

	/* Connect close request to quit the application */
	g_signal_connect (main_window, "close-request", G_CALLBACK (main_window_close_cb), NULL);

	/* Create toolbar view as main container */
	GtkWidget *toolbar_view = adw_toolbar_view_new ();
	adw_application_window_set_content (ADW_APPLICATION_WINDOW (main_window), toolbar_view);

	/* Create header bar */
	GtkWidget *header = adw_header_bar_new ();
	GtkWidget *title = adw_window_title_new (PACKAGE_NAME, "");
	adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), title);
	adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header);

	/* Create hamburger menu button */
	menu_model = create_app_menu ();
	menu_button = gtk_menu_button_new ();
	gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (menu_button), "open-menu-symbolic");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_button), menu_model);
	gtk_widget_set_tooltip_text (menu_button, "Main Menu");
	adw_header_bar_pack_end (ADW_HEADER_BAR (header), menu_button);
	g_object_unref (menu_model);

	/* Create 3-pane layout: sidebar | content (chat + userlist) */
	GtkWidget *main_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

	/* Left pane: channel sidebar (GtkListBox in a scrolled window) */
	GtkWidget *sidebar_scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar_scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (sidebar_scroll, 160, -1);

	channel_sidebar = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (channel_sidebar),
	                                 GTK_SELECTION_SINGLE);
	g_signal_connect (channel_sidebar, "row-selected",
	                  G_CALLBACK (sidebar_row_selected_cb), NULL);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sidebar_scroll),
	                               channel_sidebar);

	/* Right pane: content stack (one child per session) */
	content_stack = gtk_stack_new ();
	gtk_stack_set_transition_type (GTK_STACK (content_stack),
	                               GTK_STACK_TRANSITION_TYPE_NONE);
	gtk_widget_set_hexpand (content_stack, TRUE);
	gtk_widget_set_vexpand (content_stack, TRUE);

	gtk_paned_set_start_child (GTK_PANED (main_paned), sidebar_scroll);
	gtk_paned_set_end_child (GTK_PANED (main_paned), content_stack);
	gtk_paned_set_resize_start_child (GTK_PANED (main_paned), FALSE);
	gtk_paned_set_resize_end_child (GTK_PANED (main_paned), TRUE);
	gtk_paned_set_shrink_start_child (GTK_PANED (main_paned), FALSE);
	gtk_paned_set_shrink_end_child (GTK_PANED (main_paned), FALSE);
	gtk_paned_set_position (GTK_PANED (main_paned), 180);

	/* Set paned layout as main content */
	adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), main_paned);

	/* Window-level key controller for Ctrl+F (search) and Ctrl+G (next/prev) */
	{
		GtkEventController *win_key = gtk_event_controller_key_new ();
		g_signal_connect (win_key, "key-pressed",
		                  G_CALLBACK (window_key_pressed_cb), NULL);
		gtk_widget_add_controller (main_window, win_key);
	}
}

void
fe_init (void)
{
	GError *error = NULL;

	/* Initialize GTK explicitly */
	gtk_init ();

	/* Initialize libadwaita */
	adw_init ();

	/* Initialize the application - use NON_UNIQUE to avoid single-instance behavior
	 * that would interfere with HexChat's own instance handling */
	hexchat_app = ADW_APPLICATION (adw_application_new ("io.github.Hexchat.gtk4",
	                                                    G_APPLICATION_NON_UNIQUE));

	g_signal_connect (hexchat_app, "activate", G_CALLBACK (on_activate), NULL);

	/* Register the application so we can create windows */
	if (!g_application_register (G_APPLICATION (hexchat_app), NULL, &error))
	{
		g_warning ("Failed to register application: %s", error ? error->message : "unknown");
		if (error)
			g_error_free (error);
		return;
	}

	/* Create the main window now, before sessions are created */
	create_main_window ();

	/* Load custom palette from colors.conf (if it exists).
	 * Must be called before palette_apply_scheme so saved colors
	 * are the base and a scheme selection overrides them. */
	palette_load ();

	/* Apply saved color scheme so that sessions created after this
	 * point will use the correct palette in fe_gtk4_init_tags(). */
	if (prefs.hex_gui_color_scheme > 0)
		palette_apply_scheme (prefs.hex_gui_color_scheme);
}

void
fe_main (void)
{
	/* Present the window */
	if (main_window)
		gtk_window_present (GTK_WINDOW (main_window));

	/* Run the GLib main loop - this handles all GTK events properly */
	while (!done)
		g_main_context_iteration (NULL, TRUE);
}

void
fe_exit (void)
{
	done = TRUE;
}

void
fe_cleanup (void)
{
	if (hexchat_app)
	{
		g_object_unref (hexchat_app);
		hexchat_app = NULL;
	}
}

/* ===== Timer and I/O functions ===== */

void
fe_timeout_remove (int tag)
{
	g_source_remove (tag);
}

int
fe_timeout_add (int interval, void *callback, void *userdata)
{
	return g_timeout_add (interval, (GSourceFunc) callback, userdata);
}

int
fe_timeout_add_seconds (int interval, void *callback, void *userdata)
{
	return g_timeout_add_seconds (interval, (GSourceFunc) callback, userdata);
}

void
fe_input_remove (int tag)
{
	g_source_remove (tag);
}

int
fe_input_add (int sok, int flags, void *func, void *data)
{
	int tag, type = 0;
	GIOChannel *channel;

	channel = g_io_channel_unix_new (sok);

	if (flags & FIA_READ)
		type |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (flags & FIA_WRITE)
		type |= G_IO_OUT | G_IO_ERR;
	if (flags & FIA_EX)
		type |= G_IO_PRI;

	tag = g_io_add_watch (channel, type, (GIOFunc) func, data);
	g_io_channel_unref (channel);

	return tag;
}

void
fe_idle_add (void *func, void *data)
{
	g_idle_add (func, data);
}

/* ===== IRC color parsing helpers ===== */

/* Initialize text tags for IRC formatting */
void
fe_gtk4_init_tags (GtkTextBuffer *buffer)
{
	guint i;
	char tag_name[32];

	/* mIRC color tags (0-31) - 0-15 themed, 16-31 mirrors */
	live_colors_init ();
	for (i = 0; i < MIRC_COLS; i++)
	{
		const char *color_str = get_color (i);

		g_snprintf (tag_name, sizeof (tag_name), "fg-%02u", i);
		gtk_text_buffer_create_tag (buffer, tag_name,
		                            "foreground", color_str,
		                            NULL);

		g_snprintf (tag_name, sizeof (tag_name), "bg-%02u", i);
		gtk_text_buffer_create_tag (buffer, tag_name,
		                            "background", color_str,
		                            NULL);
	}

	/* Formatting tags */
	gtk_text_buffer_create_tag (buffer, "bold",
	                            "weight", PANGO_WEIGHT_BOLD,
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "italic",
	                            "style", PANGO_STYLE_ITALIC,
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "underline",
	                            "underline", PANGO_UNDERLINE_SINGLE,
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "strikethrough",
	                            "strikethrough", TRUE,
	                            NULL);
	/* Note: "hidden" in HexChat's xtext means "not selectable for copy" but still visible.
	 * In GtkTextView we don't have easy equivalent, so we just don't hide it visually.
	 * Create the tag but with no special properties. */
	gtk_text_buffer_create_tag (buffer, "hidden",
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "reverse",
	                            "foreground", get_color (COL_BG),
	                            "background", get_color (COL_FG),
	                            NULL);

	/* Special tags - use palette colors where appropriate */
	gtk_text_buffer_create_tag (buffer, "timestamp",
	                            "foreground", "#888888",
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "url",
	                            "foreground", "#0000FF",
	                            "underline", PANGO_UNDERLINE_SINGLE,
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "highlight",
	                            "background", get_color (COL_HILIGHT),
	                            NULL);
	/* Marker line tag */
	gtk_text_buffer_create_tag (buffer, "marker-line",
	                            "paragraph-background", get_color (COL_MARKER),
	                            "pixels-above-lines", 2,
	                            "pixels-below-lines", 2,
	                            NULL);

	/* Search highlight tags */
	gtk_text_buffer_create_tag (buffer, "search-highlight",
	                            "background", get_color (COL_MARK_BG),
	                            "foreground", get_color (COL_MARK_FG),
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "search-current",
	                            "background", get_color (COL_MARK_BG),
	                            "foreground", get_color (COL_MARK_FG),
	                            "underline", PANGO_UNDERLINE_SINGLE,
	                            NULL);
}

/* ===== URL detection and handling ===== */

/* Extract the URL string from text at the given tag range */
static char *
extract_url_at_iter (GtkTextBuffer *buffer, GtkTextIter *iter)
{
	GtkTextIter start, end;
	GtkTextTag *url_tag;
	char *url;

	url_tag = gtk_text_tag_table_lookup (
		gtk_text_buffer_get_tag_table (buffer), "url");
	if (!url_tag)
		return NULL;

	if (!gtk_text_iter_has_tag (iter, url_tag))
		return NULL;

	/* Find the extent of the URL tag */
	start = *iter;
	if (!gtk_text_iter_starts_tag (&start, url_tag))
		gtk_text_iter_backward_to_tag_toggle (&start, url_tag);

	end = *iter;
	if (!gtk_text_iter_ends_tag (&end, url_tag))
		gtk_text_iter_forward_to_tag_toggle (&end, url_tag);

	url = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	return url;
}

/* Open a URL with proper protocol handling */
static void
open_url (const char *url)
{
	int url_type;
	char *uri;

	if (!url || !*url)
		return;

	url_type = url_check_word (url);

	/* gvfs likes file:// */
	if (url_type == WORD_PATH)
	{
		uri = g_strconcat ("file://", url, NULL);
		fe_open_url (uri);
		g_free (uri);
	}
	/* IPv6 addr. Add http:// */
	else if (url_type == WORD_HOST6)
	{
		/* IPv6 addrs in urls should be enclosed in [ ] */
		if (*url != '[')
			uri = g_strdup_printf ("http://[%s]", url);
		else
			uri = g_strdup_printf ("http://%s", url);

		fe_open_url (uri);
		g_free (uri);
	}
	/* the http:// part's missing, prepend it */
	else if (strchr (url, ':') == NULL)
	{
		uri = g_strdup_printf ("http://%s", url);
		fe_open_url (uri);
		g_free (uri);
	}
	/* we have a sane URL, send it to the browser untouched */
	else
	{
		fe_open_url (url);
	}
}

/* Handle click on text view - check for URL clicks */
static void
text_view_click_cb (GtkGestureClick *gesture,
                    gint n_press,
                    gdouble x,
                    gdouble y,
                    gpointer user_data)
{
	session *sess = user_data;
	session_gui *gui;
	GtkTextView *text_view;
	GtkTextIter iter;
	int buffer_x, buffer_y;
	char *url;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	text_view = GTK_TEXT_VIEW (gui->text_view);

	/* Only handle left-click (button 1) */
	if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) != 1)
		return;

	/* Convert widget coords to buffer coords */
	gtk_text_view_window_to_buffer_coords (text_view,
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       (int)x, (int)y,
	                                       &buffer_x, &buffer_y);

	/* Get the iter at the click position */
	gtk_text_view_get_iter_at_location (text_view, &iter, buffer_x, buffer_y);

	/* Check if there's a URL at this position */
	url = extract_url_at_iter (gui->text_buffer, &iter);
	if (url)
	{
		open_url (url);
		g_free (url);
	}
}

/* Update cursor when hovering over URLs */
static void
text_view_motion_cb (GtkEventControllerMotion *controller,
                     gdouble x,
                     gdouble y,
                     gpointer user_data)
{
	session *sess = user_data;
	session_gui *gui;
	GtkTextView *text_view;
	GtkTextIter iter;
	GtkTextTag *url_tag;
	int buffer_x, buffer_y;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	text_view = GTK_TEXT_VIEW (gui->text_view);

	/* Convert widget coords to buffer coords */
	gtk_text_view_window_to_buffer_coords (text_view,
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       (int)x, (int)y,
	                                       &buffer_x, &buffer_y);

	/* Get the iter at the hover position */
	gtk_text_view_get_iter_at_location (text_view, &iter, buffer_x, buffer_y);

	/* Check if there's a URL tag at this position */
	url_tag = gtk_text_tag_table_lookup (
		gtk_text_buffer_get_tag_table (gui->text_buffer), "url");

	if (url_tag && gtk_text_iter_has_tag (&iter, url_tag))
	{
		gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "pointer");
	}
	else
	{
		gtk_widget_set_cursor (GTK_WIDGET (text_view), NULL);
	}
}

/* Scan a line for URLs and apply the url tag.
 * This strips IRC formatting codes while scanning, then applies tags
 * to the buffer at the appropriate positions. */
static void
apply_url_tags_to_line (GtkTextBuffer *buffer, int line_start_offset, const char *text)
{
	const char *p;
	const char *word_start;
	char *word;
	int word_start_offset;
	int text_offset;  /* offset in the plain-text (stripped) version */
	GtkTextIter start_iter, end_iter;

	if (!text || !*text)
		return;

	/* We need to scan through the original text, tracking the position
	 * in the stripped text (which is what's actually in the buffer) */
	p = text;
	text_offset = 0;

	while (*p)
	{
		/* Skip IRC formatting codes (they aren't in the buffer) */
		switch (*p)
		{
		case ATTR_BOLD:
		case ATTR_ITALICS:
		case ATTR_UNDERLINE:
		case ATTR_STRIKETHROUGH:
		case ATTR_HIDDEN:
		case ATTR_REVERSE:
		case ATTR_RESET:
		case ATTR_BEEP:
		case ATTR_BLINK:
			p++;
			continue;

		case ATTR_COLOR:
			p++;
			/* Skip color digits */
			if (g_ascii_isdigit (*p))
			{
				p++;
				if (g_ascii_isdigit (*p))
					p++;
				if (*p == ',')
				{
					p++;
					if (g_ascii_isdigit (*p))
					{
						p++;
						if (g_ascii_isdigit (*p))
							p++;
					}
				}
			}
			continue;
		}

		/* Skip whitespace */
		if (g_ascii_isspace (*p))
		{
			p++;
			text_offset++;
			continue;
		}

		/* Found start of a word - collect it */
		word_start = p;
		word_start_offset = text_offset;

		/* Find end of word (non-whitespace, non-format-code) */
		while (*p && !g_ascii_isspace (*p) && *p != ATTR_TAB)
		{
			/* Check for format codes embedded in word */
			if (*p == ATTR_BOLD || *p == ATTR_ITALICS ||
			    *p == ATTR_UNDERLINE || *p == ATTR_STRIKETHROUGH ||
			    *p == ATTR_HIDDEN || *p == ATTR_REVERSE || *p == ATTR_RESET ||
			    *p == ATTR_BEEP || *p == ATTR_BLINK)
			{
				p++;
				continue;
			}
			if (*p == ATTR_COLOR)
			{
				p++;
				if (g_ascii_isdigit (*p))
				{
					p++;
					if (g_ascii_isdigit (*p))
						p++;
					if (*p == ',')
					{
						p++;
						if (g_ascii_isdigit (*p))
						{
							p++;
							if (g_ascii_isdigit (*p))
								p++;
						}
					}
				}
				continue;
			}
			p++;
			text_offset++;
		}

		/* Extract word without format codes for URL checking */
		{
			const char *src;
			char *dst;
			int word_len;
			
			word_len = text_offset - word_start_offset;
			word = g_malloc (word_len + 1);
			dst = word;
			
			for (src = word_start; src < p; src++)
			{
				/* Skip format codes */
				if (*src == ATTR_BOLD || *src == ATTR_ITALICS ||
				    *src == ATTR_UNDERLINE || *src == ATTR_STRIKETHROUGH ||
				    *src == ATTR_HIDDEN || *src == ATTR_REVERSE || *src == ATTR_RESET ||
				    *src == ATTR_BEEP || *src == ATTR_BLINK)
					continue;
				if (*src == ATTR_COLOR)
				{
					src++;
					if (g_ascii_isdigit (*src))
					{
						src++;
						if (g_ascii_isdigit (*src))
							src++;
						if (*src == ',')
						{
							src++;
							if (g_ascii_isdigit (*src))
							{
								src++;
								if (g_ascii_isdigit (*src))
									src++;
							}
						}
					}
					src--;  /* Loop will increment */
					continue;
				}
				*dst++ = *src;
			}
			*dst = '\0';
		}

		/* Check if this word is a URL */
		if (url_check_word (word) > 0)
		{
			/* Apply the URL tag */
			gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, 
			                                    line_start_offset + word_start_offset);
			gtk_text_buffer_get_iter_at_offset (buffer, &end_iter,
			                                    line_start_offset + text_offset);
			gtk_text_buffer_apply_tag_by_name (buffer, "url", &start_iter, &end_iter);
		}

		g_free (word);
	}
}

/* ===== Marker line support ===== */

/* Set the marker line at the current end of the buffer */
static void
set_marker_line (session *sess)
{
	session_gui *gui;
	GtkTextIter iter;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (!gui->text_buffer || !prefs.hex_text_show_marker)
		return;

	/* Get the end of the buffer (before the last newline) */
	gtk_text_buffer_get_end_iter (gui->text_buffer, &iter);

	/* Move back to start of last line if there's content */
	if (gtk_text_iter_get_line (&iter) > 0)
	{
		gtk_text_iter_backward_line (&iter);
		gtk_text_iter_forward_to_line_end (&iter);
	}

	/* Delete old marker if it exists */
	if (gui->marker_pos)
	{
		gtk_text_buffer_delete_mark (gui->text_buffer, gui->marker_pos);
	}

	/* Create a new marker at this position */
	gui->marker_pos = gtk_text_buffer_create_mark (gui->text_buffer, "marker-line",
	                                               &iter, TRUE);  /* left gravity */
	gui->marker_visible = FALSE;  /* Will be drawn when tab becomes inactive */
}

/* Draw the marker line visually (apply the tag) */
static void
draw_marker_line (session *sess)
{
	session_gui *gui;
	GtkTextIter start_iter, end_iter;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (!gui->text_buffer || !gui->marker_pos || !prefs.hex_text_show_marker)
		return;

	/* Get the line where the marker is */
	gtk_text_buffer_get_iter_at_mark (gui->text_buffer, &start_iter, gui->marker_pos);
	gtk_text_iter_set_line_offset (&start_iter, 0);
	
	end_iter = start_iter;
	gtk_text_iter_forward_to_line_end (&end_iter);

	/* Apply the marker-line tag to just this line's newline character */
	/* We use a thin approach: just color the line end */
	if (!gtk_text_iter_equal (&start_iter, &end_iter))
	{
		gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "marker-line",
		                                   &start_iter, &end_iter);
		gui->marker_visible = TRUE;
	}
}

/* Clear the marker line (remove the tag) */
static void
clear_marker_line (session *sess)
{
	session_gui *gui;
	GtkTextIter start_iter, end_iter;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (!gui->text_buffer)
		return;

	/* Remove marker-line tag from entire buffer */
	gtk_text_buffer_get_start_iter (gui->text_buffer, &start_iter);
	gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
	gtk_text_buffer_remove_tag_by_name (gui->text_buffer, "marker-line",
	                                    &start_iter, &end_iter);

	gui->marker_visible = FALSE;

	/* Delete the mark */
	if (gui->marker_pos)
	{
		gtk_text_buffer_delete_mark (gui->text_buffer, gui->marker_pos);
		gui->marker_pos = NULL;
	}
}

/* ===== Search bar implementation ===== */

/* Remove all search-highlight and search-current tags from the buffer */
static void
search_clear (session *sess)
{
	session_gui *gui;
	GtkTextIter start, end;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	if (!gui->text_buffer)
		return;

	gtk_text_buffer_get_bounds (gui->text_buffer, &start, &end);
	gtk_text_buffer_remove_tag_by_name (gui->text_buffer, "search-highlight",
	                                    &start, &end);
	gtk_text_buffer_remove_tag_by_name (gui->text_buffer, "search-current",
	                                    &start, &end);

	if (gui->search_current)
	{
		gtk_text_buffer_delete_mark (gui->text_buffer, gui->search_current);
		gui->search_current = NULL;
	}
	gui->search_match_count = 0;
	gui->search_current_index = 0;
}

/* Update the match count label in the search bar */
static void
search_update_label (session *sess)
{
	session_gui *gui;
	char buf[64];

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	if (!gui->search_label)
		return;

	if (gui->search_match_count == 0)
	{
		const char *text = gtk_editable_get_text (GTK_EDITABLE (gui->search_entry));
		if (text && text[0])
			gtk_label_set_text (GTK_LABEL (gui->search_label), _("No results"));
		else
			gtk_label_set_text (GTK_LABEL (gui->search_label), "");
	}
	else
	{
		g_snprintf (buf, sizeof (buf), "%d / %d",
		            gui->search_current_index, gui->search_match_count);
		gtk_label_set_text (GTK_LABEL (gui->search_label), buf);
	}
}

/* Count and highlight all matches in the buffer.
 * Returns the total match count. */
static int
search_highlight_all (session *sess, const char *text,
                      gboolean match_case, gboolean use_regex)
{
	session_gui *gui;
	GtkTextIter start, end, match_start, match_end;
	GtkTextSearchFlags flags;
	GRegex *regex = NULL;
	int count = 0;

	if (!sess || !sess->gui || !text || !text[0])
		return 0;

	gui = sess->gui;
	if (!gui->text_buffer)
		return 0;

	if (use_regex)
	{
		GRegexCompileFlags gcf = match_case ? 0 : G_REGEX_CASELESS;
		GError *err = NULL;

		regex = g_regex_new (text, gcf, 0, &err);
		if (!regex)
		{
			if (err)
			{
				char *msg = g_strdup_printf (_("Regex error: %s"), err->message);
				gtk_label_set_text (GTK_LABEL (gui->search_label), msg);
				g_free (msg);
				g_error_free (err);
			}
			return -1;  /* error indicator */
		}

		/* Regex search: extract full text and find matches */
		gtk_text_buffer_get_bounds (gui->text_buffer, &start, &end);
		{
			char *buffer_text = gtk_text_buffer_get_text (gui->text_buffer,
			                                              &start, &end, FALSE);
			GMatchInfo *match_info = NULL;

			if (g_regex_match (regex, buffer_text, 0, &match_info))
			{
				while (g_match_info_matches (match_info))
				{
					int s_pos, e_pos;
					GtkTextIter ms, me;

					g_match_info_fetch_pos (match_info, 0, &s_pos, &e_pos);

					/* Convert byte offsets to char offsets for GtkTextIter */
					{
						int char_start = g_utf8_pointer_to_offset (buffer_text,
						                                           buffer_text + s_pos);
						int char_end = g_utf8_pointer_to_offset (buffer_text,
						                                         buffer_text + e_pos);
						gtk_text_buffer_get_iter_at_offset (gui->text_buffer, &ms, char_start);
						gtk_text_buffer_get_iter_at_offset (gui->text_buffer, &me, char_end);
					}

					gtk_text_buffer_apply_tag_by_name (gui->text_buffer,
					                                   "search-highlight",
					                                   &ms, &me);
					count++;
					g_match_info_next (match_info, NULL);
				}
			}

			g_match_info_free (match_info);
			g_free (buffer_text);
		}

		g_regex_unref (regex);
	}
	else
	{
		/* Plain text search using GtkTextIter */
		flags = GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY;
		if (!match_case)
			flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;

		gtk_text_buffer_get_start_iter (gui->text_buffer, &start);

		while (gtk_text_iter_forward_search (&start, text, flags,
		                                     &match_start, &match_end, NULL))
		{
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer,
			                                   "search-highlight",
			                                   &match_start, &match_end);
			count++;
			start = match_end;
		}
	}

	return count;
}

/* Find the Nth match (1-based) and apply search-current tag.
 * Returns TRUE if the match was found and scrolled to. */
static gboolean
search_goto_match (session *sess, const char *text,
                   gboolean match_case, gboolean use_regex, int target_index)
{
	session_gui *gui;
	GtkTextIter start, match_start, match_end;
	GtkTextSearchFlags flags;
	int current = 0;

	if (!sess || !sess->gui || !text || !text[0] || target_index < 1)
		return FALSE;

	gui = sess->gui;
	if (!gui->text_buffer)
		return FALSE;

	if (use_regex)
	{
		GRegexCompileFlags gcf = match_case ? 0 : G_REGEX_CASELESS;
		GError *err = NULL;
		GRegex *regex;

		regex = g_regex_new (text, gcf, 0, &err);
		if (!regex)
		{
			if (err) g_error_free (err);
			return FALSE;
		}

		gtk_text_buffer_get_bounds (gui->text_buffer, &start, &match_end);
		{
			char *buffer_text = gtk_text_buffer_get_text (gui->text_buffer,
			                                              &start, &match_end, FALSE);
			GMatchInfo *match_info = NULL;
			gboolean found = FALSE;

			if (g_regex_match (regex, buffer_text, 0, &match_info))
			{
				while (g_match_info_matches (match_info))
				{
					current++;
					if (current == target_index)
					{
						int s_pos, e_pos;

						g_match_info_fetch_pos (match_info, 0, &s_pos, &e_pos);
						{
							int char_start = g_utf8_pointer_to_offset (buffer_text,
							                                           buffer_text + s_pos);
							int char_end = g_utf8_pointer_to_offset (buffer_text,
							                                         buffer_text + e_pos);
							gtk_text_buffer_get_iter_at_offset (gui->text_buffer,
							                                    &match_start, char_start);
							gtk_text_buffer_get_iter_at_offset (gui->text_buffer,
							                                    &match_end, char_end);
						}

						gtk_text_buffer_apply_tag_by_name (gui->text_buffer,
						                                   "search-current",
						                                   &match_start, &match_end);

						/* Create/move the current-match mark */
						if (gui->search_current)
							gtk_text_buffer_move_mark (gui->text_buffer,
							                           gui->search_current,
							                           &match_start);
						else
							gui->search_current = gtk_text_buffer_create_mark (
								gui->text_buffer, "search-current",
								&match_start, TRUE);

						gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (gui->text_view),
						                              gui->search_current,
						                              0.1, FALSE, 0.0, 0.0);
						found = TRUE;
						break;
					}
					g_match_info_next (match_info, NULL);
				}
			}
			g_match_info_free (match_info);
			g_free (buffer_text);
			g_regex_unref (regex);
			return found;
		}
	}
	else
	{
		flags = GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY;
		if (!match_case)
			flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;

		gtk_text_buffer_get_start_iter (gui->text_buffer, &start);

		while (gtk_text_iter_forward_search (&start, text, flags,
		                                     &match_start, &match_end, NULL))
		{
			current++;
			if (current == target_index)
			{
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer,
				                                   "search-current",
				                                   &match_start, &match_end);

				if (gui->search_current)
					gtk_text_buffer_move_mark (gui->text_buffer,
					                           gui->search_current,
					                           &match_start);
				else
					gui->search_current = gtk_text_buffer_create_mark (
						gui->text_buffer, "search-current",
						&match_start, TRUE);

				gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (gui->text_view),
				                              gui->search_current,
				                              0.1, FALSE, 0.0, 0.0);
				return TRUE;
			}
			start = match_end;
		}
	}

	return FALSE;
}

/* Perform a full search: highlight all + navigate to a match */
static void
search_do (session *sess, gboolean backward)
{
	session_gui *gui;
	const char *text;
	gboolean match_case, use_regex, highlight_all;
	int count;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	if (!gui->search_entry || !gui->text_buffer)
		return;

	text = gtk_editable_get_text (GTK_EDITABLE (gui->search_entry));
	if (!text || !text[0])
	{
		search_clear (sess);
		search_update_label (sess);
		return;
	}

	match_case = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_case_btn));
	use_regex = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_regex_btn));
	highlight_all = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_highlight_btn));

	/* Clear existing highlights */
	search_clear (sess);

	/* Count and optionally highlight all matches */
	count = search_highlight_all (sess, text, match_case, use_regex);

	if (count < 0)
	{
		/* Regex error - label already set by search_highlight_all */
		return;
	}

	gui->search_match_count = count;

	if (!highlight_all && count > 0)
	{
		/* Remove the highlight-all tags if user doesn't want them */
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds (gui->text_buffer, &start, &end);
		gtk_text_buffer_remove_tag_by_name (gui->text_buffer, "search-highlight",
		                                    &start, &end);
	}

	if (count > 0)
	{
		int target;

		if (backward)
		{
			/* Navigate from current position backwards */
			if (gui->search_current_index > 1)
				target = gui->search_current_index - 1;
			else
				target = count;  /* wrap to last */
		}
		else
		{
			/* When first searching (index==0), go to last match (most recent text).
			 * Otherwise step forward. */
			if (gui->search_current_index == 0)
				target = count;
			else if (gui->search_current_index < count)
				target = gui->search_current_index + 1;
			else
				target = 1;  /* wrap to first */
		}

		gui->search_current_index = target;
		search_goto_match (sess, text, match_case, use_regex, target);
	}

	search_update_label (sess);
}

/* Re-highlight without changing current position (for toggle changes) */
static void
search_update_highlights (session *sess)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	/* Just re-run the full search keeping the same position */
	{
		const char *text;
		gboolean match_case, use_regex, highlight_all;
		int count, saved_index;

		if (!gui->search_entry || !gui->text_buffer)
			return;

		text = gtk_editable_get_text (GTK_EDITABLE (gui->search_entry));
		if (!text || !text[0])
		{
			search_clear (sess);
			search_update_label (sess);
			return;
		}

		match_case = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_case_btn));
		use_regex = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_regex_btn));
		highlight_all = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->search_highlight_btn));
		saved_index = gui->search_current_index;

		search_clear (sess);
		count = search_highlight_all (sess, text, match_case, use_regex);
		if (count < 0)
			return;

		gui->search_match_count = count;

		if (!highlight_all && count > 0)
		{
			GtkTextIter start, end;
			gtk_text_buffer_get_bounds (gui->text_buffer, &start, &end);
			gtk_text_buffer_remove_tag_by_name (gui->text_buffer, "search-highlight",
			                                    &start, &end);
		}

		/* Restore the previous current index, clamped to new count */
		if (saved_index > 0 && saved_index <= count)
			gui->search_current_index = saved_index;
		else if (count > 0)
			gui->search_current_index = count;
		else
			gui->search_current_index = 0;

		if (gui->search_current_index > 0)
			search_goto_match (sess, text, match_case, use_regex,
			                   gui->search_current_index);

		search_update_label (sess);
	}
}

/* --- Search bar signal callbacks --- */

/* Called when search text changes (incremental search) */
static void
search_entry_changed_cb (GtkEditable *editable, gpointer user_data)
{
	session *sess = user_data;
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	gui->search_current_index = 0;  /* reset position on new text */
	search_do (sess, FALSE);
}

/* Called when user presses Enter or clicks Next */
static void
search_next_cb (GtkWidget *widget, gpointer user_data)
{
	session *sess = user_data;
	search_do (sess, FALSE);
}

/* Called when user clicks Previous */
static void
search_prev_cb (GtkWidget *widget, gpointer user_data)
{
	session *sess = user_data;
	search_do (sess, TRUE);
}

/* Called when a toggle (highlight, case, regex) changes */
static void
search_option_toggled_cb (GtkToggleButton *btn, gpointer user_data)
{
	session *sess = user_data;
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	/* If case or regex changed, reset index and re-search */
	if (GTK_WIDGET (btn) == gui->search_case_btn ||
	    GTK_WIDGET (btn) == gui->search_regex_btn)
	{
		gui->search_current_index = 0;
		search_do (sess, FALSE);
	}
	else
	{
		/* Highlight toggle: just refresh highlights */
		search_update_highlights (sess);
	}
}

/* Called when the search bar close button is clicked */
static void
search_close_cb (GtkButton *button, gpointer user_data)
{
	session *sess = user_data;
	search_toggle (sess);
}

/* Key press handler for the search entry (Escape to close, Enter for next) */
static gboolean
search_entry_key_pressed_cb (GtkEventControllerKey *controller,
                             guint keyval,
                             guint keycode,
                             GdkModifierType state,
                             gpointer user_data)
{
	session *sess = user_data;

	if (keyval == GDK_KEY_Escape)
	{
		search_toggle (sess);
		return TRUE;
	}

	/* Shift+Enter for previous match */
	if (keyval == GDK_KEY_Return && (state & GDK_SHIFT_MASK))
	{
		search_do (sess, TRUE);
		return TRUE;
	}

	return FALSE;
}

/* Toggle the search bar visibility */
static void
search_toggle (session *sess)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;
	if (!gui->search_bar)
		return;

	if (gtk_widget_get_visible (gui->search_bar))
	{
		/* Hide search bar and clear */
		gtk_widget_set_visible (gui->search_bar, FALSE);
		search_clear (sess);
		search_update_label (sess);
		if (gui->input_entry)
			gtk_widget_grab_focus (gui->input_entry);
	}
	else
	{
		/* Show search bar */
		gtk_widget_set_visible (gui->search_bar, TRUE);
		if (gui->search_entry)
		{
			gtk_editable_set_text (GTK_EDITABLE (gui->search_entry), "");
			gtk_widget_grab_focus (gui->search_entry);
		}
	}
}

/* Create the search bar widget for a session.
 * Returns the search bar box widget (hidden by default). */
static GtkWidget *
create_search_bar (session *sess)
{
	session_gui *gui = sess->gui;
	GtkWidget *hbox, *close_btn, *label, *prev_btn, *next_btn;
	GtkEventController *key_controller;

	/* Container */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_add_css_class (hbox, "hexchat-search-bar");
	gtk_widget_set_visible (hbox, FALSE);
	gui->search_bar = hbox;

	/* Close button */
	close_btn = gtk_button_new_from_icon_name ("window-close-symbolic");
	gtk_widget_add_css_class (close_btn, "flat");
	gtk_widget_add_css_class (close_btn, "circular");
	gtk_widget_set_tooltip_text (close_btn, _("Close search bar (Escape)"));
	g_signal_connect (close_btn, "clicked", G_CALLBACK (search_close_cb), sess);
	gtk_box_append (GTK_BOX (hbox), close_btn);

	/* "Find:" label */
	label = gtk_label_new (_("Find:"));
	gtk_box_append (GTK_BOX (hbox), label);

	/* Search entry */
	gui->search_entry = gtk_search_entry_new ();
	gtk_widget_set_size_request (gui->search_entry, 200, -1);
	gtk_widget_set_hexpand (gui->search_entry, FALSE);
	g_signal_connect (gui->search_entry, "search-changed",
	                  G_CALLBACK (search_entry_changed_cb), sess);
	g_signal_connect (gui->search_entry, "activate",
	                  G_CALLBACK (search_next_cb), sess);
	gtk_box_append (GTK_BOX (hbox), gui->search_entry);

	/* Key controller for Escape/Shift+Enter in search entry */
	key_controller = gtk_event_controller_key_new ();
	g_signal_connect (key_controller, "key-pressed",
	                  G_CALLBACK (search_entry_key_pressed_cb), sess);
	gtk_widget_add_controller (gui->search_entry, key_controller);

	/* Previous button */
	prev_btn = gtk_button_new_from_icon_name ("go-up-symbolic");
	gtk_widget_add_css_class (prev_btn, "flat");
	gtk_widget_set_tooltip_text (prev_btn, _("Previous match (Shift+Enter)"));
	g_signal_connect (prev_btn, "clicked", G_CALLBACK (search_prev_cb), sess);
	gtk_box_append (GTK_BOX (hbox), prev_btn);

	/* Next button */
	next_btn = gtk_button_new_from_icon_name ("go-down-symbolic");
	gtk_widget_add_css_class (next_btn, "flat");
	gtk_widget_set_tooltip_text (next_btn, _("Next match (Enter)"));
	g_signal_connect (next_btn, "clicked", G_CALLBACK (search_next_cb), sess);
	gtk_box_append (GTK_BOX (hbox), next_btn);

	/* Match count label */
	gui->search_label = gtk_label_new ("");
	gtk_widget_add_css_class (gui->search_label, "dim-label");
	gtk_widget_set_margin_start (gui->search_label, 4);
	gtk_box_append (GTK_BOX (hbox), gui->search_label);

	/* Spacer to push toggles to the right */
	{
		GtkWidget *spacer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_hexpand (spacer, TRUE);
		gtk_box_append (GTK_BOX (hbox), spacer);
	}

	/* Highlight All toggle */
	gui->search_highlight_btn = gtk_toggle_button_new_with_label (_("Highlight All"));
	gtk_widget_add_css_class (gui->search_highlight_btn, "flat");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->search_highlight_btn), TRUE);
	g_signal_connect (gui->search_highlight_btn, "toggled",
	                  G_CALLBACK (search_option_toggled_cb), sess);
	gtk_box_append (GTK_BOX (hbox), gui->search_highlight_btn);

	/* Match Case toggle */
	gui->search_case_btn = gtk_toggle_button_new_with_label (_("Match Case"));
	gtk_widget_add_css_class (gui->search_case_btn, "flat");
	g_signal_connect (gui->search_case_btn, "toggled",
	                  G_CALLBACK (search_option_toggled_cb), sess);
	gtk_box_append (GTK_BOX (hbox), gui->search_case_btn);

	/* Regex toggle */
	gui->search_regex_btn = gtk_toggle_button_new_with_label (_("Regex"));
	gtk_widget_add_css_class (gui->search_regex_btn, "flat");
	g_signal_connect (gui->search_regex_btn, "toggled",
	                  G_CALLBACK (search_option_toggled_cb), sess);
	gtk_box_append (GTK_BOX (hbox), gui->search_regex_btn);

	return hbox;
}

/* ===== Input handling callbacks ===== */

/* Callback when user presses Enter in input box */
static void
input_activate_cb (GtkEntry *entry, gpointer user_data)
{
	session *sess = user_data;
	const char *text;
	char *cmd;

	if (!sess)
		return;

	text = gtk_editable_get_text (GTK_EDITABLE (entry));
	if (!text || text[0] == '\0')
		return;

	/* Make a copy since we'll clear the entry */
	cmd = g_strdup (text);

	/* Clear input box */
	gtk_editable_set_text (GTK_EDITABLE (entry), "");

	/* Process the command */
	handle_multiline (sess, cmd, TRUE, FALSE);

	g_free (cmd);
}

/* Callback for key press events in input box */
static gboolean
input_key_pressed_cb (GtkEventControllerKey *controller,
                      guint keyval,
                      guint keycode,
                      GdkModifierType state,
                      gpointer user_data)
{
	session *sess = user_data;
	session_gui *gui;
	const char *new_line;
	const char *current_text;

	if (!sess || !sess->gui)
		return FALSE;

	gui = sess->gui;

	/* Ctrl+F: toggle search bar */
	if (keyval == GDK_KEY_f && (state & GDK_CONTROL_MASK))
	{
		search_toggle (sess);
		return TRUE;
	}

	/* Ctrl+G: search next,  Ctrl+Shift+G: search previous */
	if (keyval == GDK_KEY_g && (state & GDK_CONTROL_MASK))
	{
		if (gui->search_bar && gtk_widget_get_visible (gui->search_bar))
		{
			search_do (sess, (state & GDK_SHIFT_MASK) != 0);
			return TRUE;
		}
	}

	switch (keyval)
	{
	case GDK_KEY_Up:
		/* History up */
		current_text = gtk_editable_get_text (GTK_EDITABLE (gui->input_entry));
		new_line = history_up (&sess->history, (char *)current_text);
		if (new_line)
		{
			gtk_editable_set_text (GTK_EDITABLE (gui->input_entry), new_line);
			gtk_editable_set_position (GTK_EDITABLE (gui->input_entry), -1);
		}
		return TRUE;

	case GDK_KEY_Down:
		/* History down */
		new_line = history_down (&sess->history);
		if (new_line)
		{
			gtk_editable_set_text (GTK_EDITABLE (gui->input_entry), new_line);
			gtk_editable_set_position (GTK_EDITABLE (gui->input_entry), -1);
		}
		return TRUE;

	case GDK_KEY_Tab:
		/* Tab completion */
		{
			const char *text;
			int cursor_pos;
			int word_start;
			gboolean at_start;
			char prefix_char;
			char word[256];
			int word_len;
			const char *match;
			GList *nicks;
			GList *l;
			int i;

			text = gtk_editable_get_text (GTK_EDITABLE (gui->input_entry));
			cursor_pos = gtk_editable_get_position (GTK_EDITABLE (gui->input_entry));

			if (cursor_pos == 0)
				return TRUE;

			/* Find the start of the word we're completing */
			word_start = cursor_pos;
			while (word_start > 0 && text[word_start - 1] != ' ')
				word_start--;

			at_start = (word_start == 0);

			/* Check for nick prefix characters like @ + ~ etc. */
			prefix_char = '\0';
			if (word_start < cursor_pos && (text[word_start] == '@' || 
			    text[word_start] == '+' || text[word_start] == '%' ||
			    text[word_start] == '~' || text[word_start] == '&'))
			{
				prefix_char = text[word_start];
				word_start++;
			}

			/* Extract the partial word */
			word_len = cursor_pos - word_start;
			if (word_len <= 0 || word_len >= (int)sizeof(word) - 1)
				return TRUE;

			for (i = 0; i < word_len; i++)
				word[i] = text[word_start + i];
			word[word_len] = '\0';

			/* Get matching nick from userlist */
			match = NULL;
			nicks = userlist_double_list (sess);
			for (l = nicks; l; l = l->next)
			{
				struct User *user = l->data;
				if (g_ascii_strncasecmp (user->nick, word, word_len) == 0)
				{
					match = user->nick;
					break;
				}
			}
			g_list_free (nicks);

			if (match)
			{
				/* Build the completed text */
				GString *new_text = g_string_new ("");

				/* Text before the word we're completing */
				if (word_start > 0)
				{
					if (prefix_char)
						g_string_append_len (new_text, text, word_start - 1);
					else
						g_string_append_len (new_text, text, word_start);
				}

				/* Add prefix character if it was present */
				if (prefix_char)
					g_string_append_c (new_text, prefix_char);

				/* Add the matched nick */
				g_string_append (new_text, match);

				/* Add suffix for nick at start of line */
				if (at_start && prefs.hex_completion_suffix[0])
				{
					g_string_append_c (new_text, prefs.hex_completion_suffix[0]);
				}
				g_string_append_c (new_text, ' ');

				/* Remember where cursor should be */
				cursor_pos = new_text->len;

				/* Add rest of text after the word */
				if (text[word_start + word_len])
					g_string_append (new_text, text + word_start + word_len);

				gtk_editable_set_text (GTK_EDITABLE (gui->input_entry), new_text->str);
				gtk_editable_set_position (GTK_EDITABLE (gui->input_entry), cursor_pos);

				g_string_free (new_text, TRUE);
			}
		}
		return TRUE;

	default:
		return FALSE;
	}
}

/* ===== User List Factory Callbacks ===== */

static void
userlist_setup_cb (GtkSignalListItemFactory *factory,
                   GtkListItem *list_item,
                   gpointer user_data)
{
	GtkWidget *label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_widget_set_margin_start (label, 4);
	gtk_widget_set_margin_end (label, 4);
	gtk_widget_set_margin_top (label, 2);
	gtk_widget_set_margin_bottom (label, 2);
	gtk_list_item_set_child (list_item, label);
}

static void
userlist_bind_cb (GtkSignalListItemFactory *factory,
                  GtkListItem *list_item,
                  gpointer user_data)
{
	GtkWidget *label = gtk_list_item_get_child (list_item);
	UserItem *item = gtk_list_item_get_item (list_item);
	char display_text[NICKLEN + 4];

	if (!item)
		return;

	/* Format: prefix + nick (e.g., "@john" or "+jane") */
	if (item->prefix[0])
		g_snprintf (display_text, sizeof (display_text), "%s%s", item->prefix, item->nick);
	else
		g_snprintf (display_text, sizeof (display_text), "%s", item->nick);

	gtk_label_set_text (GTK_LABEL (label), display_text);

	/* Style based on user status */
	if (item->is_away)
	{
		/* Grey out away users */
		gtk_widget_add_css_class (label, "dim-label");
	}
	else
	{
		gtk_widget_remove_css_class (label, "dim-label");
	}
}

static void
userlist_unbind_cb (GtkSignalListItemFactory *factory,
                    GtkListItem *list_item,
                    gpointer user_data)
{
	/* Nothing special needed for unbind */
}

/* ===== Userlist Context Menu ===== */

/* Structure to hold menu item callback data */
typedef struct {
	session *sess;
	char *nick;
	char *cmd;
} MenuItemData;

static void
menu_item_data_free (gpointer data, GClosure *closure)
{
	MenuItemData *mid = data;
	g_free (mid->nick);
	g_free (mid->cmd);
	g_free (mid);
}

/* Execute a userlist popup menu command */
static void
nick_command (session *sess, char *cmd)
{
	if (*cmd == '!')
		hexchat_exec (cmd + 1);
	else
		handle_command (sess, cmd, TRUE);
}

/* Fill in the %a %s %n etc and execute the command */
static void
nick_command_parse (session *sess, char *cmd, char *nick, char *allnick)
{
	char *buf;
	char *host = _("Host unknown");
	char *account = _("Account unknown");
	struct User *user;
	int len;

	user = userlist_find (sess, nick);
	if (user)
	{
		if (user->hostname)
			host = strchr (user->hostname, '@');
		if (host)
			host = host + 1;
		else
			host = _("Host unknown");
		if (user->account)
			account = user->account;
	}

	/* this can't overflow, since popup->cmd is only 256 */
	len = strlen (cmd) + strlen (nick) + strlen (allnick) + 512;
	buf = g_malloc (len);

	auto_insert (buf, len, (unsigned char *)cmd, 0, 0, allnick, sess->channel, "",
	             server_get_network (sess->server, TRUE), host,
	             sess->server->nick, nick, account);

	nick_command (sess, buf);

	g_free (buf);
}

/* Get the selected nick from the userlist */
static char *
userlist_get_selected_nick (session *sess)
{
	session_gui *gui;
	GtkSelectionModel *selection;
	GtkBitset *selected;
	guint first_pos;
	UserItem *item;

	if (!sess || !sess->gui)
		return NULL;

	gui = sess->gui;
	if (!gui->userlist_view)
		return NULL;

	selection = gtk_list_view_get_model (GTK_LIST_VIEW (gui->userlist_view));
	selected = gtk_selection_model_get_selection (selection);

	if (gtk_bitset_is_empty (selected))
	{
		gtk_bitset_unref (selected);
		return NULL;
	}

	first_pos = gtk_bitset_get_nth (selected, 0);
	gtk_bitset_unref (selected);

	item = g_list_model_get_item (G_LIST_MODEL (gui->userlist_store), first_pos);
	if (!item)
		return NULL;

	char *nick = g_strdup (item->nick);
	g_object_unref (item);
	return nick;
}

/* Callback for popup menu item activation */
static void
popup_menu_item_activated (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
	MenuItemData *mid = user_data;

	if (mid->sess && mid->cmd)
	{
		/* nick_command_parse handles $1 substitution etc */
		nick_command_parse (mid->sess, mid->cmd, mid->nick, mid->nick);
	}
}

/* Create and show the userlist popup menu */
static void
userlist_popup_menu (session *sess, double x, double y, GtkWidget *widget)
{
	GSList *list;
	struct popup *pop;
	GMenu *menu;
	GMenu *current_section;
	GSimpleActionGroup *action_group;
	GtkWidget *popover;
	char *nick;
	int action_index = 0;

	nick = userlist_get_selected_nick (sess);
	if (!nick)
		return;

	menu = g_menu_new ();
	current_section = g_menu_new ();
	action_group = g_simple_action_group_new ();

	/* Add nick info header */
	{
		GMenuItem *header = g_menu_item_new (nick, NULL);
		g_menu_item_set_attribute (header, "action", "s", "none");
		g_menu_append_item (current_section, header);
		g_object_unref (header);
	}

	g_menu_append_section (menu, NULL, G_MENU_MODEL (current_section));
	g_object_unref (current_section);
	current_section = g_menu_new ();

	/* Build menu from popup_list */
	list = popup_list;
	while (list)
	{
		pop = (struct popup *)list->data;
		list = list->next;

		if (pop->name[0] == '-')
		{
			/* Separator - start a new section */
			if (g_menu_model_get_n_items (G_MENU_MODEL (current_section)) > 0)
			{
				g_menu_append_section (menu, NULL, G_MENU_MODEL (current_section));
				g_object_unref (current_section);
				current_section = g_menu_new ();
			}
			continue;
		}

		if (strncasecmp (pop->name, "SUB", 3) == 0 ||
		    strncasecmp (pop->name, "ENDSUB", 6) == 0)
		{
			/* Skip submenu markers for now - flatten the menu */
			continue;
		}

		/* Create action for this menu item */
		char action_name[32];
		g_snprintf (action_name, sizeof (action_name), "popup%d", action_index);

		MenuItemData *mid = g_new0 (MenuItemData, 1);
		mid->sess = sess;
		mid->nick = g_strdup (nick);
		mid->cmd = g_strdup (pop->cmd);

		GSimpleAction *action = g_simple_action_new (action_name, NULL);
		g_signal_connect_data (action, "activate",
		                       G_CALLBACK (popup_menu_item_activated),
		                       mid,
		                       menu_item_data_free,
		                       0);
		g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
		g_object_unref (action);

		/* Add menu item */
		char detailed_action[64];
		g_snprintf (detailed_action, sizeof (detailed_action), "userlist.%s", action_name);
		g_menu_append (current_section, pop->name, detailed_action);

		action_index++;
	}

	/* Append final section if non-empty */
	if (g_menu_model_get_n_items (G_MENU_MODEL (current_section)) > 0)
	{
		g_menu_append_section (menu, NULL, G_MENU_MODEL (current_section));
	}
	g_object_unref (current_section);

	/* Insert action group into widget */
	gtk_widget_insert_action_group (widget, "userlist", G_ACTION_GROUP (action_group));

	/* Create popover menu */
	popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
	gtk_widget_set_parent (popover, widget);
	gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);

	/* Position the popover at the click location */
	GdkRectangle rect = { (int)x, (int)y, 1, 1 };
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

	gtk_popover_popup (GTK_POPOVER (popover));

	/* Clean up menu model (popover takes a reference) */
	g_object_unref (menu);
	g_object_unref (action_group);
	g_free (nick);
}

/* Right-click handler for userlist */
static void
userlist_click_cb (GtkGestureClick *gesture,
                   int n_press,
                   double x,
                   double y,
                   gpointer user_data)
{
	session *sess = user_data;
	GtkWidget *widget;

	widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

	/* Right-click - show context menu */
	userlist_popup_menu (sess, x, y, widget);
}

/* GtkListView "activate" signal: fired on double-click (or Enter key).
 * The position parameter gives the index of the activated item. */
static void
userlist_activate_cb (GtkListView *view, guint position, gpointer user_data)
{
	session *sess = user_data;
	UserItem *item;

	if (!sess || !sess->gui || !sess->gui->userlist_store)
		return;

	item = g_list_model_get_item (G_LIST_MODEL (sess->gui->userlist_store), position);
	if (!item)
		return;

	if (item->nick)
	{
		if (prefs.hex_gui_ulist_doubleclick[0])
		{
			/* Execute the configured double-click action (default: "QUERY %s") */
			nick_command_parse (sess, prefs.hex_gui_ulist_doubleclick,
			                    item->nick, item->nick);
		}
		else
		{
			/* Fallback: open or focus a dialog session directly */
			open_query (sess->server, item->nick, TRUE);
		}
	}

	g_object_unref (item);
}

/* ===== Topic and Nick callbacks ===== */

/* Called when the user presses Enter in the topic entry.
 * Sends a TOPIC command to change the channel topic. */
static void
topic_activate_cb (GtkEntry *entry, gpointer user_data)
{
	session *sess = user_data;
	const char *text;

	if (!sess)
		return;

	text = gtk_editable_get_text (GTK_EDITABLE (entry));
	if (text && text[0])
	{
		char tbuf[CHANLEN + 512];
		g_snprintf (tbuf, sizeof (tbuf), "TOPIC %s :%s", sess->channel, text);
		handle_command (sess, tbuf, FALSE);
	}
	else
	{
		/* Empty topic - unset */
		char tbuf[CHANLEN + 16];
		g_snprintf (tbuf, sizeof (tbuf), "TOPIC %s :", sess->channel);
		handle_command (sess, tbuf, FALSE);
	}
}

/* Callback for nick change dialog response */
static void
nick_change_response_cb (AdwAlertDialog *dialog, const char *response,
                         gpointer user_data)
{
	session *sess = user_data;
	GtkWidget *entry;

	if (!g_strcmp0 (response, "change"))
	{
		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		if (entry)
		{
			const char *new_nick;
			new_nick = gtk_editable_get_text (GTK_EDITABLE (entry));
			if (new_nick && new_nick[0])
			{
				char buf[256];
				g_snprintf (buf, sizeof (buf), "nick %s", new_nick);
				handle_command (sess, buf, FALSE);
			}
		}
	}
}

/* Called when the nick button is clicked.
 * Opens a dialog to change the nickname. */
static void
nick_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	session *sess = user_data;
	AdwAlertDialog *dialog;
	GtkWidget *entry;

	if (!sess || !sess->server)
		return;

	dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("Change Nickname"), NULL));
	adw_alert_dialog_set_body (dialog, _("Enter new nickname:"));

	/* Add a text entry as extra child */
	entry = gtk_entry_new ();
	if (sess->server->nick[0])
		gtk_editable_set_text (GTK_EDITABLE (entry), sess->server->nick);
	gtk_widget_set_margin_start (entry, 12);
	gtk_widget_set_margin_end (entry, 12);
	adw_alert_dialog_set_extra_child (dialog, entry);

	adw_alert_dialog_add_responses (dialog,
	                                "cancel", _("Cancel"),
	                                "change", _("Change"),
	                                NULL);
	adw_alert_dialog_set_default_response (dialog, "change");
	adw_alert_dialog_set_close_response (dialog, "cancel");

	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_signal_connect (dialog, "response",
	                  G_CALLBACK (nick_change_response_cb), sess);

	adw_alert_dialog_choose (dialog, GTK_WIDGET (main_window), NULL, NULL, NULL);
}

/* ===== Window/Session management ===== */

void
fe_new_window (struct session *sess, int focus)
{
	session_gui *gui;

	/* Allocate GUI data for this session */
	gui = g_new0 (session_gui, 1);
	sess->gui = gui;

	/* Create the main layout */
	GtkWidget *main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* Create topic entry at the top */
	gui->topic_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (gui->topic_entry), _("Topic"));
	gtk_editable_set_editable (GTK_EDITABLE (gui->topic_entry), TRUE);
	gtk_widget_add_css_class (gui->topic_entry, "hexchat-topic");
	g_signal_connect (gui->topic_entry, "activate",
	                  G_CALLBACK (topic_activate_cb), sess);
	gtk_box_append (GTK_BOX (main_box), gui->topic_entry);

	/* Create paned widget for userlist */
	gui->paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand (gui->paned, TRUE);
	gtk_widget_set_vexpand (gui->paned, TRUE);

	/* Create text view area */
	GtkWidget *text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand (text_box, TRUE);
	gtk_widget_set_vexpand (text_box, TRUE);

	/* Create scrolled window for text view */
	GtkWidget *scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (scroll, TRUE);

	/* Create text view */
	gui->text_buffer = gtk_text_buffer_new (NULL);
	fe_gtk4_init_tags (gui->text_buffer);
	gui->text_view = gtk_text_view_new_with_buffer (gui->text_buffer);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (gui->text_view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (gui->text_view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (gui->text_view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (gui->text_view), 4);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (gui->text_view), 4);
	gtk_widget_add_css_class (gui->text_view, "hexchat-textview");
	palette_update_css ();
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), gui->text_view);
	gtk_box_append (GTK_BOX (text_box), scroll);

	/* Add click gesture for URL handling */
	{
		GtkGesture *click_gesture = gtk_gesture_click_new ();
		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), 0);
		g_signal_connect (click_gesture, "pressed", G_CALLBACK (text_view_click_cb), sess);
		gtk_widget_add_controller (gui->text_view, GTK_EVENT_CONTROLLER (click_gesture));
	}

	/* Add motion controller for cursor changes on URLs */
	{
		GtkEventController *motion_controller = gtk_event_controller_motion_new ();
		g_signal_connect (motion_controller, "motion", G_CALLBACK (text_view_motion_cb), sess);
		gtk_widget_add_controller (gui->text_view, motion_controller);
	}

	/* Create input area with nick button + entry */
	gui->nick_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

	/* Nick button (flat button showing current nick) */
	gui->nick_label = gtk_button_new_with_label ("");
	gtk_widget_add_css_class (gui->nick_label, "flat");
	gtk_widget_add_css_class (gui->nick_label, "hexchat-nick");
	g_signal_connect (gui->nick_label, "clicked",
	                  G_CALLBACK (nick_button_clicked_cb), sess);
	gtk_box_append (GTK_BOX (gui->nick_box), gui->nick_label);

	/* Create input entry */
	gui->input_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (gui->input_entry), "Type a message...");
	gtk_widget_add_css_class (gui->input_entry, "hexchat-input");
	gtk_widget_set_hexpand (gui->input_entry, TRUE);
	gtk_box_append (GTK_BOX (gui->nick_box), gui->input_entry);

	gtk_box_append (GTK_BOX (text_box), gui->nick_box);

	/* Create search bar (hidden by default, toggled with Ctrl+F) */
	{
		GtkWidget *search_bar = create_search_bar (sess);
		gtk_box_append (GTK_BOX (text_box), search_bar);
	}

	/* Setup input handling - activate (Enter) signal */
	g_signal_connect (gui->input_entry, "activate",
	                  G_CALLBACK (input_activate_cb), sess);

	/* Setup key controller for history (Up/Down) and tab completion */
	GtkEventController *key_controller = gtk_event_controller_key_new ();
	g_signal_connect (key_controller, "key-pressed",
	                  G_CALLBACK (input_key_pressed_cb), sess);
	gtk_widget_add_controller (gui->input_entry, key_controller);

	/* Create userlist sidebar box */
	GtkWidget *userlist_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_size_request (userlist_box, 140, -1);

	/* User count label at top of userlist */
	gui->usercount_label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (gui->usercount_label), 0.0);
	gtk_widget_set_margin_start (gui->usercount_label, 4);
	gtk_widget_set_margin_end (gui->usercount_label, 4);
	gtk_widget_set_margin_top (gui->usercount_label, 2);
	gtk_widget_add_css_class (gui->usercount_label, "dim-label");
	gtk_widget_add_css_class (gui->usercount_label, "caption");
	gtk_box_append (GTK_BOX (userlist_box), gui->usercount_label);

	/* Create userlist */
	GtkWidget *userlist_scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (userlist_scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (userlist_scroll, TRUE);

	/* Create GListStore for user items */
	gui->userlist_store = g_list_store_new (USER_ITEM_TYPE);

	/* Create selection model */
	GtkMultiSelection *selection = gtk_multi_selection_new (G_LIST_MODEL (gui->userlist_store));

	/* Create list item factory */
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (userlist_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (userlist_bind_cb), NULL);
	g_signal_connect (factory, "unbind", G_CALLBACK (userlist_unbind_cb), NULL);

	/* Create list view */
	gui->userlist_view = gtk_list_view_new (GTK_SELECTION_MODEL (selection), factory);
	gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (gui->userlist_view), FALSE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (userlist_scroll), gui->userlist_view);

	/* Double-click / Enter activates: open query with the user */
	g_signal_connect (gui->userlist_view, "activate",
	                  G_CALLBACK (userlist_activate_cb), sess);

	/* Right-click gesture for context menu (button 3 only) */
	GtkGesture *click_gesture = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (click_gesture, "pressed", G_CALLBACK (userlist_click_cb), sess);
	gtk_widget_add_controller (gui->userlist_view, GTK_EVENT_CONTROLLER (click_gesture));

	gtk_box_append (GTK_BOX (userlist_box), userlist_scroll);

	/* Lag label at bottom of userlist sidebar */
	gui->lag_label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (gui->lag_label), 0.0);
	gtk_widget_set_margin_start (gui->lag_label, 4);
	gtk_widget_set_margin_end (gui->lag_label, 4);
	gtk_widget_set_margin_bottom (gui->lag_label, 2);
	gtk_widget_add_css_class (gui->lag_label, "dim-label");
	gtk_widget_add_css_class (gui->lag_label, "caption");
	gtk_box_append (GTK_BOX (userlist_box), gui->lag_label);

	/* Pack into paned */
	gtk_paned_set_start_child (GTK_PANED (gui->paned), text_box);
	gtk_paned_set_end_child (GTK_PANED (gui->paned), userlist_box);
	gtk_paned_set_resize_start_child (GTK_PANED (gui->paned), TRUE);
	gtk_paned_set_resize_end_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_shrink_start_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_shrink_end_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_position (GTK_PANED (gui->paned), 700);

	gtk_box_append (GTK_BOX (main_box), gui->paned);

	/* Add to content stack and create sidebar row */
	if (content_stack && channel_sidebar)
	{
		const char *tab_title;
		char stack_name[32];

		/* Get valid UTF-8 title for sidebar */
		if (sess->channel[0] && g_utf8_validate (sess->channel, -1, NULL))
			tab_title = sess->channel;
		else
			tab_title = _("New Tab");

		/* Generate a unique name for this stack child */
		g_snprintf (stack_name, sizeof (stack_name), "sess-%d", session_id_counter++);

		/* Store the content box reference */
		gui->content_box = main_box;

		/* Add session content to the stack */
		gtk_stack_add_named (GTK_STACK (content_stack), main_box, stack_name);

		/* Create sidebar row: a label inside a GtkListBoxRow */
		gui->sidebar_label = gtk_label_new (tab_title);
		gtk_label_set_xalign (GTK_LABEL (gui->sidebar_label), 0.0);
		gtk_widget_set_margin_start (gui->sidebar_label, 8);
		gtk_widget_set_margin_end (gui->sidebar_label, 8);
		gtk_widget_set_margin_top (gui->sidebar_label, 4);
		gtk_widget_set_margin_bottom (gui->sidebar_label, 4);

		gui->sidebar_row = gtk_list_box_row_new ();
		gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (gui->sidebar_row),
		                            gui->sidebar_label);

		/* Attach right-click gesture for context menu */
		{
			GtkGesture *click;
			click = gtk_gesture_click_new ();
			gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 3);
			g_signal_connect (click, "pressed",
			                  G_CALLBACK (sidebar_row_right_click_cb),
			                  gui->sidebar_row);
			gtk_widget_add_controller (gui->sidebar_row,
			                           GTK_EVENT_CONTROLLER (click));
		}

		gtk_list_box_append (GTK_LIST_BOX (channel_sidebar), gui->sidebar_row);

		if (focus)
			gtk_list_box_select_row (GTK_LIST_BOX (channel_sidebar),
			                         GTK_LIST_BOX_ROW (gui->sidebar_row));
	}

	/* Set up server relationships */
	current_sess = sess;
	if (!sess->server->front_session)
		sess->server->front_session = sess;
	if (!sess->server->server_session)
		sess->server->server_session = sess;
	if (!current_tab || focus)
		current_tab = sess;
}

void
fe_new_server (struct server *serv)
{
	serv->gui = g_new0 (server_gui, 1);
}

void
fe_close_window (struct session *sess)
{
	session_gui *gui = sess->gui;

	if (gui)
	{
		/* Only manipulate GTK widgets if we're not quitting */
		if (!hexchat_is_quitting)
		{
			if (gui->sidebar_row && channel_sidebar)
				gtk_list_box_remove (GTK_LIST_BOX (channel_sidebar),
				                     gui->sidebar_row);
			if (gui->content_box && content_stack)
				gtk_stack_remove (GTK_STACK (content_stack), gui->content_box);
		}

		/* Clear references but don't unref - GTK owns these */
		gui->text_buffer = NULL;
		gui->text_view = NULL;
		gui->userlist_store = NULL;
		gui->sidebar_row = NULL;
		gui->sidebar_label = NULL;
		gui->content_box = NULL;
		gui->topic_entry = NULL;
		gui->nick_box = NULL;
		gui->nick_label = NULL;
		gui->usercount_label = NULL;
		gui->lag_label = NULL;
		gui->input_entry = NULL;
		gui->paned = NULL;

		/* Search bar cleanup */
		gui->search_bar = NULL;
		gui->search_entry = NULL;
		gui->search_label = NULL;
		gui->search_highlight_btn = NULL;
		gui->search_case_btn = NULL;
		gui->search_regex_btn = NULL;
		gui->search_current = NULL;

		g_free (gui);
		sess->gui = NULL;
	}

	session_free (sess);
}

void
fe_session_callback (struct session *sess)
{
	/* Called when session is being freed */
}

void
fe_server_callback (struct server *serv)
{
	if (serv->gui)
	{
		g_free (serv->gui);
		serv->gui = NULL;
	}
}

/* ===== Text output ===== */

void
fe_print_text (struct session *sess, char *text, time_t stamp,
               gboolean no_activity)
{
	session_gui *gui;
	GtkTextIter start_iter, end_iter;
	GtkTextMark *end_mark;
	GtkAdjustment *vadj;
	gboolean at_bottom;
	char *p;
	char *segment_start;
	int start_offset;
	int line_start_offset;  /* Offset where this line begins in the buffer */
	char tag_name[32];

	/* Current formatting state (applied to upcoming text) */
	gboolean bold = FALSE;
	gboolean italic = FALSE;
	gboolean underline = FALSE;
	gboolean strikethrough = FALSE;
	gboolean hidden = FALSE;
	gboolean reverse = FALSE;
	int fg_color = -1;  /* -1 means no color */
	int bg_color = -1;

	/* Previous formatting state (applied to segment being inserted) */
	gboolean prev_bold = FALSE;
	gboolean prev_italic = FALSE;
	gboolean prev_underline = FALSE;
	gboolean prev_strikethrough = FALSE;
	gboolean prev_hidden = FALSE;
	gboolean prev_reverse = FALSE;
	int prev_fg_color = -1;
	int prev_bg_color = -1;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (!gui->text_buffer || !gui->text_view)
		return;

	/* Check if text_view widget is still valid */
	if (!GTK_IS_WIDGET (gui->text_view))
		return;

	/* Record offset where this line will start (for URL detection) */
	gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
	line_start_offset = gtk_text_iter_get_offset (&end_iter);

	/* Check if we're at the bottom (for auto-scroll) */
	GtkWidget *scroll = gtk_widget_get_parent (gui->text_view);
	if (GTK_IS_SCROLLED_WINDOW (scroll))
	{
		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
		at_bottom = (gtk_adjustment_get_value (vadj) >=
		             gtk_adjustment_get_upper (vadj) - gtk_adjustment_get_page_size (vadj) - 1);
	}
	else
	{
		at_bottom = TRUE;
	}

	/* Insert timestamp if enabled */
	if (prefs.hex_stamp_text && prefs.hex_stamp_text_format[0])
	{
		char *stamp_str;
		int stamp_len;
		time_t display_time;

		/* Use provided timestamp or current time */
		display_time = (stamp != 0) ? stamp : time (NULL);

		stamp_len = get_stamp_str (prefs.hex_stamp_text_format, display_time, &stamp_str);
		if (stamp_len > 0 && stamp_str)
		{
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			start_offset = gtk_text_iter_get_offset (&end_iter);
			gtk_text_buffer_insert (gui->text_buffer, &end_iter, stamp_str, stamp_len);

			/* Apply timestamp tag for styling */
			gtk_text_buffer_get_iter_at_offset (gui->text_buffer, &start_iter, start_offset);
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "timestamp", &start_iter, &end_iter);

			g_free (stamp_str);

			/* Update line_start_offset to account for timestamp */
			line_start_offset = gtk_text_iter_get_offset (&end_iter);
		}
	}

	/* Process the text with IRC formatting codes */
	p = text;
	segment_start = text;

	while (*p)
	{
		gboolean format_change = FALSE;
		gboolean insert_space = FALSE;  /* Insert space after segment (for tab separator) */
		char *format_pos = p;

		switch (*p)
		{
		case ATTR_BOLD:
			format_change = TRUE;
			p++;
			bold = !bold;
			break;

		case ATTR_ITALICS:
			format_change = TRUE;
			p++;
			italic = !italic;
			break;

		case ATTR_TAB:
			/* Tab is used as separator between nick and message - insert space */
			format_change = TRUE;
			insert_space = TRUE;
			p++;
			break;

		case ATTR_UNDERLINE:
			format_change = TRUE;
			p++;
			underline = !underline;
			break;

		case ATTR_STRIKETHROUGH:
			format_change = TRUE;
			p++;
			strikethrough = !strikethrough;
			break;

		case ATTR_HIDDEN:
			format_change = TRUE;
			p++;
			hidden = !hidden;
			break;

		case ATTR_REVERSE:
			format_change = TRUE;
			p++;
			reverse = !reverse;
			break;

		case ATTR_RESET:
			format_change = TRUE;
			p++;
			bold = italic = underline = strikethrough = hidden = reverse = FALSE;
			fg_color = bg_color = -1;
			break;

		case ATTR_COLOR:
			format_change = TRUE;
			p++;
			/* Parse foreground color (1 or 2 digits) */
			if (g_ascii_isdigit (*p))
			{
				fg_color = *p - '0';
				p++;
				if (g_ascii_isdigit (*p))
				{
					fg_color = fg_color * 10 + (*p - '0');
					p++;
				}
				/* Clamp to valid range (match GTK2 xtext behavior) */
				if (fg_color == 99)
					fg_color = -1;  /* 99 = default foreground */
				else if (fg_color > COL_MAX)
					fg_color = fg_color % MIRC_COLS;

				/* Check for background color */
				if (*p == ',')
				{
					p++;
					if (g_ascii_isdigit (*p))
					{
						bg_color = *p - '0';
						p++;
						if (g_ascii_isdigit (*p))
						{
							bg_color = bg_color * 10 + (*p - '0');
							p++;
						}
						/* Clamp to valid range */
						if (bg_color == 99)
							bg_color = -1;  /* 99 = default background */
						else if (bg_color > COL_MAX)
							bg_color = bg_color % MIRC_COLS;
					}
				}
			}
			else
			{
				/* Color code without numbers resets colors */
				fg_color = bg_color = -1;
			}
			break;

		case ATTR_BEEP:
		case ATTR_BLINK:
			/* Skip these control codes */
			format_change = TRUE;
			p++;
			break;

		default:
			p++;
			continue;
		}

		/* If we hit a format change, insert the text before it with previous tags */
		if (format_change && format_pos > segment_start)
		{
			/* Insert the segment */
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			start_offset = gtk_text_iter_get_offset (&end_iter);
			gtk_text_buffer_insert (gui->text_buffer, &end_iter, segment_start, 
			                        format_pos - segment_start);

			/* Apply tags to the inserted segment (using previous state) */
			gtk_text_buffer_get_iter_at_offset (gui->text_buffer, &start_iter, start_offset);
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);

			if (prev_bold)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "bold", &start_iter, &end_iter);
			if (prev_italic)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "italic", &start_iter, &end_iter);
			if (prev_underline)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "underline", &start_iter, &end_iter);
			if (prev_strikethrough)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "strikethrough", &start_iter, &end_iter);
			if (prev_hidden)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "hidden", &start_iter, &end_iter);
			if (prev_reverse)
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "reverse", &start_iter, &end_iter);
			if (prev_fg_color >= 0)
			{
				g_snprintf (tag_name, sizeof (tag_name), "fg-%02d", prev_fg_color);
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, tag_name, &start_iter, &end_iter);
			}
			if (prev_bg_color >= 0)
			{
				g_snprintf (tag_name, sizeof (tag_name), "bg-%02d", prev_bg_color);
				gtk_text_buffer_apply_tag_by_name (gui->text_buffer, tag_name, &start_iter, &end_iter);
			}
		}

		/* Insert a space if this was a tab separator */
		if (insert_space)
		{
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			gtk_text_buffer_insert (gui->text_buffer, &end_iter, " ", 1);
		}

		/* Update previous state to current state for next segment */
		prev_bold = bold;
		prev_italic = italic;
		prev_underline = underline;
		prev_strikethrough = strikethrough;
		prev_hidden = hidden;
		prev_reverse = reverse;
		prev_fg_color = fg_color;
		prev_bg_color = bg_color;

		segment_start = p;
	}

	/* Insert any remaining text */
	if (p > segment_start)
	{
		gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
		start_offset = gtk_text_iter_get_offset (&end_iter);
		gtk_text_buffer_insert (gui->text_buffer, &end_iter, segment_start, -1);

		/* Apply current formatting to final segment */
		gtk_text_buffer_get_iter_at_offset (gui->text_buffer, &start_iter, start_offset);
		gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);

		if (bold)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "bold", &start_iter, &end_iter);
		if (italic)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "italic", &start_iter, &end_iter);
		if (underline)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "underline", &start_iter, &end_iter);
		if (strikethrough)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "strikethrough", &start_iter, &end_iter);
		if (hidden)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "hidden", &start_iter, &end_iter);
		if (reverse)
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, "reverse", &start_iter, &end_iter);
		if (fg_color >= 0)
		{
			g_snprintf (tag_name, sizeof (tag_name), "fg-%02d", fg_color);
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, tag_name, &start_iter, &end_iter);
		}
		if (bg_color >= 0)
		{
			g_snprintf (tag_name, sizeof (tag_name), "bg-%02d", bg_color);
			gtk_text_buffer_apply_tag_by_name (gui->text_buffer, tag_name, &start_iter, &end_iter);
		}
	}

	/* Ensure text ends with a newline (scrollback replay strips newlines) */
	{
		gint len = strlen (text);
		if (len == 0 || text[len - 1] != '\n')
		{
			gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
			gtk_text_buffer_insert (gui->text_buffer, &end_iter, "\n", 1);
		}
	}

	/* Apply URL tags to detect clickable links */
	apply_url_tags_to_line (gui->text_buffer, line_start_offset, text);

	/* Auto-scroll to bottom if we were at the bottom */
	if (at_bottom)
	{
		gtk_text_buffer_get_end_iter (gui->text_buffer, &end_iter);
		end_mark = gtk_text_buffer_create_mark (gui->text_buffer, NULL, &end_iter, FALSE);
		/* Use scroll_to_mark with yalign=1.0 to ensure we scroll to the very bottom */
		gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (gui->text_view), end_mark,
		                              0.0,    /* within_margin - no margin */
		                              TRUE,   /* use_align */
		                              0.0,    /* xalign - left */
		                              1.0);   /* yalign - bottom */
		gtk_text_buffer_delete_mark (gui->text_buffer, end_mark);
	}
}

void
fe_text_clear (struct session *sess, int lines)
{
	session_gui *gui;
	GtkTextIter start, end;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (!gui->text_buffer)
		return;

	if (lines == 0)
	{
		/* Clear all */
		gtk_text_buffer_get_start_iter (gui->text_buffer, &start);
		gtk_text_buffer_get_end_iter (gui->text_buffer, &end);
		gtk_text_buffer_delete (gui->text_buffer, &start, &end);
	}
	else
	{
		/* Clear first N lines */
		gtk_text_buffer_get_start_iter (gui->text_buffer, &start);
		end = start;
		gtk_text_iter_forward_lines (&end, lines);
		gtk_text_buffer_delete (gui->text_buffer, &start, &end);
	}
}

void
fe_message (char *msg, int flags)
{
	AdwAlertDialog *dialog;
	const char *heading;

	if (flags & FE_MSG_ERROR)
		heading = _("Error");
	else if (flags & FE_MSG_WARN)
		heading = _("Warning");
	else if (flags & FE_MSG_INFO)
		heading = _("Information");
	else
		heading = _("HexChat");

	dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (heading, NULL));

	if (flags & FE_MSG_MARKUP)
		adw_alert_dialog_set_body_use_markup (dialog, TRUE);

	adw_alert_dialog_set_body (dialog, msg);

	adw_alert_dialog_add_response (dialog, "ok", _("OK"));
	adw_alert_dialog_set_default_response (dialog, "ok");
	adw_alert_dialog_set_close_response (dialog, "ok");

	if (flags & FE_MSG_WAIT)
	{
		/* Blocking: use choose with NULL callback for synchronous-like behavior.
		 * In GTK4 there's no direct gtk_dialog_run(); we present and let the
		 * main loop handle it. For FE_MSG_WAIT we'll use the blocking variant. */
		adw_alert_dialog_choose (dialog,
		                         main_window ? GTK_WIDGET (main_window) : NULL,
		                         NULL, NULL, NULL);
	}
	else
	{
		adw_alert_dialog_choose (dialog,
		                         main_window ? GTK_WIDGET (main_window) : NULL,
		                         NULL, NULL, NULL);
	}
}

/* ===== Channel/Topic display ===== */

void
fe_set_topic (struct session *sess, char *topic, char *stripped_topic)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->topic_entry)
	{
		const char *display_topic;

		if (prefs.hex_text_stripcolor_topic)
			display_topic = stripped_topic ? stripped_topic : "";
		else
			display_topic = topic ? topic : "";

		gtk_editable_set_text (GTK_EDITABLE (gui->topic_entry), display_topic);

		/* Set tooltip to full topic text */
		if (stripped_topic && stripped_topic[0])
			gtk_widget_set_tooltip_text (gui->topic_entry, stripped_topic);
		else
			gtk_widget_set_tooltip_text (gui->topic_entry, NULL);
	}
}

void
fe_set_channel (struct session *sess)
{
	session_gui *gui;
	const char *tab_title;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->sidebar_label)
	{
		/* Get valid UTF-8 title for sidebar */
		if (sess->channel[0] && g_utf8_validate (sess->channel, -1, NULL))
			tab_title = sess->channel;
		else
			tab_title = _("New Tab");

		gtk_label_set_text (GTK_LABEL (gui->sidebar_label), tab_title);
	}
}

void
fe_set_title (struct session *sess)
{
	char tbuf[512];
	int type;

	if (!sess || !sess->server)
		return;

	/* Only update if this is the currently visible session */
	if (sess != current_tab)
		return;

	type = sess->type;

	if (sess->server->connected == FALSE && type != SESS_DIALOG)
	{
		gtk_window_set_title (GTK_WINDOW (main_window), _(DISPLAY_NAME));
		return;
	}

	switch (type)
	{
	case SESS_DIALOG:
		g_snprintf (tbuf, sizeof (tbuf), "%s %s @ %s - %s",
		            _("Dialog with"), sess->channel,
		            server_get_network (sess->server, TRUE),
		            _(DISPLAY_NAME));
		break;
	case SESS_SERVER:
		g_snprintf (tbuf, sizeof (tbuf), "%s%s%s - %s",
		            prefs.hex_gui_win_nick ? sess->server->nick : "",
		            prefs.hex_gui_win_nick ? " @ " : "",
		            server_get_network (sess->server, TRUE),
		            _(DISPLAY_NAME));
		break;
	case SESS_CHANNEL:
		g_snprintf (tbuf, sizeof (tbuf), "%s%s%s / %s%s%s%s - %s",
		            prefs.hex_gui_win_nick ? sess->server->nick : "",
		            prefs.hex_gui_win_nick ? " @ " : "",
		            server_get_network (sess->server, TRUE),
		            sess->channel,
		            prefs.hex_gui_win_modes && sess->current_modes ? " (" : "",
		            prefs.hex_gui_win_modes && sess->current_modes ? sess->current_modes : "",
		            prefs.hex_gui_win_modes && sess->current_modes ? ")" : "",
		            _(DISPLAY_NAME));
		if (prefs.hex_gui_win_ucount)
		{
			g_snprintf (tbuf + strlen (tbuf), 9, " (%d)", sess->total);
		}
		break;
	case SESS_NOTICES:
	case SESS_SNOTICES:
		g_snprintf (tbuf, sizeof (tbuf), "%s%s%s (notices) - %s",
		            prefs.hex_gui_win_nick ? sess->server->nick : "",
		            prefs.hex_gui_win_nick ? " @ " : "",
		            server_get_network (sess->server, TRUE),
		            _(DISPLAY_NAME));
		break;
	default:
		g_snprintf (tbuf, sizeof (tbuf), "%s", _(DISPLAY_NAME));
		break;
	}

	if (main_window)
		gtk_window_set_title (GTK_WINDOW (main_window), tbuf);
}

void
fe_set_nonchannel (struct session *sess, int state)
{
	/* Not needed in GTK4 - the GTK2 version was also empty */
}

void
fe_clear_channel (struct session *sess)
{
	session_gui *gui;
	char tbuf[CHANLEN + 6];

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	/* Update sidebar label to show waiting channel or <none> */
	if (gui->sidebar_label)
	{
		if (sess->waitchannel[0])
		{
			g_snprintf (tbuf, sizeof (tbuf), "(%s)", sess->waitchannel);
		}
		else
		{
			g_strlcpy (tbuf, _("<none>"), sizeof (tbuf));
		}
		gtk_label_set_text (GTK_LABEL (gui->sidebar_label), tbuf);
	}

	/* Clear the topic entry */
	if (gui->topic_entry)
		gtk_editable_set_text (GTK_EDITABLE (gui->topic_entry), "");
}

void
fe_set_tab_color (struct session *sess, tabcolor col)
{
	session_gui *gui;
	int col_noflags;
	int col_shouldoverride;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	/* Don't change color of the active/focused tab */
	if (col != 0 && sess == current_tab)
		return;

	if (!gui->sidebar_label)
		return;

	col_noflags = (col & ~FE_COLOR_ALLFLAGS);
	col_shouldoverride = !(col & FE_COLOR_FLAG_NOOVERRIDE);

	/* Remove old color CSS classes */
	gtk_widget_remove_css_class (gui->sidebar_label, "hexchat-tab-newdata");
	gtk_widget_remove_css_class (gui->sidebar_label, "hexchat-tab-newmsg");
	gtk_widget_remove_css_class (gui->sidebar_label, "hexchat-tab-hilight");

	switch (col_noflags)
	{
	case 0: /* no particular color (theme default) */
		sess->tab_state = TAB_STATE_NONE;
		break;
	case 1: /* new data has been displayed */
		if (col_shouldoverride || !((sess->tab_state & TAB_STATE_NEW_MSG)
		                            || (sess->tab_state & TAB_STATE_NEW_HILIGHT)))
		{
			sess->tab_state = TAB_STATE_NEW_DATA;
			gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-newdata");
		}
		else
		{
			/* Restore the higher-priority class */
			if (sess->tab_state & TAB_STATE_NEW_HILIGHT)
				gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-hilight");
			else if (sess->tab_state & TAB_STATE_NEW_MSG)
				gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-newmsg");
		}
		break;
	case 2: /* new message arrived in channel */
		if (col_shouldoverride || !(sess->tab_state & TAB_STATE_NEW_HILIGHT))
		{
			sess->tab_state = TAB_STATE_NEW_MSG;
			gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-newmsg");
		}
		else
		{
			/* Restore highlight class */
			gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-hilight");
		}
		break;
	case 3: /* your nick has been seen (highlight) */
		sess->tab_state = TAB_STATE_NEW_HILIGHT;
		gtk_widget_add_css_class (gui->sidebar_label, "hexchat-tab-hilight");
		break;
	}

	sess->last_tab_state = sess->tab_state;
}

/* ===== Notifications ===== */

static GDBusProxy *fdo_notifications = NULL;

static void
init_notifications (void)
{
	GError *error = NULL;

	if (fdo_notifications)
		return;

	fdo_notifications = g_dbus_proxy_new_for_bus_sync (
		G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_NONE,
		NULL,
		"org.freedesktop.Notifications",
		"/org/freedesktop/Notifications",
		"org.freedesktop.Notifications",
		NULL,
		&error);

	if (error)
	{
		g_warning ("Failed to connect to notification daemon: %s", error->message);
		g_error_free (error);
	}
}

static void
show_notification (const char *title, const char *body)
{
	GVariantBuilder builder;

	if (!notifications_enabled)
		return;

	/* Ensure we have valid UTF-8 strings */
	if (!title || !title[0] || !g_utf8_validate (title, -1, NULL))
		title = "HexChat";
	if (!body || !body[0] || !g_utf8_validate (body, -1, NULL))
		body = "New activity";

	/* Initialize notifications if needed */
	if (!fdo_notifications)
		init_notifications ();

	if (!fdo_notifications)
		return;

	/* Build the notification call */
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(susssasa{sv}i)"));
	g_variant_builder_add (&builder, "s", "HexChat");  /* app_name */
	g_variant_builder_add (&builder, "u", 0);          /* replaces_id */
	g_variant_builder_add (&builder, "s", "hexchat");  /* app_icon */
	g_variant_builder_add (&builder, "s", title);      /* summary */
	g_variant_builder_add (&builder, "s", body);       /* body */

	/* actions (empty array) */
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_close (&builder);

	/* hints (empty dict) */
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_close (&builder);

	g_variant_builder_add (&builder, "i", 5000);       /* expire_timeout (ms) */

	g_dbus_proxy_call (fdo_notifications,
	                   "Notify",
	                   g_variant_builder_end (&builder),
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   NULL,
	                   NULL);
}

void
fe_flash_window (struct session *sess)
{
	const char *title;
	const char *network;
	const char *channel;
	char *body;
	gboolean window_focused = FALSE;

	/* Check if window is focused */
	if (main_window)
		window_focused = gtk_window_is_active (GTK_WINDOW (main_window));

	/* Don't notify if window is already focused */
	if (window_focused)
		return;

	/* Show desktop notification */
	if (sess)
	{
		/* Determine notification title based on session type */
		if (sess->type == SESS_DIALOG)
			title = "Private Message";
		else if (sess->type == SESS_CHANNEL)
			title = "Channel Activity";
		else
			title = "HexChat";

		/* Get channel/query name safely with UTF-8 validation */
		channel = sess->channel;
		if (!channel || !channel[0] || !g_utf8_validate (channel, -1, NULL))
			channel = "Unknown";

		/* Build notification body */
		network = sess->server ? sess->server->network : NULL;
		if (network && !g_utf8_validate (network, -1, NULL))
			network = NULL;
		if (!network && sess->server)
		{
			network = sess->server->servername;
			if (network && !g_utf8_validate (network, -1, NULL))
				network = NULL;
		}

		if (network && network[0])
			body = g_strdup_printf ("%s on %s", channel, network);
		else
			body = g_strdup (channel);

		show_notification (title, body);
		g_free (body);
	}
}

void
fe_update_mode_buttons (struct session *sess, char mode, char sign)
{
	/* TODO: Update channel mode buttons */
}

void
fe_update_channel_key (struct session *sess)
{
	/* TODO: Update channel key display */
}

void
fe_update_channel_limit (struct session *sess)
{
	/* TODO: Update channel limit display */
}

/* ===== User list ===== */

/* Helper function to find a user item in the store by User pointer */
static guint
find_user_in_store (GListStore *store, struct User *user, UserItem **found_item)
{
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (store));
	guint i;

	for (i = 0; i < n_items; i++)
	{
		UserItem *item = g_list_model_get_item (G_LIST_MODEL (store), i);
		if (item && item->user == user)
		{
			if (found_item)
				*found_item = item;
			else
				g_object_unref (item);
			return i;
		}
		if (item)
			g_object_unref (item);
	}
	return G_MAXUINT;
}

void
fe_userlist_insert (struct session *sess, struct User *newuser, gboolean sel)
{
	session_gui *gui;
	UserItem *item;

	if (!sess || !sess->gui || !newuser)
		return;

	gui = sess->gui;

	if (!gui->userlist_store)
		return;

	/* Create a new UserItem and add to store */
	item = user_item_new (newuser);
	
	/* Insert sorted by nick (simple alphabetical for now) */
	/* TODO: Respect ops/voices sorting preference */
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (gui->userlist_store));
	guint insert_pos = n_items;
	guint i;

	for (i = 0; i < n_items; i++)
	{
		UserItem *existing = g_list_model_get_item (G_LIST_MODEL (gui->userlist_store), i);
		if (existing)
		{
			/* Sort: ops first, then halfops, then voiced, then regular */
			int new_rank = (newuser->op ? 3 : 0) + (newuser->hop ? 2 : 0) + (newuser->voice ? 1 : 0);
			int existing_rank = (existing->is_op ? 3 : 0) + (existing->is_hop ? 2 : 0) + (existing->is_voice ? 1 : 0);

			if (new_rank > existing_rank ||
			    (new_rank == existing_rank && g_ascii_strcasecmp (item->nick, existing->nick) < 0))
			{
				g_object_unref (existing);
				insert_pos = i;
				break;
			}
			g_object_unref (existing);
		}
	}

	g_list_store_insert (gui->userlist_store, insert_pos, item);
	g_object_unref (item);
}

int
fe_userlist_remove (struct session *sess, struct User *user)
{
	session_gui *gui;
	guint pos;

	if (!sess || !sess->gui || !user)
		return 0;

	gui = sess->gui;

	if (!gui->userlist_store)
		return 0;

	pos = find_user_in_store (gui->userlist_store, user, NULL);
	if (pos != G_MAXUINT)
	{
		g_list_store_remove (gui->userlist_store, pos);
		return 1;
	}
	return 0;
}

void
fe_userlist_rehash (struct session *sess, struct User *user)
{
	/* Rehash is called when user modes change - re-sort the user */
	session_gui *gui;
	UserItem *item;
	guint pos;

	if (!sess || !sess->gui || !user)
		return;

	gui = sess->gui;

	if (!gui->userlist_store)
		return;

	/* Find the user */
	pos = find_user_in_store (gui->userlist_store, user, &item);
	if (pos != G_MAXUINT && item)
	{
		/* Update the item data */
		user_item_update (item, user);
		g_object_unref (item);

		/* Remove and re-insert to maintain sort order */
		g_list_store_remove (gui->userlist_store, pos);
		fe_userlist_insert (sess, user, FALSE);
	}
}

void
fe_userlist_update (struct session *sess, struct User *user)
{
	/* Update is called when user info changes (e.g., away status) */
	session_gui *gui;
	UserItem *item;
	guint pos;

	if (!sess || !sess->gui || !user)
		return;

	gui = sess->gui;

	if (!gui->userlist_store)
		return;

	pos = find_user_in_store (gui->userlist_store, user, &item);
	if (pos != G_MAXUINT && item)
	{
		user_item_update (item, user);
		g_object_unref (item);
		/* Signal the model that the item changed */
		/* GListStore doesn't have a direct "item-changed" signal, 
		 * so we remove and re-add at the same position */
		g_list_store_remove (gui->userlist_store, pos);
		UserItem *new_item = user_item_new (user);
		g_list_store_insert (gui->userlist_store, pos, new_item);
		g_object_unref (new_item);
	}
}

void
fe_userlist_numbers (struct session *sess)
{
	session_gui *gui;
	char tbuf[256];

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->usercount_label)
	{
		if (sess->total)
		{
			g_snprintf (tbuf, sizeof (tbuf), _("%d ops, %d total"),
			            sess->ops, sess->total);
			gtk_label_set_text (GTK_LABEL (gui->usercount_label), tbuf);
		}
		else
		{
			gtk_label_set_text (GTK_LABEL (gui->usercount_label), "");
		}
	}

	/* Update titlebar user count if enabled */
	if (sess->type == SESS_CHANNEL && prefs.hex_gui_win_ucount)
		fe_set_title (sess);
}

void
fe_userlist_clear (struct session *sess)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->userlist_store)
		g_list_store_remove_all (gui->userlist_store);
}

void
fe_userlist_set_selected (struct session *sess)
{
	/* TODO: Sync selection state from User->selected to GtkSelectionModel */
}

void
fe_uselect (session *sess, char *word[], int do_clear, int scroll_to)
{
	/* TODO: Select users by name - used by /uselect command */
}

/* ===== DCC ===== */

void
fe_dcc_add (struct DCC *dcc)
{
	/* TODO: Add DCC to GUI */
}

void
fe_dcc_update (struct DCC *dcc)
{
	/* TODO: Update DCC in GUI */
}

void
fe_dcc_remove (struct DCC *dcc)
{
	/* TODO: Remove DCC from GUI */
}

int
fe_dcc_open_recv_win (int passive)
{
	/* TODO: Open DCC receive window */
	return FALSE;
}

int
fe_dcc_open_send_win (int passive)
{
	/* TODO: Open DCC send window */
	return FALSE;
}

int
fe_dcc_open_chat_win (int passive)
{
	/* TODO: Open DCC chat window */
	return FALSE;
}

void
fe_dcc_send_filereq (struct session *sess, char *nick, int maxcps, int passive)
{
	/* TODO: Open file request dialog */
}

/* ===== Server state ===== */

void
fe_set_nick (struct server *serv, char *newnick)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (sess->gui && sess->gui->nick_label)
				gtk_button_set_label (GTK_BUTTON (sess->gui->nick_label),
				                      newnick ? newnick : "");
		}
		list = list->next;
	}
}

void
fe_set_lag (server *serv, long lag)
{
	GSList *list = sess_list;
	session *sess;
	char lagtext[64];
	unsigned long nowtim;

	if (lag == -1)
	{
		if (!serv->lag_sent)
			return;
		nowtim = make_ping_time ();
		lag = nowtim - serv->lag_sent;
	}

	/* Cap at 30 seconds */
	if (lag > 30000 && serv->lag_sent)
		lag = 30000;

	g_snprintf (lagtext, sizeof (lagtext), "%s%ld.%lds",
	            serv->lag_sent ? "+" : "", lag / 1000, (lag / 100) % 10);

	while (list)
	{
		sess = list->data;
		if (sess->server == serv && sess->gui && sess->gui->lag_label)
		{
			gtk_label_set_text (GTK_LABEL (sess->gui->lag_label), lagtext);
			gtk_widget_set_tooltip_text (sess->gui->lag_label,
			                             serv->lag_sent ? _("Lag (waiting for pong)") : _("Lag"));
		}
		list = list->next;
	}
}

void
fe_set_throttle (server *serv)
{
	/* TODO: Update throttle meter */
}

void
fe_set_away (server *serv)
{
	GSList *list = sess_list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv && sess->gui && sess->gui->nick_label)
		{
			if (serv->is_away)
				gtk_widget_add_css_class (sess->gui->nick_label, "hexchat-nick-away");
			else
				gtk_widget_remove_css_class (sess->gui->nick_label, "hexchat-nick-away");
		}
		list = list->next;
	}
}

void
fe_server_event (server *serv, int type, int arg)
{
	/* TODO: Handle server events */
}

void
fe_progressbar_start (struct session *sess)
{
	/* TODO: Show connection progress */
}

void
fe_progressbar_end (struct server *serv)
{
	/* TODO: Hide connection progress */
}

/* ===== Dialogs ===== */

void
fe_serverlist_open (session *sess)
{
	servlist_open (sess);
}

/* Response callback for fe_get_bool dialog */
static void
get_bool_response_cb (AdwAlertDialog *dialog, const char *response,
                      gpointer user_data)
{
	void (*callback) (int value, void *user_data);
	void *ud;

	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	ud = g_object_get_data (G_OBJECT (dialog), "ud");

	if (callback)
	{
		if (!g_strcmp0 (response, "yes"))
			callback (1, ud);
		else
			callback (0, ud);
	}
}

void
fe_get_bool (char *title, char *prompt, void *callback, void *userdata)
{
	AdwAlertDialog *dialog;

	dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (title, prompt));

	adw_alert_dialog_add_responses (dialog,
	                                "no", _("No"),
	                                "yes", _("Yes"),
	                                NULL);
	adw_alert_dialog_set_default_response (dialog, "yes");
	adw_alert_dialog_set_close_response (dialog, "no");

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (get_bool_response_cb), NULL);

	adw_alert_dialog_choose (dialog,
	                         main_window ? GTK_WIDGET (main_window) : NULL,
	                         NULL, NULL, NULL);
}

/* Response callback for fe_get_str dialog */
static void
get_str_response_cb (AdwAlertDialog *dialog, const char *response,
                     gpointer user_data)
{
	void (*callback) (int cancel, char *text, void *user_data);
	GtkWidget *entry;
	void *ud;

	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	ud = g_object_get_data (G_OBJECT (dialog), "ud");
	entry = g_object_get_data (G_OBJECT (dialog), "entry");

	if (callback)
	{
		if (!g_strcmp0 (response, "ok") && entry)
		{
			const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
			callback (FALSE, (char *) text, ud);
		}
		else
		{
			callback (TRUE, "", ud);
		}
	}
}

void
fe_get_str (char *prompt, char *def, void *callback, void *ud)
{
	AdwAlertDialog *dialog;
	GtkWidget *entry;

	dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (prompt, NULL));

	entry = gtk_entry_new ();
	if (def)
		gtk_editable_set_text (GTK_EDITABLE (entry), def);
	gtk_widget_set_margin_start (entry, 12);
	gtk_widget_set_margin_end (entry, 12);
	adw_alert_dialog_set_extra_child (dialog, entry);

	adw_alert_dialog_add_responses (dialog,
	                                "cancel", _("Cancel"),
	                                "ok", _("OK"),
	                                NULL);
	adw_alert_dialog_set_default_response (dialog, "ok");
	adw_alert_dialog_set_close_response (dialog, "cancel");

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", ud);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (get_str_response_cb), NULL);

	adw_alert_dialog_choose (dialog,
	                         main_window ? GTK_WIDGET (main_window) : NULL,
	                         NULL, NULL, NULL);
}

/* Response callback for fe_get_int dialog */
static void
get_int_response_cb (AdwAlertDialog *dialog, const char *response,
                     gpointer user_data)
{
	void (*callback) (int cancel, int value, void *user_data);
	GtkWidget *spin;
	void *ud;

	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	ud = g_object_get_data (G_OBJECT (dialog), "ud");
	spin = g_object_get_data (G_OBJECT (dialog), "spin");

	if (callback)
	{
		int value = 0;
		if (spin)
			value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));

		if (!g_strcmp0 (response, "ok"))
			callback (FALSE, value, ud);
		else
			callback (TRUE, value, ud);
	}
}

void
fe_get_int (char *prompt, int def, void *callback, void *ud)
{
	AdwAlertDialog *dialog;
	GtkWidget *spin;
	GtkAdjustment *adj;

	dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (prompt, NULL));

	adj = gtk_adjustment_new (def, 0, 1024, 1, 10, 0);
	spin = gtk_spin_button_new (adj, 1, 0);
	gtk_widget_set_margin_start (spin, 12);
	gtk_widget_set_margin_end (spin, 12);
	adw_alert_dialog_set_extra_child (dialog, spin);

	adw_alert_dialog_add_responses (dialog,
	                                "cancel", _("Cancel"),
	                                "ok", _("OK"),
	                                NULL);
	adw_alert_dialog_set_default_response (dialog, "ok");
	adw_alert_dialog_set_close_response (dialog, "cancel");

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", ud);
	g_object_set_data (G_OBJECT (dialog), "spin", spin);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (get_int_response_cb), NULL);

	adw_alert_dialog_choose (dialog,
	                         main_window ? GTK_WIDGET (main_window) : NULL,
	                         NULL, NULL, NULL);
}

/* Callback data for fe_get_file async operations */
typedef struct
{
	void (*callback) (void *userdata, char *file);
	void *userdata;
} FileReqData;

static void
file_dialog_open_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	FileReqData *data = user_data;
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	GFile *file;

	file = gtk_file_dialog_open_finish (dialog, result, NULL);
	if (file)
	{
		char *path = g_file_get_path (file);
		if (data->callback)
			data->callback (data->userdata, path);
		g_free (path);
		/* Call again with NULL to signal completion */
		if (data->callback)
			data->callback (data->userdata, NULL);
		g_object_unref (file);
	}
	else
	{
		/* Cancelled */
		if (data->callback)
			data->callback (data->userdata, NULL);
	}

	g_free (data);
}

static void
file_dialog_save_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	FileReqData *data = user_data;
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	GFile *file;

	file = gtk_file_dialog_save_finish (dialog, result, NULL);
	if (file)
	{
		char *path = g_file_get_path (file);
		if (data->callback)
			data->callback (data->userdata, path);
		g_free (path);
		if (data->callback)
			data->callback (data->userdata, NULL);
		g_object_unref (file);
	}
	else
	{
		if (data->callback)
			data->callback (data->userdata, NULL);
	}

	g_free (data);
}

static void
file_dialog_folder_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	FileReqData *data = user_data;
	GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
	GFile *file;

	file = gtk_file_dialog_select_folder_finish (dialog, result, NULL);
	if (file)
	{
		char *path = g_file_get_path (file);
		if (data->callback)
			data->callback (data->userdata, path);
		g_free (path);
		if (data->callback)
			data->callback (data->userdata, NULL);
		g_object_unref (file);
	}
	else
	{
		if (data->callback)
			data->callback (data->userdata, NULL);
	}

	g_free (data);
}

void
fe_get_file (const char *title, char *initial,
             void (*callback) (void *userdata, char *file), void *userdata,
             int flags)
{
	GtkFileDialog *dialog;
	FileReqData *data;

	dialog = gtk_file_dialog_new ();
	if (title)
		gtk_file_dialog_set_title (dialog, title);

	/* Set initial directory/file */
	if (initial && initial[0])
	{
		GFile *init_file = g_file_new_for_path (initial);
		if (flags & FRF_FILTERISINITIAL)
		{
			GFile *parent = g_file_get_parent (init_file);
			if (parent)
			{
				gtk_file_dialog_set_initial_folder (dialog, parent);
				g_object_unref (parent);
			}
			char *basename = g_file_get_basename (init_file);
			if (basename)
			{
				gtk_file_dialog_set_initial_name (dialog, basename);
				g_free (basename);
			}
		}
		else
		{
			gtk_file_dialog_set_initial_folder (dialog, init_file);
		}
		g_object_unref (init_file);
	}

	data = g_new0 (FileReqData, 1);
	data->callback = callback;
	data->userdata = userdata;

	if (flags & FRF_CHOOSEFOLDER)
	{
		gtk_file_dialog_select_folder (dialog,
		                               main_window ? GTK_WINDOW (main_window) : NULL,
		                               NULL, file_dialog_folder_cb, data);
	}
	else if (flags & FRF_WRITE)
	{
		gtk_file_dialog_save (dialog,
		                      main_window ? GTK_WINDOW (main_window) : NULL,
		                      NULL, file_dialog_save_cb, data);
	}
	else
	{
		gtk_file_dialog_open (dialog,
		                      main_window ? GTK_WINDOW (main_window) : NULL,
		                      NULL, file_dialog_open_cb, data);
	}

	g_object_unref (dialog);
}

void
fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud)
{
	/* Used by DCC - open a save-as dialog.
	 * Following GTK2 behavior: ignores yes/no procs, opens file dialog. */
	struct DCC *dcc = ud;

	if (dcc->file)
	{
		char *filepath = g_build_filename (prefs.hex_dcc_dir, dcc->file, NULL);
		fe_get_file (message, filepath,
		             (void (*)(void *, char *)) yesproc, ud,
		             FRF_WRITE | FRF_NOASKOVERWRITE | FRF_FILTERISINITIAL);
		g_free (filepath);
	}
}

/* ===== Input box ===== */

char *
fe_get_inputbox_contents (struct session *sess)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return NULL;

	gui = sess->gui;

	if (gui->input_entry)
		return g_strdup (gtk_editable_get_text (GTK_EDITABLE (gui->input_entry)));

	return NULL;
}

void
fe_set_inputbox_contents (struct session *sess, char *text)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->input_entry)
		gtk_editable_set_text (GTK_EDITABLE (gui->input_entry), text ? text : "");
}

int
fe_get_inputbox_cursor (struct session *sess)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return 0;

	gui = sess->gui;

	if (gui->input_entry)
		return gtk_editable_get_position (GTK_EDITABLE (gui->input_entry));

	return 0;
}

void
fe_set_inputbox_cursor (struct session *sess, int delta, int pos)
{
	session_gui *gui;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->input_entry)
	{
		if (delta)
			pos += gtk_editable_get_position (GTK_EDITABLE (gui->input_entry));
		gtk_editable_set_position (GTK_EDITABLE (gui->input_entry), pos);
	}
}

/* ===== Miscellaneous ===== */

void
fe_beep (session *sess)
{
	GdkDisplay *display = gdk_display_get_default ();
	if (display)
		gdk_display_beep (display);
}

void
fe_open_url (const char *url)
{
	GtkWidget *window = main_window;
	if (window)
	{
		GtkUriLauncher *launcher = gtk_uri_launcher_new (url);
		gtk_uri_launcher_launch (launcher, GTK_WINDOW (window), NULL, NULL, NULL);
		g_object_unref (launcher);
	}
}

void
fe_pluginlist_update (void)
{
	/* TODO: Update plugin list display */
}

void
fe_buttons_update (struct session *sess)
{
	/* TODO: Update userlist buttons */
}

void
fe_dlgbuttons_update (struct session *sess)
{
	/* TODO: Update dialog buttons */
}

void
fe_url_add (const char *text)
{
	/* TODO: Add URL to URL grabber */
}

char *
fe_menu_add (menu_entry *me)
{
	/* TODO: Add menu entry */
	return NULL;
}

void
fe_menu_del (menu_entry *me)
{
	/* TODO: Delete menu entry */
}

void
fe_menu_update (menu_entry *me)
{
	/* TODO: Update menu entry */
}

void
fe_add_rawlog (struct server *serv, char *text, int len, int outbound)
{
	/* TODO: Add to raw log window */
}

void
fe_ignore_update (int level)
{
	/* TODO: Update ignore list display */
}

void
fe_notify_update (char *name)
{
	/* TODO: Update notify list */
}

void
fe_notify_ask (char *name, char *networks)
{
	/* TODO: Ask to add to notify list */
}

/* Implemented in chanlist.c */
/* fe_is_chanwindow, fe_add_chan_list, fe_chan_list_end are in chanlist.c */

gboolean
fe_add_ban_list (struct session *sess, char *mask, char *who, char *when, int rplcode)
{
	/* TODO: Add to ban list */
	return FALSE;
}

gboolean
fe_ban_list_end (struct session *sess, int rplcode)
{
	/* TODO: Ban list complete */
	return FALSE;
}

void
fe_open_chan_list (server *serv, char *filter, int do_refresh)
{
	chanlist_opengui (serv, do_refresh);
}

void
fe_lastlog (session *sess, session *lastlog_sess, char *sstr, gtk_xtext_search_flags flags)
{
	session_gui *src_gui, *dst_gui;
	GtkTextIter line_start, line_end;
	gboolean use_regex, match_case;
	GRegex *regex = NULL;

	if (!sess || !sess->gui || !lastlog_sess || !lastlog_sess->gui)
		return;

	src_gui = sess->gui;
	dst_gui = lastlog_sess->gui;

	if (!src_gui->text_buffer || !dst_gui->text_buffer)
		return;

	use_regex = (flags & regexp) != 0;
	match_case = (flags & case_match) != 0;

	if (use_regex)
	{
		GRegexCompileFlags gcf = match_case ? 0 : G_REGEX_CASELESS;
		GError *err = NULL;

		regex = g_regex_new (sstr, gcf, 0, &err);
		if (!regex)
		{
			if (err)
			{
				PrintText (lastlog_sess, _(err->message));
				g_error_free (err);
			}
			return;
		}
	}

	/* Iterate through each line of the source buffer */
	gtk_text_buffer_get_start_iter (src_gui->text_buffer, &line_start);

	while (!gtk_text_iter_is_end (&line_start))
	{
		char *line_text;
		gboolean matched = FALSE;

		line_end = line_start;
		gtk_text_iter_forward_to_line_end (&line_end);

		line_text = gtk_text_buffer_get_text (src_gui->text_buffer,
		                                      &line_start, &line_end, FALSE);

		if (line_text && line_text[0])
		{
			if (use_regex)
			{
				matched = g_regex_match (regex, line_text, 0, NULL);
			}
			else
			{
				if (match_case)
					matched = (strstr (line_text, sstr) != NULL);
				else
				{
					char *hay = g_utf8_casefold (line_text, -1);
					char *nee = g_utf8_casefold (sstr, -1);
					matched = (strstr (hay, nee) != NULL);
					g_free (hay);
					g_free (nee);
				}
			}

			if (matched)
			{
				/* Append the matching line to the lastlog session */
				GtkTextIter dst_end;
				gtk_text_buffer_get_end_iter (dst_gui->text_buffer, &dst_end);

				if (gtk_text_buffer_get_char_count (dst_gui->text_buffer) > 0)
					gtk_text_buffer_insert (dst_gui->text_buffer, &dst_end, "\n", 1);

				gtk_text_buffer_insert (dst_gui->text_buffer, &dst_end,
				                        line_text, -1);
			}
		}

		g_free (line_text);

		/* Move to next line */
		if (!gtk_text_iter_forward_line (&line_start))
			break;
	}

	if (regex)
		g_regex_unref (regex);
}

void
fe_ctrl_gui (session *sess, fe_gui_action action, int arg)
{
	switch (action)
	{
	case FE_GUI_FOCUS:
		if (sess && sess->gui)
		{
			session_gui *gui = sess->gui;
			current_sess = sess;
			current_tab = sess;
			if (sess->server)
				sess->server->front_session = sess;
			if (gui->sidebar_row && channel_sidebar)
				gtk_list_box_select_row (GTK_LIST_BOX (channel_sidebar),
				                         GTK_LIST_BOX_ROW (gui->sidebar_row));
		}
		break;
	case FE_GUI_SHOW:
		if (main_window)
			gtk_widget_set_visible (main_window, TRUE);
		break;
	case FE_GUI_HIDE:
		if (main_window)
			gtk_widget_set_visible (main_window, FALSE);
		break;
	case FE_GUI_FLASH:
		fe_flash_window (sess);
		break;
	case FE_GUI_ICONIFY:
		if (main_window)
			gtk_window_minimize (GTK_WINDOW (main_window));
		break;
	default:
		break;
	}
}

int
fe_gui_info (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0: /* window status */
		if (!main_window || !gtk_widget_get_visible (main_window))
			return 2; /* hidden */

		if (gtk_window_is_active (GTK_WINDOW (main_window)))
			return 1; /* active/focused */

		return 0; /* normal (not focused) */
	}

	return -1;
}

void *
fe_gui_info_ptr (session *sess, int info_type)
{
	switch (info_type)
	{
	case 0: /* native window pointer */
	case 1: /* GtkWindow * */
		return main_window;
	}
	return NULL;
}

const char *
fe_get_default_font (void)
{
	return "Monospace 10";
}

/* ===== Tray icon / Status indication ===== */

/* On Wayland/GTK4, there's no traditional system tray.
 * Instead, we use:
 * - Notification priority (urgent for highlights/PMs)
 * - Window urgency hint
 */

void
fe_tray_set_flash (const char *filename1, const char *filename2, int timeout)
{
	/* Flash is handled through window urgency and notifications */
}

void
fe_tray_set_file (const char *filename)
{
	/* Custom tray icon not supported on Wayland */
}

void
fe_tray_set_icon (feicon icon)
{
	current_tray_icon = icon;

	/* Set window urgency hint when there's activity */
	if (main_window && icon != FE_ICON_NORMAL)
	{
		/* GTK4 doesn't have urgency hint directly, but we can request attention */
		if (!gtk_window_is_active (GTK_WINDOW (main_window)))
		{
			/* The window manager should show this in taskbar/dock */
			gtk_window_present (GTK_WINDOW (main_window));
		}
	}
}

void
fe_tray_set_tooltip (const char *text)
{
	/* Tooltip not applicable without tray icon */
}
