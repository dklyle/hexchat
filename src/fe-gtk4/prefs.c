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

#include <string.h>
#include <adwaita.h>

#include "../common/hexchat.h"
#include "../common/hexchatc.h"
#include "../common/cfgfiles.h"
#include "../common/text.h"

#include "fe-gtk4.h"

/* Forward declarations */
static void prefs_save_settings (void);

/* Global preferences window */
static GtkWidget *prefs_window = NULL;

/* ===== Preference callbacks ===== */

/* Toggle preference callback */
static void
toggle_pref_changed (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
	unsigned int *pref = user_data;
	gboolean active;

	g_object_get (obj, "active", &active, NULL);
	*pref = active ? 1 : 0;
}

/* Integer preference callback (from spin row) */
static void
int_pref_changed (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
	int *pref = user_data;
	double value;

	g_object_get (obj, "value", &value, NULL);
	*pref = (int)value;
}

/* String preference callback (from entry row) */
static void
string_pref_changed (GtkEditable *editable, gpointer user_data)
{
	char *pref = user_data;
	const char *text;

	text = gtk_editable_get_text (editable);
	if (text)
	{
		/* Safely copy the string */
		g_strlcpy (pref, text, 256);  /* Most string prefs are 256 bytes */
	}
}

/* ===== Helper functions to create preference rows ===== */

/* Create a switch row for boolean preference */
static GtkWidget *
create_switch_row (const char *title, const char *subtitle, unsigned int *pref)
{
	GtkWidget *row;

	row = adw_switch_row_new ();
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
	if (subtitle)
		adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
	adw_switch_row_set_active (ADW_SWITCH_ROW (row), *pref != 0);

	g_signal_connect (row, "notify::active",
	                  G_CALLBACK (toggle_pref_changed), pref);

	return row;
}

/* Create a spin row for integer preference */
static GtkWidget *
create_spin_row (const char *title, const char *subtitle, int *pref,
                 int min_val, int max_val, int step)
{
	GtkWidget *row;
	GtkAdjustment *adj;

	adj = gtk_adjustment_new (*pref, min_val, max_val, step, step * 10, 0);

	row = adw_spin_row_new (adj, step, 0);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
	if (subtitle)
		adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);

	g_signal_connect (row, "notify::value",
	                  G_CALLBACK (int_pref_changed), pref);

	return row;
}

/* Create an entry row for string preference */
static GtkWidget *
create_entry_row (const char *title, const char *subtitle, char *pref, int max_len)
{
	GtkWidget *row;

	row = adw_entry_row_new ();
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
	gtk_editable_set_text (GTK_EDITABLE (row), pref ? pref : "");

	g_signal_connect (row, "changed",
	                  G_CALLBACK (string_pref_changed), pref);

	return row;
}

/* ===== Preference pages ===== */

/* Create Interface preferences page */
static GtkWidget *
create_interface_page (void)
{
	GtkWidget *page;
	GtkWidget *group;
	GtkWidget *row;

	page = adw_preferences_page_new ();
	adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Interface");
	adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page), "preferences-desktop-appearance-symbolic");

	/* General group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "General");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Show topic bar", "Display the channel topic in a bar", 
	                         &prefs.hex_gui_topicbar);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Show user list", NULL,
	                         &prefs.hex_gui_ulist_hide);
	/* Invert the logic - the pref is "hide", but we show "show" */
	adw_switch_row_set_active (ADW_SWITCH_ROW (row), prefs.hex_gui_ulist_hide == 0);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Show user count", "Display number of users in title",
	                         &prefs.hex_gui_win_ucount);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Show channel modes", "Display channel modes in title",
	                         &prefs.hex_gui_win_modes);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* Tabs group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Tabs");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Show tab icons", "Display icons next to tab names",
	                         &prefs.hex_gui_tab_icons);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Sort tabs alphabetically", NULL,
	                         &prefs.hex_gui_tab_sort);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Open channels in tabs", NULL,
	                         &prefs.hex_gui_tab_chans);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Open dialogs in tabs", NULL,
	                         &prefs.hex_gui_tab_dialogs);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	return page;
}

/* Create Text Display preferences page */
static GtkWidget *
create_chatting_page (void)
{
	GtkWidget *page;
	GtkWidget *group;
	GtkWidget *row;

	page = adw_preferences_page_new ();
	adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Chatting");
	adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page), "user-available-symbolic");

	/* Timestamps group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Timestamps");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Show timestamps", "Display time next to messages",
	                         &prefs.hex_stamp_text);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("Timestamp format", "strftime format string",
	                        prefs.hex_stamp_text_format, sizeof(prefs.hex_stamp_text_format));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* Text Display group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Text Display");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Colored nick names", "Give each person a different color",
	                         &prefs.hex_text_color_nicks);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Indent nick names", "Right-justify nick names",
	                         &prefs.hex_text_indent);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Show marker line", "Insert a line after last read text",
	                         &prefs.hex_text_show_marker);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Word wrap", "Wrap long lines",
	                         &prefs.hex_text_wordwrap);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_spin_row ("Max lines", "Maximum lines in text buffer",
	                       &prefs.hex_text_max_lines, 100, 100000, 100);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* Nick Completion group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Nick Completion");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_entry_row ("Completion suffix", "Added after nick completion",
	                        prefs.hex_completion_suffix, sizeof(prefs.hex_completion_suffix));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Auto-complete", "Complete nicks automatically when typing",
	                         &prefs.hex_completion_auto);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	return page;
}

/* Create Network preferences page */
static GtkWidget *
create_network_page (void)
{
	GtkWidget *page;
	GtkWidget *group;
	GtkWidget *row;

	page = adw_preferences_page_new ();
	adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Network");
	adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page), "network-wired-symbolic");

	/* Connection group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Connection");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Auto-reconnect", "Automatically reconnect on disconnect",
	                         &prefs.hex_net_auto_reconnect);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_spin_row ("Reconnect delay", "Seconds to wait before reconnecting",
	                       &prefs.hex_net_reconnect_delay, 0, 600, 5);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_spin_row ("Ping timeout", "Seconds before considering connection dead",
	                       &prefs.hex_net_ping_timeout, 30, 600, 30);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* Identity group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Identity");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_entry_row ("Nick name", NULL,
	                        prefs.hex_irc_nick1, sizeof(prefs.hex_irc_nick1));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("Second choice", NULL,
	                        prefs.hex_irc_nick2, sizeof(prefs.hex_irc_nick2));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("Third choice", NULL,
	                        prefs.hex_irc_nick3, sizeof(prefs.hex_irc_nick3));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("User name", NULL,
	                        prefs.hex_irc_user_name, sizeof(prefs.hex_irc_user_name));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("Real name", NULL,
	                        prefs.hex_irc_real_name, sizeof(prefs.hex_irc_real_name));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* IRC Behavior group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "IRC Behavior");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Auto-rejoin on kick", NULL,
	                         &prefs.hex_irc_auto_rejoin);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Rejoin on reconnect", "Rejoin channels after reconnecting",
	                         &prefs.hex_irc_reconnect_rejoin);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Skip MOTD", "Don't display server MOTD",
	                         &prefs.hex_irc_skip_motd);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Set invisible mode", "Hide from WHO queries",
	                         &prefs.hex_irc_invisible);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	return page;
}

/* Create Logging preferences page */
static GtkWidget *
create_logging_page (void)
{
	GtkWidget *page;
	GtkWidget *group;
	GtkWidget *row;

	page = adw_preferences_page_new ();
	adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Logging");
	adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page), "document-save-symbolic");

	/* Logging group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Chat Logging");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Enable logging", "Save chat to files",
	                         &prefs.hex_irc_logging);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Add timestamps to log", NULL,
	                         &prefs.hex_stamp_log);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_entry_row ("Log directory", NULL,
	                        prefs.hex_irc_logmask, sizeof(prefs.hex_irc_logmask));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	/* URL Grabber group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "URL Grabber");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_switch_row ("Enable URL grabber", "Collect URLs from chat",
	                         &prefs.hex_url_grabber);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Log URLs to file", NULL,
	                         &prefs.hex_url_logging);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_spin_row ("Maximum URLs", "Limit stored URLs (0 = unlimited)",
	                       &prefs.hex_url_grabber_limit, 0, 10000, 10);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	return page;
}

/* Create Away preferences page */
static GtkWidget *
create_away_page (void)
{
	GtkWidget *page;
	GtkWidget *group;
	GtkWidget *row;

	page = adw_preferences_page_new ();
	adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Away");
	adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page), "user-away-symbolic");

	/* Away Settings group */
	group = adw_preferences_group_new ();
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Away Settings");
	adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

	row = create_entry_row ("Away reason", "Message shown when away",
	                        prefs.hex_away_reason, sizeof(prefs.hex_away_reason));
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Auto-unmark away", "Unmark away when you type",
	                         &prefs.hex_away_auto_unmark);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Show away once", "Only show away message once per nick",
	                         &prefs.hex_away_show_once);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Track away status", "Track away status of users",
	                         &prefs.hex_away_track);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	row = create_switch_row ("Omit alerts when away", NULL,
	                         &prefs.hex_away_omit_alerts);
	adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

	return page;
}

/* Save all preferences to config file */
static void
prefs_save_settings (void)
{
	save_config ();
}

/* Close callback - save settings */
static gboolean
prefs_window_close_cb (GtkWindow *window, gpointer user_data)
{
	prefs_save_settings ();
	prefs_window = NULL;
	return FALSE;  /* Allow window to close */
}

/* ===== Public API ===== */

void
prefs_show (GtkWindow *parent)
{
	GtkWidget *page;

	if (prefs_window)
	{
		gtk_window_present (GTK_WINDOW (prefs_window));
		return;
	}

	prefs_window = adw_preferences_window_new ();
	gtk_window_set_title (GTK_WINDOW (prefs_window), "Preferences");
	gtk_window_set_default_size (GTK_WINDOW (prefs_window), 700, 550);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (prefs_window), parent);

	/* Add preference pages */
	page = create_interface_page ();
	adw_preferences_window_add (ADW_PREFERENCES_WINDOW (prefs_window),
	                            ADW_PREFERENCES_PAGE (page));

	page = create_chatting_page ();
	adw_preferences_window_add (ADW_PREFERENCES_WINDOW (prefs_window),
	                            ADW_PREFERENCES_PAGE (page));

	page = create_network_page ();
	adw_preferences_window_add (ADW_PREFERENCES_WINDOW (prefs_window),
	                            ADW_PREFERENCES_PAGE (page));

	page = create_logging_page ();
	adw_preferences_window_add (ADW_PREFERENCES_WINDOW (prefs_window),
	                            ADW_PREFERENCES_PAGE (page));

	page = create_away_page ();
	adw_preferences_window_add (ADW_PREFERENCES_WINDOW (prefs_window),
	                            ADW_PREFERENCES_PAGE (page));

	/* Connect close signal to save settings */
	g_signal_connect (prefs_window, "close-request",
	                  G_CALLBACK (prefs_window_close_cb), NULL);

	gtk_window_present (GTK_WINDOW (prefs_window));
}
