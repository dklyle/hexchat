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
AdwTabView *tab_view = NULL;

static gboolean done = FALSE;
static gboolean notifications_enabled = TRUE;
static feicon current_tray_icon = FE_ICON_NORMAL;

/* Forward declarations for marker line functions */
static void set_marker_line (session *sess);
static void draw_marker_line (session *sess);
static void clear_marker_line (session *sess);

/* Forward declaration for notification */
static void show_notification (const char *title, const char *body);

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

/* mIRC color palette (16 colors + extended 99 colors) */
/* These are standard mIRC colors as RGB hex values */
static const char *mirc_colors[] = {
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
	/* Extended colors 16-98 follow the extended mIRC palette */
	"#470000", /* 16 */
	"#472100", /* 17 */
	"#474700", /* 18 */
	"#324700", /* 19 */
	"#004700", /* 20 */
	"#00472C", /* 21 */
	"#004747", /* 22 */
	"#002747", /* 23 */
	"#000047", /* 24 */
	"#2E0047", /* 25 */
	"#470047", /* 26 */
	"#47002A", /* 27 */
	"#740000", /* 28 */
	"#743A00", /* 29 */
	"#747400", /* 30 */
	"#517400", /* 31 */
	"#007400", /* 32 */
	"#007449", /* 33 */
	"#007474", /* 34 */
	"#004074", /* 35 */
	"#000074", /* 36 */
	"#4B0074", /* 37 */
	"#740074", /* 38 */
	"#740045", /* 39 */
	"#B50000", /* 40 */
	"#B56300", /* 41 */
	"#B5B500", /* 42 */
	"#7DB500", /* 43 */
	"#00B500", /* 44 */
	"#00B571", /* 45 */
	"#00B5B5", /* 46 */
	"#0063B5", /* 47 */
	"#0000B5", /* 48 */
	"#7500B5", /* 49 */
	"#B500B5", /* 50 */
	"#B5006B", /* 51 */
	"#FF0000", /* 52 */
	"#FF8C00", /* 53 */
	"#FFFF00", /* 54 */
	"#B2FF00", /* 55 */
	"#00FF00", /* 56 */
	"#00FFA0", /* 57 */
	"#00FFFF", /* 58 */
	"#008CFF", /* 59 */
	"#0000FF", /* 60 */
	"#A500FF", /* 61 */
	"#FF00FF", /* 62 */
	"#FF0098", /* 63 */
	"#FF5959", /* 64 */
	"#FFB459", /* 65 */
	"#FFFF71", /* 66 */
	"#CFFF60", /* 67 */
	"#6FFF6F", /* 68 */
	"#65FFC9", /* 69 */
	"#6DFFFF", /* 70 */
	"#59B4FF", /* 71 */
	"#5959FF", /* 72 */
	"#C459FF", /* 73 */
	"#FF66FF", /* 74 */
	"#FF59BC", /* 75 */
	"#FF9C9C", /* 76 */
	"#FFD39C", /* 77 */
	"#FFFF9C", /* 78 */
	"#E2FF9C", /* 79 */
	"#9CFF9C", /* 80 */
	"#9CFFDB", /* 81 */
	"#9CFFFF", /* 82 */
	"#9CD3FF", /* 83 */
	"#9C9CFF", /* 84 */
	"#DC9CFF", /* 85 */
	"#FF9CFF", /* 86 */
	"#FF94D3", /* 87 */
	"#000000", /* 88 */
	"#131313", /* 89 */
	"#282828", /* 90 */
	"#363636", /* 91 */
	"#4D4D4D", /* 92 */
	"#656565", /* 93 */
	"#818181", /* 94 */
	"#9F9F9F", /* 95 */
	"#BCBCBC", /* 96 */
	"#E2E2E2", /* 97 */
	"#FFFFFF", /* 98 */
};
#define MIRC_COLORS_COUNT (sizeof(mirc_colors) / sizeof(mirc_colors[0]))

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

/* Get session from a tab page */
static session *
get_session_from_page (AdwTabPage *page)
{
	GSList *list;
	session *sess;

	if (!page)
		return NULL;

	for (list = sess_list; list; list = list->next)
	{
		sess = list->data;
		if (sess && sess->gui && ((session_gui *)sess->gui)->tab_page == page)
			return sess;
	}
	return NULL;
}

/* Callback when selected tab changes */
static void
tab_selected_changed_cb (AdwTabView *view, GParamSpec *pspec, gpointer user_data)
{
	AdwTabPage *new_page;
	session *new_sess;

	new_page = adw_tab_view_get_selected_page (view);
	new_sess = get_session_from_page (new_page);

	/* Set marker on the previous session before switching */
	if (prev_selected_sess && prev_selected_sess != new_sess)
	{
		set_marker_line (prev_selected_sess);
		draw_marker_line (prev_selected_sess);
	}

	/* Clear marker on the new session (user is now viewing it) */
	if (new_sess && new_sess->gui)
	{
		clear_marker_line (new_sess);
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

	/* Create tab view for sessions */
	tab_view = ADW_TAB_VIEW (adw_tab_view_new ());
	
	/* Create tab bar */
	AdwTabBar *tab_bar = adw_tab_bar_new ();
	adw_tab_bar_set_view (ADW_TAB_BAR (tab_bar), tab_view);
	adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), GTK_WIDGET (tab_bar));

	/* Connect to page selection changes for marker line support */
	g_signal_connect (tab_view, "notify::selected-page",
	                  G_CALLBACK (tab_selected_changed_cb), NULL);

	/* Set tab view as main content */
	adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), GTK_WIDGET (tab_view));
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

	/* mIRC color tags (0-98) - use the global mirc_colors palette */
	for (i = 0; i < MIRC_COLORS_COUNT; i++)
	{
		g_snprintf (tag_name, sizeof (tag_name), "fg-%02u", i);
		gtk_text_buffer_create_tag (buffer, tag_name,
		                            "foreground", mirc_colors[i],
		                            NULL);

		g_snprintf (tag_name, sizeof (tag_name), "bg-%02u", i);
		gtk_text_buffer_create_tag (buffer, tag_name,
		                            "background", mirc_colors[i],
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
	                            "foreground", "#000000",
	                            "background", "#FFFFFF",
	                            NULL);

	/* Special tags */
	gtk_text_buffer_create_tag (buffer, "timestamp",
	                            "foreground", "#888888",
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "url",
	                            "foreground", "#0000FF",
	                            "underline", PANGO_UNDERLINE_SINGLE,
	                            NULL);
	gtk_text_buffer_create_tag (buffer, "highlight",
	                            "background", "#FFFF00",
	                            NULL);
	/* Marker line tag - a red line that marks the last read position */
	gtk_text_buffer_create_tag (buffer, "marker-line",
	                            "paragraph-background", "#FF3902",
	                            "pixels-above-lines", 2,
	                            "pixels-below-lines", 2,
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
	guint button;

	widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

	if (button == GDK_BUTTON_SECONDARY)
	{
		/* Right-click - show context menu */
		userlist_popup_menu (sess, x, y, widget);
	}
	else if (button == GDK_BUTTON_PRIMARY && n_press == 2)
	{
		/* Double-click - open private chat */
		char *nick = userlist_get_selected_nick (sess);
		if (nick)
		{
			/* Execute the configured double-click action */
			if (prefs.hex_gui_ulist_doubleclick[0])
			{
				nick_command_parse (sess, prefs.hex_gui_ulist_doubleclick, nick, nick);
			}
			g_free (nick);
		}
	}
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

	/* Create input entry */
	gui->input_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (gui->input_entry), "Type a message...");
	gtk_box_append (GTK_BOX (text_box), gui->input_entry);

	/* Setup input handling - activate (Enter) signal */
	g_signal_connect (gui->input_entry, "activate",
	                  G_CALLBACK (input_activate_cb), sess);

	/* Setup key controller for history (Up/Down) and tab completion */
	GtkEventController *key_controller = gtk_event_controller_key_new ();
	g_signal_connect (key_controller, "key-pressed",
	                  G_CALLBACK (input_key_pressed_cb), sess);
	gtk_widget_add_controller (gui->input_entry, key_controller);

	/* Create userlist */
	GtkWidget *userlist_scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (userlist_scroll),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (userlist_scroll, 140, -1);

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

	/* Add click gesture for right-click context menu and double-click */
	GtkGesture *click_gesture = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), 0); /* Listen to all buttons */
	g_signal_connect (click_gesture, "pressed", G_CALLBACK (userlist_click_cb), sess);
	gtk_widget_add_controller (gui->userlist_view, GTK_EVENT_CONTROLLER (click_gesture));

	/* Pack into paned */
	gtk_paned_set_start_child (GTK_PANED (gui->paned), text_box);
	gtk_paned_set_end_child (GTK_PANED (gui->paned), userlist_scroll);
	gtk_paned_set_resize_start_child (GTK_PANED (gui->paned), TRUE);
	gtk_paned_set_resize_end_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_shrink_start_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_shrink_end_child (GTK_PANED (gui->paned), FALSE);
	gtk_paned_set_position (GTK_PANED (gui->paned), 700);

	gtk_box_append (GTK_BOX (main_box), gui->paned);

	/* Add to tab view */
	if (tab_view)
	{
		const char *tab_title;

		/* Get valid UTF-8 title for tab */
		if (sess->channel[0] && g_utf8_validate (sess->channel, -1, NULL))
			tab_title = sess->channel;
		else
			tab_title = _("New Tab");

		gui->tab_page = adw_tab_view_append (tab_view, main_box);
		adw_tab_page_set_title (gui->tab_page, tab_title);

		if (focus)
			adw_tab_view_set_selected_page (tab_view, gui->tab_page);
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
			if (gui->tab_page && tab_view)
				adw_tab_view_close_page (tab_view, gui->tab_page);
		}

		/* Clear references but don't unref - GTK owns these */
		gui->text_buffer = NULL;
		gui->text_view = NULL;
		gui->userlist_store = NULL;
		gui->tab_page = NULL;

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
				/* Clamp to valid range */
				if (fg_color >= (int)MIRC_COLORS_COUNT)
					fg_color = fg_color % 16;

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
						if (bg_color >= (int)MIRC_COLORS_COUNT)
							bg_color = bg_color % 16;
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
	/* TODO: Use AdwAlertDialog */
	g_print ("%s\n", msg);
}

/* ===== Channel/Topic display ===== */

void
fe_set_topic (struct session *sess, char *topic, char *stripped_topic)
{
	/* TODO: Update topic display in header */
}

void
fe_set_channel (struct session *sess)
{
	session_gui *gui;
	const char *tab_title;

	if (!sess || !sess->gui)
		return;

	gui = sess->gui;

	if (gui->tab_page)
	{
		/* Get valid UTF-8 title for tab */
		if (sess->channel[0] && g_utf8_validate (sess->channel, -1, NULL))
			tab_title = sess->channel;
		else
			tab_title = _("New Tab");

		adw_tab_page_set_title (gui->tab_page, tab_title);
	}
}

void
fe_set_title (struct session *sess)
{
	/* TODO: Update window title */
}

void
fe_set_nonchannel (struct session *sess, int state)
{
	/* TODO: Indicate non-channel session state */
}

void
fe_clear_channel (struct session *sess)
{
	/* TODO: Clear channel-specific UI elements */
}

void
fe_set_tab_color (struct session *sess, tabcolor col)
{
	/* TODO: Set tab indicator based on activity */
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
	/* TODO: Update user count display in status bar or header */
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
	/* TODO: Update nick display */
}

void
fe_set_lag (server *serv, long lag)
{
	/* TODO: Update lag meter */
}

void
fe_set_throttle (server *serv)
{
	/* TODO: Update throttle meter */
}

void
fe_set_away (server *serv)
{
	/* TODO: Update away status */
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

void
fe_get_bool (char *title, char *prompt, void *callback, void *userdata)
{
	/* TODO: Show yes/no dialog */
}

void
fe_get_str (char *prompt, char *def, void *callback, void *ud)
{
	/* TODO: Show string input dialog */
}

void
fe_get_int (char *prompt, int def, void *callback, void *ud)
{
	/* TODO: Show integer input dialog */
}

void
fe_get_file (const char *title, char *initial,
             void (*callback) (void *userdata, char *file), void *userdata,
             int flags)
{
	/* TODO: Show file chooser dialog */
}

void
fe_confirm (const char *message, void (*yesproc)(void *), void (*noproc)(void *), void *ud)
{
	/* TODO: Show confirmation dialog */
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
	/* TODO: Open channel list window */
	serv->p_list_channels (serv, filter, 1);
}

void
fe_lastlog (session *sess, session *lastlog_sess, char *sstr, gtk_xtext_search_flags flags)
{
	/* TODO: Search/lastlog functionality */
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
			if (gui->tab_page && tab_view)
				adw_tab_view_set_selected_page (tab_view, gui->tab_page);
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
	return -1;
}

void *
fe_gui_info_ptr (session *sess, int info_type)
{
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
