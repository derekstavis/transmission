/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: actions.c 14011 2013-02-10 23:06:05Z jordan $
 */

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libtransmission/transmission.h>

#include "actions.h"
#include "conf.h"
#include "tr-core.h"
#include "tr-prefs.h"

#include "icon-lock.h"
#include "icon-logo-24.h"
#include "icon-logo-48.h"
#include "icon-ratio.h"
#include "icon-turtle.h"
#include "icon-utilities.h"

#define UNUSED G_GNUC_UNUSED
#define TR_RESOURCE_PATH "/com/transmissionbt/transmission/"


static TrCore * myCore = NULL;

static void
action_cb (GSimpleAction * a, GVariant * parameter UNUSED, gpointer user_data)
{
    gtr_actions_handler(g_action_get_name (G_ACTION (a)), user_data);
}

static void
radio_action_cb (GSimpleAction * action,
                GVariant * parameter,
                gpointer user_data UNUSED)
{
    g_action_change_state (G_ACTION (action), parameter);
};

static void
change_sort_cb (GSimpleAction * action,
                GVariant * state,
                gpointer user_data UNUSED)
{
    const char * val = g_variant_get_string( state, NULL );

    gtr_core_set_pref (myCore, TR_KEY_sort_mode, val);
    g_simple_action_set_state (action, state);
}

static void
toggle_action_cb (GSimpleAction *  action,
                  GVariant * parameter UNUSED,
                  gpointer user_data UNUSED )
{

    GVariant * state = g_action_get_state( G_ACTION( action ) );
    const gboolean val = g_variant_get_boolean( state );
    printf("toggle_action_cb(%s, %d)\n", g_action_get_name (G_ACTION (action)), val);

    g_action_change_state( G_ACTION( action ), g_variant_new_boolean( !val ) );
    g_variant_unref( state );
};

static void
change_pref_cb (GSimpleAction * action,
                GVariant * state,
                gpointer user_data UNUSED)
 {
    const char * key = g_action_get_name (G_ACTION (action));
    size_t len = 0;
    tr_quark entry_name = TR_KEY_NONE;

    g_return_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN));
    
    if (key == NULL)
        len = 0;
    else if (len == (size_t)-1)
        len = strlen (key);

    if (tr_quark_lookup (key, len, &entry_name)) {
       printf("set_pref_bool %s, %d\n", key, g_variant_get_boolean(state)); 
       gtr_core_set_pref_bool (myCore, entry_name, g_variant_get_boolean (state));
       g_simple_action_set_state (action, state);
    }
};

static void
change_ratio_limit_cb (GSimpleAction * action,
                       GVariant      * ratio,
                       gpointer user_data UNUSED)
 {
    const char * key = g_action_get_name (G_ACTION (action));
    size_t len = 0;
    tr_quark entry_name = TR_KEY_NONE;
    double value;

    g_return_if_fail (g_variant_is_of_type (ratio, G_VARIANT_TYPE_DOUBLE));
   
    value = g_variant_get_double(ratio);
 
    if (key == NULL)
        len = 0;
    else if (len == (size_t)-1)
        len = strlen (key);

    if (tr_quark_lookup (key, len, &entry_name)) {
      gtr_core_set_pref_double (myCore, TR_KEY_ratio_limit, value);
      gtr_core_set_pref_bool (myCore, TR_KEY_ratio_limit_enabled, TRUE);
    
      g_simple_action_set_state (action, ratio);
    }
};


static void
change_toggle_cb (GSimpleAction * action,
                  GVariant * state,
                  gpointer user_data UNUSED)
{
    gtr_actions_handler( g_action_get_name( G_ACTION( action ) ), user_data );
    g_simple_action_set_state( action, state );
};

static GActionEntry win_entries[] =
{
    /* radio actions */
    { "sort-mode",                     radio_action_cb, "s", "\"sort-by-activity\"", change_sort_cb, {} },
    { "ratio-limit",                   radio_action_cb, "d", "0.20", change_ratio_limit_cb, {} },
    

    /* show toggle actions */
    { "toggle-main-window",            toggle_action_cb, NULL, "true", change_toggle_cb, {} },
    { "toggle-message-log",            toggle_action_cb, NULL, "false", change_toggle_cb, {} },

    /* pref toggle actions */
    { "alt-speed-enabled",             toggle_action_cb, NULL, "false", change_pref_cb, {} },
    { "compact-view",                  toggle_action_cb, NULL, "false", change_pref_cb, {} },
    { "sort-reversed",                 toggle_action_cb, NULL, "false", change_pref_cb, {} },
    { "show-filterbar",                toggle_action_cb, NULL, "true", change_pref_cb, {} },
    { "show-statusbar",                toggle_action_cb, NULL, "true", change_pref_cb, {} },
    { "show-toolbar",                  toggle_action_cb, NULL, "true", change_pref_cb, {} },
    { "ratio-limit-enabled",           toggle_action_cb, NULL, "true", change_pref_cb, {} },

    /* plain actions */
    { "copy-magnet-link-to-clipboard", action_cb, NULL, NULL, NULL, {} },
    { "open-torrent-from-url",         action_cb, NULL, NULL, NULL, {} },
    { "open-torrent",                  action_cb, NULL, NULL, NULL, {} },
    { "torrent-start",                 action_cb, NULL, NULL, NULL, {} },
    { "torrent-start-now",             action_cb, NULL, NULL, NULL, {} },
    { "show-stats",                    action_cb, NULL, NULL, NULL, {} },
    { "donate",                        action_cb, NULL, NULL, NULL, {} },
    { "torrent-verify",                action_cb, NULL, NULL, NULL, {} },
    { "torrent-stop",                  action_cb, NULL, NULL, NULL, {} },
    { "pause-all-torrents",            action_cb, NULL, NULL, NULL, {} },
    { "start-all-torrents",            action_cb, NULL, NULL, NULL, {} },
    { "relocate-torrent",              action_cb, NULL, NULL, NULL, {} },
    { "remove-torrent",                action_cb, NULL, NULL, NULL, {} },
    { "delete-torrent",                action_cb, NULL, NULL, NULL, {} },
    { "new-torrent",                   action_cb, NULL, NULL, NULL, {} },
    { "quit",                          action_cb, NULL, NULL, NULL, {} },
    { "select-all",                    action_cb, NULL, NULL, NULL, {} },
    { "deselect-all",                  action_cb, NULL, NULL, NULL, {} },
    { "preferences",                   action_cb, NULL, NULL, NULL, {} },
    { "show-torrent-properties",       action_cb, NULL, NULL, NULL, {} },
    { "open-torrent-folder",           action_cb, NULL, NULL, NULL, {} },
    { "show-about-dialog",             action_cb, NULL, NULL, NULL, {} },
    { "help",                          action_cb, NULL, NULL, NULL, {} },
    { "torrent-reannounce", action_cb, NULL, NULL, NULL, {} },
    { "queue-move-top", action_cb, NULL, NULL, NULL, {} },
    { "queue-move-up", action_cb, NULL, NULL, NULL, {} },
    { "queue-move-down", action_cb, NULL, NULL, NULL, {} },
    { "queue-move-bottom", action_cb, NULL, NULL, NULL, {} },
    { "present-main-window", action_cb, NULL, NULL, NULL, {} }
};

static void
update_entry_states( GActionEntry *entries, int n_entries )
{
    int i;

    g_return_if_fail( entries != NULL || n_entries == 0 );

    for (i = 0; n_entries == -1 ? entries[i].name != NULL : i < n_entries; i++)
    {
        GActionEntry *entry = &entries[i];

        if ( strcmp( entry->name, "sort-mode" ) == 0 )
        {
            const char *value = gtr_pref_string_get (TR_KEY_sort_mode);
            entry->state = g_strdup_printf( "\"%s\"", value ); /* FIXME: leak */
        }
        else if ( strcmp( entry->name, "alt-speed-enabled" ) == 0 ||
                  strcmp( entry->name, "compact-view" ) == 0 ||
                  strcmp( entry->name, "sort-reversed" ) == 0 ||
                  strcmp( entry->name, "show-filterbar" ) == 0 ||
                  strcmp( entry->name, "show-statusbar" ) == 0 ||
                  strcmp( entry->name, "show-toolbar" ) == 0 )
        {
	    tr_quark entry_name = TR_KEY_NONE;
            gboolean value;
            size_t len = 0;

            if (entry->name == NULL)
                len = 0;
            else if (len == (size_t)-1)
                len = strlen (entry->name);

            if (tr_quark_lookup (entry->name, len, &entry_name)) {
                value = gtr_pref_flag_get (entry_name);
                entry->state = value ? "true" : "false";
            }
        }
    }
}

typedef struct
{
    const guint8*   raw;
    const char *    name;
}
BuiltinIconInfo;

static const BuiltinIconInfo my_fallback_icons[] =
{
    { tr_icon_logo_48,  WINDOW_ICON          },
    { tr_icon_logo_24,  TRAY_ICON            },
    { tr_icon_logo_48,  NOTIFICATION_ICON    },
    { tr_icon_lock,     "transmission-lock"  },
    { utilities_icon,   "utilities"          },
    { blue_turtle,      "alt-speed-on"       },
    { grey_turtle,      "alt-speed-off"      },
    { ratio_icon,       "ratio"              }
};

static void
register_my_icons (void)
{
  int i;
  const int n = G_N_ELEMENTS (my_fallback_icons);
  GtkIconTheme * theme = gtk_icon_theme_get_default ();

  for (i=0; i<n; ++i)
    {
      const char * name = my_fallback_icons[i].name;

      if (!gtk_icon_theme_has_icon (theme, name))
        {
          GdkPixbuf * p;

          p = gdk_pixbuf_new_from_inline (-1, my_fallback_icons[i].raw, FALSE, NULL);

          if (p != NULL)
            {
              int width;

              width = gdk_pixbuf_get_width (p);

              gtk_icon_theme_add_builtin_icon (name, width, p);

              g_object_unref (p);
            }
        }
    }

}

void
gtr_actions_set_core (TrCore * core)
{
    myCore = core;
}

void
gtr_actions_init( GtkApplication * app, gpointer callback_user_data UNUSED )
{
    gtk_application_set_app_menu (app, gtr_action_get_menu_model ("menubar"));
    register_my_icons();
}

void
gtr_actions_add_to_map( GActionMap * map, gpointer callback_user_data)
{
    g_return_if_fail( GTK_IS_APPLICATION_WINDOW( map ) );
 
    update_entry_states( win_entries, G_N_ELEMENTS( win_entries ) );
    g_action_map_add_action_entries( map, win_entries,
                                     G_N_ELEMENTS( win_entries ),
                                     callback_user_data );
}

/****
*****
****/

static GAction*
get_action( const char* name )
{
    GtkApplication *app = GTK_APPLICATION( g_application_get_default ( ) );
    GList *windows = gtk_application_get_windows( app );

    if (windows->data) {
      return g_action_map_lookup_action( G_ACTION_MAP( windows->data ), name );
    }

    return NULL;
}

void
gtr_action_activate (const char * name)
{
    GAction * action = get_action (name);

    g_assert (action != NULL);
    g_action_activate (action, NULL);
}

void
gtr_action_set_sensitive (const char * name, gboolean b)
 {
    GAction * action = get_action( name );
 
    g_assert( action != NULL );
    g_simple_action_set_enabled( G_SIMPLE_ACTION( action ), b );
}

void
gtr_action_set_toggled (const char * name, gboolean b)
{
    GSimpleAction * action = G_SIMPLE_ACTION( get_action( name ) );

    g_simple_action_set_state( action , g_variant_new_boolean( b ) );
}

GMenuModel *
gtr_action_get_menu_model( const char * id )
{
    static GtkBuilder * builder = NULL;
 
    if ( builder == NULL )
    {
        builder = gtk_builder_new( );
        gtk_builder_add_from_resource( builder, TR_RESOURCE_PATH "transmission-menus.ui", NULL );
    }

    return G_MENU_MODEL (gtk_builder_get_object (builder, id));
}
