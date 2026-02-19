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

#ifndef HEXCHAT_FE_GTK4_H
#define HEXCHAT_FE_GTK4_H

#include <adwaita.h>

/* Forward declarations */
struct session;
struct server;

#define DISPLAY_NAME "HexChat"

/* GUI data attached to each session */
typedef struct session_gui
{
	AdwTabPage *tab_page;          /* tab in AdwTabView */
	GtkWidget *text_view;          /* GtkTextView for IRC output */
	GtkTextBuffer *text_buffer;    /* text buffer for text_view */
	GtkWidget *input_entry;        /* GtkEntry for user input */
	GtkWidget *userlist_view;      /* GtkListView for user list */
	GListStore *userlist_store;    /* GListStore backing userlist */
	GtkWidget *topic_label;        /* topic display widget */
	GtkWidget *paned;              /* GtkPaned for main layout */
	
	/* Marker line support */
	GtkTextMark *marker_pos;       /* Position of marker line */
	gboolean marker_visible;       /* Whether marker is currently shown */
} session_gui;

/* GUI data attached to each server */
typedef struct server_gui
{
	/* Channel list window */
	GtkWidget *chanlist_window;        /* AdwWindow for channel list */
	GtkWidget *chanlist_view;          /* GtkColumnView for channel list */
	GListStore *chanlist_store;        /* GListStore for channels */
	GtkWidget *chanlist_label;         /* Status label */
	GtkWidget *chanlist_entry;         /* Search entry */
	GtkWidget *chanlist_refresh;       /* Refresh button */
	GtkWidget *chanlist_join;          /* Join button */
	GtkWidget *chanlist_min_spin;      /* Min users spinner */
	GtkWidget *chanlist_max_spin;      /* Max users spinner */
	GtkFilterListModel *chanlist_filter_model;  /* Filtered model */
	GtkCustomFilter *chanlist_filter;  /* Custom filter */

	/* Channel list state */
	guint chanlist_users_found;        /* Total users found */
	guint chanlist_users_shown;        /* Users in shown channels */
	guint chanlist_channels_found;     /* Total channels found */
	guint chanlist_channels_shown;     /* Channels currently shown */
	guint32 chanlist_minusers;         /* Min users filter */
	guint32 chanlist_maxusers;         /* Max users filter */
	gboolean chanlist_match_channel;   /* Match in channel name */
	gboolean chanlist_match_topic;     /* Match in topic */
} server_gui;

/* Restore GUI state */
typedef struct restore_gui
{
	/* Tab/window state for restoration */
	gpointer dummy;
} restore_gui;

/* Global application state */
extern AdwApplication *hexchat_app;
extern GtkWidget *main_window;
extern AdwTabView *tab_view;

/* Initialization */
void fe_gtk4_init_tags (GtkTextBuffer *buffer);

/* Channel list */
void chanlist_opengui (struct server *serv, int do_refresh);

/* Server list */
void servlist_open (struct session *sess);

/* Preferences */
void prefs_show (GtkWindow *parent);

#endif /* HEXCHAT_FE_GTK4_H */
