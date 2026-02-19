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

/* Server List GUI for GTK4/libadwaita */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <adwaita.h>

#include "../common/hexchat.h"
#include "../common/hexchatc.h"
#include "../common/servlist.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/util.h"

#include "fe-gtk4.h"

#ifdef USE_OPENSSL
# define DEFAULT_SERVER "newserver/6697"
#else
# define DEFAULT_SERVER "newserver/6667"
#endif

/* ===== NetworkItem GObject for GtkListView ===== */

#define NETWORK_ITEM_TYPE (network_item_get_type())
G_DECLARE_FINAL_TYPE (NetworkItem, network_item, NETWORK, ITEM, GObject)

struct _NetworkItem
{
	GObject parent_instance;
	ircnet *net;
	char *name;
	gboolean favorite;
	gboolean auto_connect;
};

G_DEFINE_TYPE (NetworkItem, network_item, G_TYPE_OBJECT)

static void
network_item_finalize (GObject *object)
{
	NetworkItem *self = NETWORK_ITEM (object);
	g_free (self->name);
	G_OBJECT_CLASS (network_item_parent_class)->finalize (object);
}

static void
network_item_class_init (NetworkItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = network_item_finalize;
}

static void
network_item_init (NetworkItem *self)
{
	self->net = NULL;
	self->name = NULL;
	self->favorite = FALSE;
	self->auto_connect = FALSE;
}

static NetworkItem *
network_item_new (ircnet *net)
{
	NetworkItem *item = g_object_new (NETWORK_ITEM_TYPE, NULL);
	item->net = net;
	item->name = g_strdup (net->name);
	item->favorite = (net->flags & FLAG_FAVORITE) != 0;
	item->auto_connect = (net->flags & FLAG_AUTO_CONNECT) != 0;
	return item;
}

/* ===== Server List Window State ===== */

static GtkWidget *serverlist_win = NULL;
static GtkWidget *networks_list = NULL;
static GListStore *networks_store = NULL;
static GtkSingleSelection *network_selection = NULL;
static ircnet *selected_net = NULL;
static session *servlist_sess = NULL;

/* Global user entries */
static GtkWidget *entry_nick1 = NULL;
static GtkWidget *entry_nick2 = NULL;
static GtkWidget *entry_nick3 = NULL;
static GtkWidget *entry_username = NULL;

/* Network edit dialog */
static GtkWidget *edit_dialog = NULL;

/* ===== Forward Declarations ===== */

static void servlist_connect_cb (GtkWidget *button, gpointer user_data);
static void servlist_edit_cb (GtkWidget *button, gpointer user_data);
static void servlist_add_cb (GtkWidget *button, gpointer user_data);
static void servlist_remove_cb (GtkWidget *button, gpointer user_data);
static void servlist_network_selected (GtkSingleSelection *sel, GParamSpec *pspec, gpointer user_data);
static void servlist_populate (void);
static void servlist_savegui (void);

/* ===== Utility Functions ===== */

static ircnet *
servlist_get_selected_net (void)
{
	guint pos;
	NetworkItem *item;

	if (!network_selection)
		return NULL;

	pos = gtk_single_selection_get_selected (network_selection);
	if (pos == GTK_INVALID_LIST_POSITION)
		return NULL;

	item = g_list_model_get_item (G_LIST_MODEL (networks_store), pos);
	if (!item)
		return NULL;

	g_object_unref (item);
	return item->net;
}

/* ===== Network List Setup ===== */

static void
network_setup_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *box, *label, *star;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);
	gtk_widget_set_margin_top (box, 4);
	gtk_widget_set_margin_bottom (box, 4);

	star = gtk_image_new_from_icon_name ("starred-symbolic");
	gtk_widget_set_visible (star, FALSE);
	gtk_box_append (GTK_BOX (box), star);

	label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_box_append (GTK_BOX (box), label);

	gtk_list_item_set_child (list_item, box);
}

static void
network_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *box, *label, *star;
	NetworkItem *item;

	item = gtk_list_item_get_item (list_item);
	if (!item)
		return;

	box = gtk_list_item_get_child (list_item);
	star = gtk_widget_get_first_child (box);
	label = gtk_widget_get_next_sibling (star);

	gtk_label_set_text (GTK_LABEL (label), item->name);
	gtk_widget_set_visible (star, item->favorite);

	if (item->auto_connect)
	{
		PangoAttrList *attrs = pango_attr_list_new ();
		pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
		gtk_label_set_attributes (GTK_LABEL (label), attrs);
		pango_attr_list_unref (attrs);
	}
	else
	{
		gtk_label_set_attributes (GTK_LABEL (label), NULL);
	}
}

/* ===== GUI Save/Load ===== */

static void
servlist_savegui (void)
{
	const char *text;

	/* Save global user settings */
	if (entry_nick1)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (entry_nick1));
		if (text[0])
			g_strlcpy (prefs.hex_irc_nick1, text, sizeof (prefs.hex_irc_nick1));
	}
	if (entry_nick2)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (entry_nick2));
		if (text[0])
			g_strlcpy (prefs.hex_irc_nick2, text, sizeof (prefs.hex_irc_nick2));
	}
	if (entry_nick3)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (entry_nick3));
		if (text[0])
			g_strlcpy (prefs.hex_irc_nick3, text, sizeof (prefs.hex_irc_nick3));
	}
	if (entry_username)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (entry_username));
		if (text[0])
			g_strlcpy (prefs.hex_irc_user_name, text, sizeof (prefs.hex_irc_user_name));
	}

	/* Save network list and prefs */
	servlist_save ();
}

/* ===== Populate Network List ===== */

static void
servlist_populate (void)
{
	GSList *list;
	ircnet *net;
	guint i;

	if (!networks_store)
		return;

	/* Clear existing */
	g_list_store_remove_all (networks_store);

	/* If no networks exist, create a default one */
	if (!network_list)
	{
		net = servlist_net_add ("New Network", "", FALSE);
		servlist_server_add (net, DEFAULT_SERVER);
	}

	/* Populate list */
	list = network_list;
	i = 0;
	while (list)
	{
		net = list->data;
		g_list_store_append (networks_store, network_item_new (net));

		if ((int)i == prefs.hex_gui_slist_select)
			selected_net = net;

		i++;
		list = list->next;
	}

	/* Select the saved network */
	if (selected_net && network_selection)
	{
		guint n;
		n = g_list_model_get_n_items (G_LIST_MODEL (networks_store));
		for (i = 0; i < n; i++)
		{
			NetworkItem *item = g_list_model_get_item (G_LIST_MODEL (networks_store), i);
			if (item && item->net == selected_net)
			{
				gtk_single_selection_set_selected (network_selection, i);
				g_object_unref (item);
				break;
			}
			if (item)
				g_object_unref (item);
		}
	}
}

/* ===== Connect to Network ===== */

static void
servlist_connect_cb (GtkWidget *button, gpointer user_data)
{
	ircnet *net;
	guint pos;

	net = servlist_get_selected_net ();
	if (!net)
		return;

	/* Save GUI settings before connecting */
	servlist_savegui ();

	/* Remember selected network */
	pos = gtk_single_selection_get_selected (network_selection);
	if (pos != GTK_INVALID_LIST_POSITION)
		prefs.hex_gui_slist_select = (int)pos;

	/* Close the dialog */
	if (serverlist_win)
	{
		gtk_window_destroy (GTK_WINDOW (serverlist_win));
		serverlist_win = NULL;
	}

	/* Connect to the network */
	servlist_connect (servlist_sess, net, TRUE);
}

/* ===== Network Selection Changed ===== */

static void
servlist_network_selected (GtkSingleSelection *sel, GParamSpec *pspec, gpointer user_data)
{
	selected_net = servlist_get_selected_net ();
}

/* ===== Network Row Activated (double-click) ===== */

static void
servlist_row_activated_cb (GtkListView *list_view, guint position, gpointer user_data)
{
	selected_net = servlist_get_selected_net ();
	if (selected_net)
		servlist_connect_cb (NULL, NULL);
}

/* ===== Add Network ===== */

static void
servlist_add_cb (GtkWidget *button, gpointer user_data)
{
	ircnet *net;

	net = servlist_net_add ("New Network", "", FALSE);
	if (net)
	{
		servlist_server_add (net, DEFAULT_SERVER);
		servlist_populate ();

		/* Select the new network (it's at the end) */
		if (network_selection && networks_store)
		{
			guint n = g_list_model_get_n_items (G_LIST_MODEL (networks_store));
			if (n > 0)
				gtk_single_selection_set_selected (network_selection, n - 1);
		}
	}
}

/* ===== Remove Network ===== */

static void
servlist_remove_response_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	AdwAlertDialog *dialog = ADW_ALERT_DIALOG (source);
	ircnet *net = user_data;
	const char *response;

	response = adw_alert_dialog_choose_finish (dialog, result);
	if (g_strcmp0 (response, "delete") == 0 && net)
	{
		servlist_net_remove (net);
		selected_net = NULL;
		servlist_populate ();
	}
}

static void
servlist_remove_cb (GtkWidget *button, gpointer user_data)
{
	ircnet *net;
	AdwDialog *dialog;
	char *msg;

	net = servlist_get_selected_net ();
	if (!net)
		return;

	msg = g_strdup_printf ("Delete network \"%s\"?", net->name);

	dialog = adw_alert_dialog_new ("Delete Network", msg);
	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
	                                "cancel", "Cancel",
	                                "delete", "Delete",
	                                NULL);
	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
	                                          "delete", ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
	adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

	adw_alert_dialog_choose (ADW_ALERT_DIALOG (dialog),
	                         GTK_WIDGET (serverlist_win),
	                         NULL,
	                         servlist_remove_response_cb,
	                         net);

	g_free (msg);
}

/* ===== Edit Network Dialog ===== */

static GtkWidget *edit_entry_nick = NULL;
static GtkWidget *edit_entry_nick2 = NULL;
static GtkWidget *edit_entry_user = NULL;
static GtkWidget *edit_entry_real = NULL;
static GtkWidget *edit_entry_pass = NULL;
static GtkWidget *edit_check_global = NULL;
static GtkWidget *edit_check_autoconnect = NULL;
static GtkWidget *edit_check_ssl = NULL;
static GtkWidget *edit_check_favorite = NULL;
static GtkWidget *edit_servers_list = NULL;
static GListStore *edit_servers_store = NULL;

/* ServerItem for server list in edit dialog */
#define SERVER_ITEM_TYPE (server_item_get_type())
G_DECLARE_FINAL_TYPE (ServerItem, server_item, SERVER, ITEM, GObject)

struct _ServerItem
{
	GObject parent_instance;
	ircserver *serv;
	char *hostname;
};

G_DEFINE_TYPE (ServerItem, server_item, G_TYPE_OBJECT)

static void
server_item_finalize (GObject *object)
{
	ServerItem *self = SERVER_ITEM (object);
	g_free (self->hostname);
	G_OBJECT_CLASS (server_item_parent_class)->finalize (object);
}

static void
server_item_class_init (ServerItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = server_item_finalize;
}

static void
server_item_init (ServerItem *self)
{
	self->serv = NULL;
	self->hostname = NULL;
}

static ServerItem *
server_item_new (ircserver *serv)
{
	ServerItem *item = g_object_new (SERVER_ITEM_TYPE, NULL);
	item->serv = serv;
	item->hostname = g_strdup (serv->hostname);
	return item;
}

static void
edit_toggle_global_user (gboolean use_global)
{
	gboolean sensitive = !use_global;

	if (edit_entry_nick)
		gtk_widget_set_sensitive (edit_entry_nick, sensitive);
	if (edit_entry_nick2)
		gtk_widget_set_sensitive (edit_entry_nick2, sensitive);
	if (edit_entry_user)
		gtk_widget_set_sensitive (edit_entry_user, sensitive);
	if (edit_entry_real)
		gtk_widget_set_sensitive (edit_entry_real, sensitive);
}

static void
edit_global_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	gboolean active = gtk_check_button_get_active (check);
	edit_toggle_global_user (active);

	if (selected_net)
	{
		if (active)
			selected_net->flags |= FLAG_USE_GLOBAL;
		else
			selected_net->flags &= ~FLAG_USE_GLOBAL;
	}
}

static void
edit_autoconnect_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	if (selected_net)
	{
		if (gtk_check_button_get_active (check))
			selected_net->flags |= FLAG_AUTO_CONNECT;
		else
			selected_net->flags &= ~FLAG_AUTO_CONNECT;
	}
}

static void
edit_ssl_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	if (selected_net)
	{
		if (gtk_check_button_get_active (check))
			selected_net->flags |= FLAG_USE_SSL;
		else
			selected_net->flags &= ~FLAG_USE_SSL;
	}
}

static void
edit_favorite_toggled_cb (GtkCheckButton *check, gpointer user_data)
{
	if (selected_net)
	{
		if (gtk_check_button_get_active (check))
			selected_net->flags |= FLAG_FAVORITE;
		else
			selected_net->flags &= ~FLAG_FAVORITE;
	}
}

static void
edit_save_entries (void)
{
	const char *text;

	if (!selected_net)
		return;

	/* Only save if not using global */
	if (!(selected_net->flags & FLAG_USE_GLOBAL))
	{
		if (edit_entry_nick)
		{
			text = gtk_editable_get_text (GTK_EDITABLE (edit_entry_nick));
			g_free (selected_net->nick);
			selected_net->nick = text[0] ? g_strdup (text) : NULL;
		}
		if (edit_entry_nick2)
		{
			text = gtk_editable_get_text (GTK_EDITABLE (edit_entry_nick2));
			g_free (selected_net->nick2);
			selected_net->nick2 = text[0] ? g_strdup (text) : NULL;
		}
		if (edit_entry_user)
		{
			text = gtk_editable_get_text (GTK_EDITABLE (edit_entry_user));
			g_free (selected_net->user);
			selected_net->user = text[0] ? g_strdup (text) : NULL;
		}
		if (edit_entry_real)
		{
			text = gtk_editable_get_text (GTK_EDITABLE (edit_entry_real));
			g_free (selected_net->real);
			selected_net->real = text[0] ? g_strdup (text) : NULL;
		}
	}

	if (edit_entry_pass)
	{
		text = gtk_editable_get_text (GTK_EDITABLE (edit_entry_pass));
		g_free (selected_net->pass);
		selected_net->pass = text[0] ? g_strdup (text) : NULL;
	}
}

static void
edit_close_cb (GtkWidget *button, gpointer user_data)
{
	edit_save_entries ();

	if (edit_dialog)
	{
		gtk_window_destroy (GTK_WINDOW (edit_dialog));
		edit_dialog = NULL;
	}

	/* Update the network list in case flags changed */
	servlist_populate ();
}

static void
server_setup_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *label;

	label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_widget_set_margin_start (label, 6);
	gtk_widget_set_margin_end (label, 6);
	gtk_widget_set_margin_top (label, 4);
	gtk_widget_set_margin_bottom (label, 4);
	gtk_list_item_set_child (list_item, label);
}

static void
server_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *label;
	ServerItem *item;

	item = gtk_list_item_get_item (list_item);
	if (!item)
		return;

	label = gtk_list_item_get_child (list_item);
	gtk_label_set_text (GTK_LABEL (label), item->hostname);
}

static void
edit_populate_servers (ircnet *net)
{
	GSList *list;
	ircserver *serv;

	if (!edit_servers_store)
		return;

	g_list_store_remove_all (edit_servers_store);

	list = net->servlist;
	while (list)
	{
		serv = list->data;
		g_list_store_append (edit_servers_store, server_item_new (serv));
		list = list->next;
	}
}

static void
edit_add_server_cb (GtkWidget *button, gpointer user_data)
{
	ircserver *serv;

	if (!selected_net)
		return;

	serv = servlist_server_add (selected_net, DEFAULT_SERVER);
	if (serv)
		edit_populate_servers (selected_net);
}

static void
edit_remove_server_cb (GtkWidget *button, gpointer user_data)
{
	GtkSingleSelection *sel = user_data;
	guint pos;
	ServerItem *item;

	if (!selected_net || !sel)
		return;

	pos = gtk_single_selection_get_selected (sel);
	if (pos == GTK_INVALID_LIST_POSITION)
		return;

	item = g_list_model_get_item (G_LIST_MODEL (edit_servers_store), pos);
	if (item && item->serv)
	{
		servlist_server_remove (selected_net, item->serv);
		edit_populate_servers (selected_net);
	}
	if (item)
		g_object_unref (item);
}

static GtkWidget *
servlist_create_edit_dialog (ircnet *net)
{
	GtkWidget *dialog;
	GtkWidget *content;
	GtkWidget *header;
	GtkWidget *close_btn;
	GtkWidget *notebook;
	GtkWidget *page_servers, *page_options;
	GtkWidget *box, *grid, *scrolled;
	GtkWidget *label;
	GtkWidget *list_view;
	GtkWidget *btn_box, *add_btn, *remove_btn;
	GtkListItemFactory *factory;
	GtkSingleSelection *sel;
	char *title;
	int row;

	title = g_strdup_printf ("Edit: %s", net->name);

	dialog = adw_window_new ();
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 450);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	if (serverlist_win)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (serverlist_win));

	g_free (title);

	/* Main box */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	adw_window_set_content (ADW_WINDOW (dialog), content);

	/* Header bar */
	header = adw_header_bar_new ();
	gtk_box_append (GTK_BOX (content), header);

	close_btn = gtk_button_new_with_label ("Close");
	gtk_widget_add_css_class (close_btn, "suggested-action");
	adw_header_bar_pack_end (ADW_HEADER_BAR (header), close_btn);
	g_signal_connect (close_btn, "clicked", G_CALLBACK (edit_close_cb), NULL);

	/* Notebook for tabs */
	notebook = gtk_notebook_new ();
	gtk_widget_set_vexpand (notebook, TRUE);
	gtk_widget_set_margin_start (notebook, 12);
	gtk_widget_set_margin_end (notebook, 12);
	gtk_widget_set_margin_top (notebook, 12);
	gtk_widget_set_margin_bottom (notebook, 12);
	gtk_box_append (GTK_BOX (content), notebook);

	/* ===== Servers Tab ===== */
	page_servers = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (page_servers, 6);
	gtk_widget_set_margin_end (page_servers, 6);
	gtk_widget_set_margin_top (page_servers, 6);
	gtk_widget_set_margin_bottom (page_servers, 6);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page_servers, gtk_label_new ("Servers"));

	/* Server list */
	scrolled = gtk_scrolled_window_new ();
	gtk_widget_set_vexpand (scrolled, TRUE);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled), 150);
	gtk_box_append (GTK_BOX (page_servers), scrolled);

	edit_servers_store = g_list_store_new (SERVER_ITEM_TYPE);
	sel = gtk_single_selection_new (G_LIST_MODEL (edit_servers_store));

	factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (server_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (server_bind_cb), NULL);

	list_view = gtk_list_view_new (GTK_SELECTION_MODEL (sel), GTK_LIST_ITEM_FACTORY (factory));
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), list_view);
	edit_servers_list = list_view;

	/* Server buttons */
	btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append (GTK_BOX (page_servers), btn_box);

	add_btn = gtk_button_new_with_label ("Add");
	g_signal_connect (add_btn, "clicked", G_CALLBACK (edit_add_server_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), add_btn);

	remove_btn = gtk_button_new_with_label ("Remove");
	g_signal_connect (remove_btn, "clicked", G_CALLBACK (edit_remove_server_cb), sel);
	gtk_box_append (GTK_BOX (btn_box), remove_btn);

	edit_populate_servers (net);

	/* ===== Options Tab ===== */
	page_options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (page_options, 6);
	gtk_widget_set_margin_end (page_options, 6);
	gtk_widget_set_margin_top (page_options, 6);
	gtk_widget_set_margin_bottom (page_options, 6);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page_options, gtk_label_new ("Options"));

	/* Checkboxes */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_append (GTK_BOX (page_options), box);

	edit_check_autoconnect = gtk_check_button_new_with_label ("Connect automatically on startup");
	gtk_check_button_set_active (GTK_CHECK_BUTTON (edit_check_autoconnect), (net->flags & FLAG_AUTO_CONNECT) != 0);
	g_signal_connect (edit_check_autoconnect, "toggled", G_CALLBACK (edit_autoconnect_toggled_cb), NULL);
	gtk_box_append (GTK_BOX (box), edit_check_autoconnect);

	edit_check_ssl = gtk_check_button_new_with_label ("Use SSL for all servers");
	gtk_check_button_set_active (GTK_CHECK_BUTTON (edit_check_ssl), (net->flags & FLAG_USE_SSL) != 0);
	g_signal_connect (edit_check_ssl, "toggled", G_CALLBACK (edit_ssl_toggled_cb), NULL);
	gtk_box_append (GTK_BOX (box), edit_check_ssl);

	edit_check_favorite = gtk_check_button_new_with_label ("Mark as favorite");
	gtk_check_button_set_active (GTK_CHECK_BUTTON (edit_check_favorite), (net->flags & FLAG_FAVORITE) != 0);
	g_signal_connect (edit_check_favorite, "toggled", G_CALLBACK (edit_favorite_toggled_cb), NULL);
	gtk_box_append (GTK_BOX (box), edit_check_favorite);

	edit_check_global = gtk_check_button_new_with_label ("Use global user information");
	gtk_check_button_set_active (GTK_CHECK_BUTTON (edit_check_global), (net->flags & FLAG_USE_GLOBAL) != 0);
	g_signal_connect (edit_check_global, "toggled", G_CALLBACK (edit_global_toggled_cb), NULL);
	gtk_box_append (GTK_BOX (box), edit_check_global);

	/* User info grid */
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_append (GTK_BOX (page_options), grid);

	row = 0;

	label = gtk_label_new ("Nick name:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	edit_entry_nick = gtk_entry_new ();
	if (net->nick)
		gtk_editable_set_text (GTK_EDITABLE (edit_entry_nick), net->nick);
	gtk_widget_set_hexpand (edit_entry_nick, TRUE);
	gtk_grid_attach (GTK_GRID (grid), edit_entry_nick, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("Second choice:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	edit_entry_nick2 = gtk_entry_new ();
	if (net->nick2)
		gtk_editable_set_text (GTK_EDITABLE (edit_entry_nick2), net->nick2);
	gtk_widget_set_hexpand (edit_entry_nick2, TRUE);
	gtk_grid_attach (GTK_GRID (grid), edit_entry_nick2, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("User name:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	edit_entry_user = gtk_entry_new ();
	if (net->user)
		gtk_editable_set_text (GTK_EDITABLE (edit_entry_user), net->user);
	gtk_widget_set_hexpand (edit_entry_user, TRUE);
	gtk_grid_attach (GTK_GRID (grid), edit_entry_user, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("Real name:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	edit_entry_real = gtk_entry_new ();
	if (net->real)
		gtk_editable_set_text (GTK_EDITABLE (edit_entry_real), net->real);
	gtk_widget_set_hexpand (edit_entry_real, TRUE);
	gtk_grid_attach (GTK_GRID (grid), edit_entry_real, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("Password:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
	edit_entry_pass = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (edit_entry_pass), FALSE);
	if (net->pass)
		gtk_editable_set_text (GTK_EDITABLE (edit_entry_pass), net->pass);
	gtk_widget_set_hexpand (edit_entry_pass, TRUE);
	gtk_grid_attach (GTK_GRID (grid), edit_entry_pass, 1, row, 1, 1);

	/* Update sensitivity based on global user checkbox */
	edit_toggle_global_user ((net->flags & FLAG_USE_GLOBAL) != 0);

	return dialog;
}

static void
servlist_edit_cb (GtkWidget *button, gpointer user_data)
{
	ircnet *net;

	net = servlist_get_selected_net ();
	if (!net)
		return;

	if (edit_dialog)
	{
		gtk_window_destroy (GTK_WINDOW (edit_dialog));
		edit_dialog = NULL;
	}

	edit_dialog = servlist_create_edit_dialog (net);
	gtk_window_present (GTK_WINDOW (edit_dialog));
}

/* ===== Window Close Handler ===== */

static gboolean
servlist_delete_cb (GtkWidget *widget, gpointer user_data)
{
	servlist_savegui ();
	serverlist_win = NULL;
	selected_net = NULL;

	/* If no sessions exist, exit the application */
	if (sess_list == NULL)
		hexchat_exit ();

	return FALSE;
}

/* ===== Create Server List Window ===== */

static GtkWidget *
servlist_create_window (void)
{
	GtkWidget *window;
	GtkWidget *content;
	GtkWidget *header;
	GtkWidget *main_box;
	GtkWidget *user_frame, *user_box, *user_grid;
	GtkWidget *net_frame, *net_box, *net_hbox;
	GtkWidget *scrolled;
	GtkWidget *list_view;
	GtkWidget *btn_box, *btn;
	GtkWidget *label;
	GtkListItemFactory *factory;
	char title[128];
	int row;

	g_snprintf (title, sizeof (title), "Network List - %s", DISPLAY_NAME);

	window = adw_window_new ();
	gtk_window_set_title (GTK_WINDOW (window), title);
	gtk_window_set_default_size (GTK_WINDOW (window), 450, 500);

	if (main_window)
		gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (main_window));

	g_signal_connect (window, "close-request", G_CALLBACK (servlist_delete_cb), NULL);

	/* Main content box */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	adw_window_set_content (ADW_WINDOW (window), content);

	/* Header bar */
	header = adw_header_bar_new ();
	gtk_box_append (GTK_BOX (content), header);

	/* Main scrollable area */
	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (main_box, 12);
	gtk_widget_set_margin_end (main_box, 12);
	gtk_widget_set_margin_top (main_box, 12);
	gtk_widget_set_margin_bottom (main_box, 12);
	gtk_widget_set_vexpand (main_box, TRUE);
	gtk_box_append (GTK_BOX (content), main_box);

	/* ===== User Information Section ===== */
	user_frame = gtk_frame_new ("User Information");
	gtk_box_append (GTK_BOX (main_box), user_frame);

	user_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (user_box, 12);
	gtk_widget_set_margin_end (user_box, 12);
	gtk_widget_set_margin_top (user_box, 6);
	gtk_widget_set_margin_bottom (user_box, 12);
	gtk_frame_set_child (GTK_FRAME (user_frame), user_box);

	user_grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (user_grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (user_grid), 12);
	gtk_box_append (GTK_BOX (user_box), user_grid);

	row = 0;

	label = gtk_label_new ("Nick name:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (user_grid), label, 0, row, 1, 1);
	entry_nick1 = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry_nick1), prefs.hex_irc_nick1);
	gtk_widget_set_hexpand (entry_nick1, TRUE);
	gtk_grid_attach (GTK_GRID (user_grid), entry_nick1, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("Second choice:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (user_grid), label, 0, row, 1, 1);
	entry_nick2 = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry_nick2), prefs.hex_irc_nick2);
	gtk_widget_set_hexpand (entry_nick2, TRUE);
	gtk_grid_attach (GTK_GRID (user_grid), entry_nick2, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("Third choice:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (user_grid), label, 0, row, 1, 1);
	entry_nick3 = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry_nick3), prefs.hex_irc_nick3);
	gtk_widget_set_hexpand (entry_nick3, TRUE);
	gtk_grid_attach (GTK_GRID (user_grid), entry_nick3, 1, row, 1, 1);
	row++;

	label = gtk_label_new ("User name:");
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_grid_attach (GTK_GRID (user_grid), label, 0, row, 1, 1);
	entry_username = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (entry_username), prefs.hex_irc_user_name);
	gtk_widget_set_hexpand (entry_username, TRUE);
	gtk_grid_attach (GTK_GRID (user_grid), entry_username, 1, row, 1, 1);

	/* ===== Networks Section ===== */
	net_frame = gtk_frame_new ("Networks");
	gtk_widget_set_vexpand (net_frame, TRUE);
	gtk_box_append (GTK_BOX (main_box), net_frame);

	net_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (net_box, 12);
	gtk_widget_set_margin_end (net_box, 12);
	gtk_widget_set_margin_top (net_box, 6);
	gtk_widget_set_margin_bottom (net_box, 12);
	gtk_frame_set_child (GTK_FRAME (net_frame), net_box);

	net_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_vexpand (net_hbox, TRUE);
	gtk_box_append (GTK_BOX (net_box), net_hbox);

	/* Network list */
	scrolled = gtk_scrolled_window_new ();
	gtk_widget_set_hexpand (scrolled, TRUE);
	gtk_widget_set_vexpand (scrolled, TRUE);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled), 200);
	gtk_box_append (GTK_BOX (net_hbox), scrolled);

	networks_store = g_list_store_new (NETWORK_ITEM_TYPE);
	network_selection = gtk_single_selection_new (G_LIST_MODEL (networks_store));
	g_signal_connect (network_selection, "notify::selected", G_CALLBACK (servlist_network_selected), NULL);

	factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (factory, "setup", G_CALLBACK (network_setup_cb), NULL);
	g_signal_connect (factory, "bind", G_CALLBACK (network_bind_cb), NULL);

	list_view = gtk_list_view_new (GTK_SELECTION_MODEL (network_selection), GTK_LIST_ITEM_FACTORY (factory));
	g_signal_connect (list_view, "activate", G_CALLBACK (servlist_row_activated_cb), NULL);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), list_view);
	networks_list = list_view;

	/* Network buttons */
	btn_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_append (GTK_BOX (net_hbox), btn_box);

	btn = gtk_button_new_with_label ("Add");
	g_signal_connect (btn, "clicked", G_CALLBACK (servlist_add_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), btn);

	btn = gtk_button_new_with_label ("Remove");
	g_signal_connect (btn, "clicked", G_CALLBACK (servlist_remove_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), btn);

	btn = gtk_button_new_with_label ("Edit...");
	g_signal_connect (btn, "clicked", G_CALLBACK (servlist_edit_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), btn);

	/* ===== Bottom Buttons ===== */
	btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (btn_box, GTK_ALIGN_END);
	gtk_box_append (GTK_BOX (main_box), btn_box);

	btn = gtk_button_new_with_label ("Close");
	g_signal_connect (btn, "clicked", G_CALLBACK (servlist_delete_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), btn);

	btn = gtk_button_new_with_label ("Connect");
	gtk_widget_add_css_class (btn, "suggested-action");
	g_signal_connect (btn, "clicked", G_CALLBACK (servlist_connect_cb), NULL);
	gtk_box_append (GTK_BOX (btn_box), btn);

	return window;
}

/* ===== Public API ===== */

void
servlist_open (session *sess)
{
	if (serverlist_win)
	{
		gtk_window_present (GTK_WINDOW (serverlist_win));
		return;
	}

	servlist_sess = sess;
	selected_net = NULL;

	serverlist_win = servlist_create_window ();
	servlist_populate ();

	gtk_window_present (GTK_WINDOW (serverlist_win));
}
