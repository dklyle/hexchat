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

#include <adwaita.h>

#include "../common/hexchat.h"
#include "../common/hexchatc.h"
#include "../common/outbound.h"
#include "../common/server.h"
#include "../common/util.h"
#include "../common/fe.h"

#include "fe-gtk4.h"

/* ===== Channel Item GObject ===== */

#define CHANNEL_ITEM_TYPE (channel_item_get_type ())
G_DECLARE_FINAL_TYPE (ChannelItem, channel_item, CHANNEL, ITEM, GObject)

struct _ChannelItem
{
	GObject parent_instance;
	char *channel;
	char *topic;
	guint users;
};

G_DEFINE_TYPE (ChannelItem, channel_item, G_TYPE_OBJECT)

static void
channel_item_finalize (GObject *object)
{
	ChannelItem *self = CHANNEL_ITEM (object);
	g_free (self->channel);
	g_free (self->topic);
	G_OBJECT_CLASS (channel_item_parent_class)->finalize (object);
}

static void
channel_item_class_init (ChannelItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = channel_item_finalize;
}

static void
channel_item_init (ChannelItem *self)
{
	self->channel = NULL;
	self->topic = NULL;
	self->users = 0;
}

static ChannelItem *
channel_item_new (const char *channel, guint users, const char *topic)
{
	ChannelItem *item = g_object_new (CHANNEL_ITEM_TYPE, NULL);
	item->channel = g_strdup (channel);
	item->users = users;
	/* Strip colors from topic */
	item->topic = strip_color (topic, -1, STRIP_ALL);
	return item;
}

/* ===== Channel List Filter ===== */

static gboolean
chanlist_filter_func (GObject *item, gpointer user_data)
{
	server *serv = user_data;
	ChannelItem *chan_item = CHANNEL_ITEM (item);
	const char *search_text;

	if (!serv || !serv->gui)
		return TRUE;

	/* Filter by user count */
	if (chan_item->users < serv->gui->chanlist_minusers)
		return FALSE;
	if (chan_item->users > serv->gui->chanlist_maxusers)
		return FALSE;

	/* Filter by search text */
	search_text = gtk_editable_get_text (GTK_EDITABLE (serv->gui->chanlist_entry));
	if (search_text && search_text[0])
	{
		gboolean match = FALSE;

		if (serv->gui->chanlist_match_channel)
		{
			if (nocasestrstr (chan_item->channel, search_text))
				match = TRUE;
		}
		if (serv->gui->chanlist_match_topic && chan_item->topic)
		{
			if (nocasestrstr (chan_item->topic, search_text))
				match = TRUE;
		}
		if (!match)
			return FALSE;
	}

	return TRUE;
}

/* ===== Update Status Label ===== */

static void
chanlist_update_label (server *serv)
{
	char buf[256];
	guint shown_channels = 0;
	guint shown_users = 0;

	if (!serv->gui->chanlist_label)
		return;

	/* Count shown items */
	if (serv->gui->chanlist_filter_model)
	{
		guint n = g_list_model_get_n_items (G_LIST_MODEL (serv->gui->chanlist_filter_model));
		guint i;
		for (i = 0; i < n; i++)
		{
			ChannelItem *item = g_list_model_get_item (G_LIST_MODEL (serv->gui->chanlist_filter_model), i);
			if (item)
			{
				shown_channels++;
				shown_users += item->users;
				g_object_unref (item);
			}
		}
	}

	serv->gui->chanlist_channels_shown = shown_channels;
	serv->gui->chanlist_users_shown = shown_users;

	g_snprintf (buf, sizeof (buf),
	            _("Displaying %u/%u users on %u/%u channels."),
	            serv->gui->chanlist_users_shown,
	            serv->gui->chanlist_users_found,
	            serv->gui->chanlist_channels_shown,
	            serv->gui->chanlist_channels_found);

	gtk_label_set_text (GTK_LABEL (serv->gui->chanlist_label), buf);
}

/* ===== Callbacks ===== */

static void
chanlist_refresh_cb (GtkButton *button, gpointer user_data)
{
	server *serv = user_data;
	char tbuf[128];

	if (!serv->connected)
	{
		/* Not connected */
		return;
	}

	/* Clear existing data */
	if (serv->gui->chanlist_store)
		g_list_store_remove_all (serv->gui->chanlist_store);

	serv->gui->chanlist_channels_found = 0;
	serv->gui->chanlist_users_found = 0;

	/* Request channel list from server */
	g_snprintf (tbuf, sizeof (tbuf), "LIST >%d,<%d",
	            serv->gui->chanlist_minusers - 1,
	            serv->gui->chanlist_maxusers + 1);
	handle_command (serv->server_session, tbuf, FALSE);

	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
}

static void
chanlist_join_cb (GtkButton *button, gpointer user_data)
{
	server *serv = user_data;
	GtkSelectionModel *selection;
	GtkBitset *selected;
	guint pos;
	ChannelItem *item;
	char tbuf[CHANLEN + 6];

	if (!serv->gui->chanlist_view || !serv->connected)
		return;

	selection = gtk_column_view_get_model (GTK_COLUMN_VIEW (serv->gui->chanlist_view));
	selected = gtk_selection_model_get_selection (selection);

	if (gtk_bitset_is_empty (selected))
	{
		gtk_bitset_unref (selected);
		return;
	}

	pos = gtk_bitset_get_nth (selected, 0);
	gtk_bitset_unref (selected);

	item = g_list_model_get_item (G_LIST_MODEL (serv->gui->chanlist_filter_model), pos);
	if (item)
	{
		g_snprintf (tbuf, sizeof (tbuf), "join %s", item->channel);
		handle_command (serv->server_session, tbuf, FALSE);
		g_object_unref (item);
	}
}

static void
chanlist_row_activated_cb (GtkColumnView *view,
                           guint position,
                           gpointer user_data)
{
	chanlist_join_cb (NULL, user_data);
}

static void
chanlist_search_changed_cb (GtkEditable *editable, gpointer user_data)
{
	server *serv = user_data;

	if (serv->gui->chanlist_filter)
		gtk_filter_changed (GTK_FILTER (serv->gui->chanlist_filter),
		                    GTK_FILTER_CHANGE_DIFFERENT);

	chanlist_update_label (serv);
}

static void
chanlist_minusers_changed_cb (GtkSpinButton *spin, gpointer user_data)
{
	server *serv = user_data;

	serv->gui->chanlist_minusers = gtk_spin_button_get_value_as_int (spin);
	prefs.hex_gui_chanlist_minusers = serv->gui->chanlist_minusers;

	if (serv->gui->chanlist_filter)
		gtk_filter_changed (GTK_FILTER (serv->gui->chanlist_filter),
		                    GTK_FILTER_CHANGE_DIFFERENT);

	chanlist_update_label (serv);
}

static void
chanlist_maxusers_changed_cb (GtkSpinButton *spin, gpointer user_data)
{
	server *serv = user_data;

	serv->gui->chanlist_maxusers = gtk_spin_button_get_value_as_int (spin);
	prefs.hex_gui_chanlist_maxusers = serv->gui->chanlist_maxusers;

	if (serv->gui->chanlist_filter)
		gtk_filter_changed (GTK_FILTER (serv->gui->chanlist_filter),
		                    GTK_FILTER_CHANGE_DIFFERENT);

	chanlist_update_label (serv);
}

static void
chanlist_match_channel_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	server *serv = user_data;

	serv->gui->chanlist_match_channel = gtk_check_button_get_active (check);

	if (serv->gui->chanlist_filter)
		gtk_filter_changed (GTK_FILTER (serv->gui->chanlist_filter),
		                    GTK_FILTER_CHANGE_DIFFERENT);

	chanlist_update_label (serv);
}

static void
chanlist_match_topic_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	server *serv = user_data;

	serv->gui->chanlist_match_topic = gtk_check_button_get_active (check);

	if (serv->gui->chanlist_filter)
		gtk_filter_changed (GTK_FILTER (serv->gui->chanlist_filter),
		                    GTK_FILTER_CHANGE_DIFFERENT);

	chanlist_update_label (serv);
}

static void
chanlist_window_closed_cb (GtkWindow *window, gpointer user_data)
{
	server *serv = user_data;

	if (serv->gui)
	{
		serv->gui->chanlist_window = NULL;
		serv->gui->chanlist_view = NULL;
		serv->gui->chanlist_store = NULL;
		serv->gui->chanlist_label = NULL;
		serv->gui->chanlist_entry = NULL;
		serv->gui->chanlist_refresh = NULL;
		serv->gui->chanlist_join = NULL;
		serv->gui->chanlist_filter_model = NULL;
		serv->gui->chanlist_filter = NULL;
	}
}

/* ===== Column View Factory Callbacks ===== */

static void
channel_setup_cb (GtkSignalListItemFactory *factory,
                  GtkListItem *list_item,
                  gpointer user_data)
{
	GtkWidget *label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_widget_set_margin_start (label, 6);
	gtk_widget_set_margin_end (label, 6);
	gtk_list_item_set_child (list_item, label);
}

static void
channel_bind_cb (GtkSignalListItemFactory *factory,
                 GtkListItem *list_item,
                 gpointer user_data)
{
	GtkWidget *label = gtk_list_item_get_child (list_item);
	ChannelItem *item = gtk_list_item_get_item (list_item);

	if (item)
		gtk_label_set_text (GTK_LABEL (label), item->channel);
}

static void
users_setup_cb (GtkSignalListItemFactory *factory,
                GtkListItem *list_item,
                gpointer user_data)
{
	GtkWidget *label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (label), 1.0);
	gtk_widget_set_margin_start (label, 6);
	gtk_widget_set_margin_end (label, 6);
	gtk_list_item_set_child (list_item, label);
}

static void
users_bind_cb (GtkSignalListItemFactory *factory,
               GtkListItem *list_item,
               gpointer user_data)
{
	GtkWidget *label = gtk_list_item_get_child (list_item);
	ChannelItem *item = gtk_list_item_get_item (list_item);
	char buf[32];

	if (item)
	{
		g_snprintf (buf, sizeof (buf), "%u", item->users);
		gtk_label_set_text (GTK_LABEL (label), buf);
	}
}

static void
topic_setup_cb (GtkSignalListItemFactory *factory,
                GtkListItem *list_item,
                gpointer user_data)
{
	GtkWidget *label = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_margin_start (label, 6);
	gtk_widget_set_margin_end (label, 6);
	gtk_list_item_set_child (list_item, label);
}

static void
topic_bind_cb (GtkSignalListItemFactory *factory,
               GtkListItem *list_item,
               gpointer user_data)
{
	GtkWidget *label = gtk_list_item_get_child (list_item);
	ChannelItem *item = gtk_list_item_get_item (list_item);

	if (item && item->topic)
		gtk_label_set_text (GTK_LABEL (label), item->topic);
	else
		gtk_label_set_text (GTK_LABEL (label), "");
}

/* ===== Public Functions ===== */

void
chanlist_opengui (server *serv, int do_refresh)
{
	GtkWidget *window, *vbox, *hbox, *scroll, *grid;
	GtkWidget *label, *entry, *button, *spin, *check;
	GtkColumnViewColumn *column;
	GtkListItemFactory *factory;
	GtkSingleSelection *selection;
	char tbuf[256];

	if (!serv->gui)
		return;

	/* If window already exists, bring it to front */
	if (serv->gui->chanlist_window)
	{
		gtk_window_present (GTK_WINDOW (serv->gui->chanlist_window));
		return;
	}

	/* Initialize defaults */
	if (!serv->gui->chanlist_minusers)
	{
		serv->gui->chanlist_minusers = prefs.hex_gui_chanlist_minusers;
		if (serv->gui->chanlist_minusers < 1 || serv->gui->chanlist_minusers > 999999)
			serv->gui->chanlist_minusers = 5;
	}
	if (!serv->gui->chanlist_maxusers)
	{
		serv->gui->chanlist_maxusers = prefs.hex_gui_chanlist_maxusers;
		if (serv->gui->chanlist_maxusers < 1 || serv->gui->chanlist_maxusers > 999999)
			serv->gui->chanlist_maxusers = 9999;
	}
	serv->gui->chanlist_match_channel = TRUE;
	serv->gui->chanlist_match_topic = TRUE;

	/* Create window */
	g_snprintf (tbuf, sizeof (tbuf), _("Channel List (%s) - %s"),
	            server_get_network (serv, TRUE), _(DISPLAY_NAME));

	window = adw_window_new ();
	gtk_window_set_title (GTK_WINDOW (window), tbuf);
	gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);
	gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (main_window));
	serv->gui->chanlist_window = window;

	g_signal_connect (window, "close-request",
	                  G_CALLBACK (chanlist_window_closed_cb), serv);

	/* Main content */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* Header bar */
	GtkWidget *header = adw_header_bar_new ();
	gtk_box_append (GTK_BOX (vbox), header);

	/* Content box with margins */
	GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (content, 12);
	gtk_widget_set_margin_end (content, 12);
	gtk_widget_set_margin_top (content, 12);
	gtk_widget_set_margin_bottom (content, 12);
	gtk_widget_set_vexpand (content, TRUE);
	gtk_box_append (GTK_BOX (vbox), content);

	/* Status label */
	label = gtk_label_new (_("Ready to download channel list."));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_box_append (GTK_BOX (content), label);
	serv->gui->chanlist_label = label;

	/* Create data store */
	serv->gui->chanlist_store = g_list_store_new (CHANNEL_ITEM_TYPE);

	/* Create filter */
	serv->gui->chanlist_filter = gtk_custom_filter_new (
		(GtkCustomFilterFunc)chanlist_filter_func, serv, NULL);

	/* Create filter model */
	serv->gui->chanlist_filter_model = gtk_filter_list_model_new (
		G_LIST_MODEL (serv->gui->chanlist_store),
		GTK_FILTER (serv->gui->chanlist_filter));

	/* Create selection model */
	selection = gtk_single_selection_new (G_LIST_MODEL (serv->gui->chanlist_filter_model));

	/* Create column view */
	serv->gui->chanlist_view = gtk_column_view_new (GTK_SELECTION_MODEL (selection));
	gtk_column_view_set_show_column_separators (GTK_COLUMN_VIEW (serv->gui->chanlist_view), TRUE);
	gtk_column_view_set_show_row_separators (GTK_COLUMN_VIEW (serv->gui->chanlist_view), TRUE);

	g_signal_connect (serv->gui->chanlist_view, "activate",
	                  G_CALLBACK (chanlist_row_activated_cb), serv);

	/* Channel column */
	factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (channel_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (channel_bind_cb), NULL);
	column = gtk_column_view_column_new (_("Channel"), factory);
	gtk_column_view_column_set_resizable (column, TRUE);
	gtk_column_view_column_set_fixed_width (column, 150);
	gtk_column_view_append_column (GTK_COLUMN_VIEW (serv->gui->chanlist_view), column);
	g_object_unref (column);

	/* Users column */
	factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (users_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (users_bind_cb), NULL);
	column = gtk_column_view_column_new (_("Users"), factory);
	gtk_column_view_column_set_resizable (column, TRUE);
	gtk_column_view_column_set_fixed_width (column, 80);
	gtk_column_view_append_column (GTK_COLUMN_VIEW (serv->gui->chanlist_view), column);
	g_object_unref (column);

	/* Topic column */
	factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (topic_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (topic_bind_cb), NULL);
	column = gtk_column_view_column_new (_("Topic"), factory);
	gtk_column_view_column_set_expand (column, TRUE);
	gtk_column_view_append_column (GTK_COLUMN_VIEW (serv->gui->chanlist_view), column);
	g_object_unref (column);

	/* Scrolled window for column view */
	scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (scroll, TRUE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), serv->gui->chanlist_view);
	gtk_box_append (GTK_BOX (content), scroll);

	/* Controls grid */
	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_box_append (GTK_BOX (content), grid);

	/* Row 0: Find entry */
	label = gtk_label_new (_("Find:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search channels..."));
	gtk_widget_set_hexpand (entry, TRUE);
	g_signal_connect (entry, "changed", G_CALLBACK (chanlist_search_changed_cb), serv);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 2, 1);
	serv->gui->chanlist_entry = entry;

	/* Row 1: Look in checkboxes */
	label = gtk_label_new (_("Look in:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_grid_attach (GTK_GRID (grid), hbox, 1, 1, 2, 1);

	check = gtk_check_button_new_with_label (_("Channel name"));
	gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
	g_signal_connect (check, "toggled", G_CALLBACK (chanlist_match_channel_toggled_cb), serv);
	gtk_box_append (GTK_BOX (hbox), check);

	check = gtk_check_button_new_with_label (_("Topic"));
	gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
	g_signal_connect (check, "toggled", G_CALLBACK (chanlist_match_topic_toggled_cb), serv);
	gtk_box_append (GTK_BOX (hbox), check);

	/* Row 2: User count filters */
	label = gtk_label_new (_("Show only:"));
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_grid_attach (GTK_GRID (grid), hbox, 1, 2, 2, 1);

	label = gtk_label_new (_("channels with"));
	gtk_box_append (GTK_BOX (hbox), label);

	spin = gtk_spin_button_new_with_range (1, 999999, 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), serv->gui->chanlist_minusers);
	g_signal_connect (spin, "value-changed", G_CALLBACK (chanlist_minusers_changed_cb), serv);
	gtk_box_append (GTK_BOX (hbox), spin);
	serv->gui->chanlist_min_spin = spin;

	label = gtk_label_new (_("to"));
	gtk_box_append (GTK_BOX (hbox), label);

	spin = gtk_spin_button_new_with_range (1, 999999, 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), serv->gui->chanlist_maxusers);
	g_signal_connect (spin, "value-changed", G_CALLBACK (chanlist_maxusers_changed_cb), serv);
	gtk_box_append (GTK_BOX (hbox), spin);
	serv->gui->chanlist_max_spin = spin;

	label = gtk_label_new (_("users."));
	gtk_box_append (GTK_BOX (hbox), label);

	/* Row 3: Buttons */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (hbox, GTK_ALIGN_END);
	gtk_grid_attach (GTK_GRID (grid), hbox, 0, 3, 3, 1);

	button = gtk_button_new_with_label (_("Download List"));
	g_signal_connect (button, "clicked", G_CALLBACK (chanlist_refresh_cb), serv);
	gtk_box_append (GTK_BOX (hbox), button);
	serv->gui->chanlist_refresh = button;

	button = gtk_button_new_with_label (_("Join Channel"));
	g_signal_connect (button, "clicked", G_CALLBACK (chanlist_join_cb), serv);
	gtk_box_append (GTK_BOX (hbox), button);
	serv->gui->chanlist_join = button;

	adw_window_set_content (ADW_WINDOW (window), vbox);
	gtk_window_present (GTK_WINDOW (window));

	if (do_refresh)
		chanlist_refresh_cb (GTK_BUTTON (serv->gui->chanlist_refresh), serv);
}

/* Called by fe_add_chan_list to add a channel */
void
fe_add_chan_list (server *serv, char *chan, char *users, char *topic)
{
	ChannelItem *item;
	guint user_count;

	if (!serv->gui || !serv->gui->chanlist_store)
		return;

	user_count = atoi (users);

	item = channel_item_new (chan, user_count, topic);
	g_list_store_append (serv->gui->chanlist_store, item);
	g_object_unref (item);

	serv->gui->chanlist_channels_found++;
	serv->gui->chanlist_users_found += user_count;

	/* Update label periodically */
	if ((serv->gui->chanlist_channels_found % 100) == 0)
		chanlist_update_label (serv);
}

/* Called when channel list download is complete */
void
fe_chan_list_end (server *serv)
{
	if (!serv->gui)
		return;

	if (serv->gui->chanlist_refresh)
		gtk_widget_set_sensitive (serv->gui->chanlist_refresh, TRUE);

	chanlist_update_label (serv);
}

/* Check if channel list window exists */
int
fe_is_chanwindow (server *serv)
{
	if (!serv->gui)
		return 0;

	return serv->gui->chanlist_window != NULL;
}
