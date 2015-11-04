/******************************************************************************
 * $Id: tr-window.c 14142 2013-07-24 00:13:31Z jordan $
 *
 * Copyright (c) Transmission authors and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#include <string.h> /* strlen() */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> /* tr_formatter_speed_KBps () */

#include "actions.h"
#include "conf.h"
#include "filter.h"
#include "hig.h"
#include "torrent-cell-renderer.h"
#include "tr-prefs.h"
#include "tr-window.h"
#include "tr-limit-popover.h"
#include "util.h"

#define TR_ROOT_PATH "/com/transmissionbt/transmission/"
#define TR_CSS_RESOURCE_PATH TR_ROOT_PATH "css/transmission.scss"
#define TR_ICON_RESOURCE_PATH TR_ROOT_PATH "icons"

typedef struct
{
    GtkWidget          * scroll;
    GtkWidget          * view;
    GtkWidget          * toolbar;
    GtkWidget          * limit_popover;
    GtkWidget          * filter;
    GtkWidget          * status;
    GtkWidget          * status_menu;
    GtkLabel           * ul_lb;
    GtkLabel           * dl_lb;
    GtkLabel           * stats_lb;
    GtkWidget          * alt_speed_image;
    GtkWidget          * alt_speed_button;
    GtkWidget          * options_menu;
    GtkTreeSelection   * selection;
    GtkCellRenderer    * renderer;
    GtkTreeViewColumn  * column;
    GtkTreeModel       * filter_model;
    TrCore             * core;
    gulong               pref_handler_id;
}
PrivateData;

static TR_DEFINE_QUARK (private_data, private_data)

static PrivateData*
get_private_data (GtkWindow * w)
{
  return g_object_get_qdata (G_OBJECT (w), private_data_quark ());
}

/***
****
***/

static void
on_popup_menu (GtkWidget * self UNUSED,
               GdkEventButton * event)
{
  static GtkWidget * menu = NULL;
 
  GMenuModel *model = gtr_action_get_menu_model( "torrent-options-popup" );
  menu = gtk_menu_new_from_model( model );
  gtk_menu_attach_to_widget( GTK_MENU( menu ), self, NULL );

  g_object_unref( model );

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  (event ? event->button : 0),
                  (event ? event->time : 0));
}

static void
view_row_activated (GtkTreeView       * tree_view UNUSED,
                    GtkTreePath       * path      UNUSED,
                    GtkTreeViewColumn * column    UNUSED,
                    gpointer            user_data UNUSED)
{
  gtr_action_activate ("show-torrent-properties");
}

static gboolean
tree_view_search_equal_func (GtkTreeModel * model,
                             gint           column UNUSED,
                             const gchar  * key,
                             GtkTreeIter  * iter,
                             gpointer       search_data UNUSED)
{
    gboolean match;
    char * lower;
    const char * name = NULL;

    lower = g_strstrip (g_utf8_strdown (key, -1));
    gtk_tree_model_get (model, iter, MC_NAME_COLLATED, &name, -1);
    match = strstr (name, lower) != NULL;
    g_free (lower);

    return !match;
}

static GtkWidget*
makeview (PrivateData * p)
{
  GtkWidget * view;
  GtkTreeViewColumn * col;
  GtkTreeSelection * sel;
  GtkCellRenderer * r;
  GtkTreeView * tree_view;

  view = gtk_tree_view_new ();
  tree_view = GTK_TREE_VIEW (view);
  gtk_tree_view_set_search_column (tree_view, MC_NAME_COLLATED);
  gtk_tree_view_set_search_equal_func (tree_view,
                                       tree_view_search_equal_func,
                                       NULL, NULL);
  gtk_tree_view_set_headers_visible (tree_view, FALSE);
  gtk_tree_view_set_fixed_height_mode (tree_view, TRUE);

  p->selection = gtk_tree_view_get_selection (tree_view);

  p->column = col = GTK_TREE_VIEW_COLUMN (g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
    "title", _("Torrent"),
    "resizable", TRUE,
    "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
    NULL));

  p->renderer = r = torrent_cell_renderer_new ();
  gtk_tree_view_column_pack_start (col, r, FALSE);
  gtk_tree_view_column_add_attribute (col, r, "torrent", MC_TORRENT);
  gtk_tree_view_column_add_attribute (col, r, "piece-upload-speed", MC_SPEED_UP);
  gtk_tree_view_column_add_attribute (col, r, "piece-download-speed", MC_SPEED_DOWN);

  gtk_tree_view_append_column (tree_view, col);
  g_object_set (r, "xpad", GUI_PAD_SMALL, "ypad", GUI_PAD_SMALL, NULL);

  sel = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (sel),
                               GTK_SELECTION_MULTIPLE);

  g_signal_connect (view, "popup-menu",
                    G_CALLBACK (on_popup_menu), NULL);
  g_signal_connect (view, "button-press-event",
                    G_CALLBACK (on_tree_view_button_pressed),
                    (void *) on_popup_menu);
  g_signal_connect (view, "button-release-event",
                    G_CALLBACK (on_tree_view_button_released), NULL);
  g_signal_connect (view, "row-activated",
                    G_CALLBACK (view_row_activated), NULL);


  gtk_tree_view_set_model (tree_view, p->filter_model);
  g_object_unref (p->filter_model);

  return view;
}

static void syncAltSpeedButton (PrivateData * p);

static void
prefsChanged (TrCore           * core UNUSED,
              const tr_quark     key,
              gpointer           wind)
{
  gboolean isEnabled;
  int limit;
  double ratio;
  PrivateData * p = get_private_data (GTK_WINDOW (wind));

  switch (key)
    {
      case TR_KEY_compact_view:
        g_object_set (p->renderer, "compact", gtr_pref_flag_get (key), NULL);
        /* since the cell size has changed, we need gtktreeview to revalidate
         * its fixed-height mode values. Unfortunately there's not an API call
         * for that, but it *does* revalidate when it thinks the style's been tweaked */
        g_signal_emit_by_name (p->view, "style-updated", NULL, NULL);
        break;

      case TR_KEY_show_statusbar:
        isEnabled = gtr_pref_flag_get (key);
        g_object_set (p->status, "visible", isEnabled, NULL);
        break;

      case TR_KEY_show_filterbar:
        isEnabled = gtr_pref_flag_get (key);
        g_object_set (p->filter, "visible", isEnabled, NULL);
        break;

      case TR_KEY_show_toolbar:
        isEnabled = gtr_pref_flag_get (key);
        g_object_set (p->toolbar, "visible", isEnabled, NULL);
        break;

      case TR_KEY_statusbar_stats:
        gtr_window_refresh (wind);
        break;

      case TR_KEY_speed_limit_down:
        limit = gtr_pref_int_get(key);
        tr_limit_popover_set_speed_limit_down (
          TR_LIMIT_POPOVER (p->limit_popover), limit);
        break;

      case TR_KEY_speed_limit_up:
        limit = gtr_pref_int_get(key);
        tr_limit_popover_set_speed_limit_up (
          TR_LIMIT_POPOVER (p->limit_popover), limit);
        break;

      case TR_KEY_ratio_limit:
        ratio = gtr_pref_double_get(key);
        tr_limit_popover_set_ratio_limit (
          TR_LIMIT_POPOVER (p->limit_popover), ratio);
        break;

      case TR_KEY_alt_speed_enabled:
      case TR_KEY_alt_speed_up:
      case TR_KEY_alt_speed_down:
        syncAltSpeedButton (p);
        break;

      default:
        break;
    }
}

static void
privateFree (gpointer vprivate)
{
  PrivateData * p = vprivate;
  g_signal_handler_disconnect (p->core, p->pref_handler_id);
  g_free (p);
}

#define STATS_MODE "stats-mode"

static void
syncAltSpeedButton (PrivateData * p)
{
  char u[32];
  char d[32];
  char * str;
  const char * fmt;
  const gboolean b = gtr_pref_flag_get (TR_KEY_alt_speed_enabled);
  const char * icon = b ? "alternative-speed-on-symbolic"
                        : "alternative-speed-off-symbolic";
  GtkWidget * w = p->alt_speed_button;

  tr_formatter_speed_KBps (u, gtr_pref_int_get (TR_KEY_alt_speed_up), sizeof (u));
  tr_formatter_speed_KBps (d, gtr_pref_int_get (TR_KEY_alt_speed_down), sizeof (d));
  fmt = b ? _("Click to disable Alternative Speed Limits\n (%1$s down, %2$s up)")
          : _("Click to enable Alternative Speed Limits\n (%1$s down, %2$s up)");
  str = g_strdup_printf (fmt, d, u);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), b);
  gtk_image_set_from_icon_name (GTK_IMAGE (p->alt_speed_image), icon, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_set_halign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (w, str);

  g_free (str);
}

static void
alt_speed_toggled_cb (GtkToggleButton * button, gpointer vprivate)
{
  PrivateData * p = vprivate;
  const gboolean b = gtk_toggle_button_get_active (button);
  gtr_core_set_pref_bool (p->core, TR_KEY_alt_speed_enabled,  b);
}

/***
****  FILTER
***/

static void
onFilterChanged (GtkToggleButton * button, gpointer vp)
{
  PrivateData * p = vp;
  GtkWidget   * w = p->filter;
  const gboolean b = gtk_toggle_button_get_active (button);

  gtk_revealer_set_reveal_child (GTK_REVEALER (w), b);

}

static gboolean
onAltSpeedToggledIdle (gpointer vp)
{
  PrivateData * p = vp;
  gboolean b = tr_sessionUsesAltSpeed (gtr_core_session (p->core));
  gtr_core_set_pref_bool (p->core, TR_KEY_alt_speed_enabled, b);

  return G_SOURCE_REMOVE;
}

static void
onAltSpeedToggled (tr_session * s UNUSED, bool isEnabled UNUSED, bool byUser UNUSED, void * p)
{
  gdk_threads_add_idle (onAltSpeedToggledIdle, p);
}

static GMenuModel *
get_statistics_menu_model ()
{
  unsigned int i;
  GMenuItem * mi;
  GMenu     * m;

  struct {
    const char *  val, *i18n;
  } stats_modes[] = {
    { "total-ratio",      N_("Total Ratio")        },
    { "session-ratio",    N_("Session Ratio")      },
    { "total-transfer",   N_("Total Transfer")     },
    { "session-transfer", N_("Session Transfer")   }
  };

  const char * action_key;
  char detailed_action[256];

  action_key  = tr_quark_get_string(TR_KEY_statusbar_stats, NULL);
  
  m = g_menu_new ();

  for (i = 0; i < G_N_ELEMENTS (stats_modes); i++) {
    g_snprintf(detailed_action, 
               sizeof (detailed_action), 
               "win.%s('%s')", 
               action_key, stats_modes[i].val);

    mi = g_menu_item_new (stats_modes[i].i18n, detailed_action);
    g_menu_append_item (m, mi);
  }

  return G_MENU_MODEL (m);
}

/***
****  Speed limit popover
***/


void 
on_ratio_limit(TrLimitPopover *popover UNUSED, double ratio, gpointer vp)
{
  PrivateData *p = vp;

  gtr_core_set_pref_double (p->core, TR_KEY_ratio_limit, ratio);
  gtr_core_set_pref_bool (p->core, TR_KEY_ratio_limit_enabled, TRUE);
}

void 
on_speed_limit_up(TrLimitPopover *popover UNUSED, int limit, gpointer vp)
{
  PrivateData *p = vp;

  gtr_core_set_pref_int (p->core, TR_KEY_speed_limit_up, limit);
  gtr_core_set_pref_bool (p->core, TR_KEY_speed_limit_up_enabled, TRUE);
}

void 
on_speed_limit_down(TrLimitPopover *popover UNUSED, int limit, gpointer vp)
{
  PrivateData *p = vp;

  gtr_core_set_pref_int (p->core, TR_KEY_speed_limit_down, limit);
  gtr_core_set_pref_bool (p->core, TR_KEY_speed_limit_down_enabled, TRUE);
}

/***
**** Start/Pause all torrents headerbar button
***/

void
on_start_all_torrents_toggled (GtkToggleButton *button, gpointer vp)
{
  GtkApplicationWindow *win = vp;
  gboolean active = gtk_toggle_button_get_active (button);
  GAction *action;
  
  if (active) {
    action = g_action_map_lookup_action (G_ACTION_MAP (win),
                                         "start-all-torrents");
    gtk_button_set_image (GTK_BUTTON (button),
      gtk_image_new_from_icon_name ("media-playback-pause-symbolic",
                                    GTK_ICON_SIZE_MENU));
  } else {
    action = g_action_map_lookup_action (G_ACTION_MAP (win), 
                                         "pause-all-torrents");
    gtk_button_set_image (GTK_BUTTON (button),
      gtk_image_new_from_icon_name ("media-playback-start-symbolic",
                                    GTK_ICON_SIZE_MENU));
  }
}

/***
****  PUBLIC
***/

GtkWidget *
gtr_status_bar_new (PrivateData *p)
{
  GtkWidget * ul_lb, * dl_lb;
  GtkWidget * w, * img, * box, * pop, * box_wrapper;  

  box_wrapper = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_style_context_add_class (gtk_widget_get_style_context (box_wrapper),
    "action-bar");

  w = gtk_menu_button_new ();
  gtk_button_set_image (GTK_BUTTON (w), gtk_image_new_from_icon_name (
    "network-transmit-receive-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
  
  pop = gtk_popover_new (w);

  box = p->limit_popover = GTK_WIDGET (tr_limit_popover_new ());
  gtk_container_add (GTK_CONTAINER (pop), GTK_WIDGET (box));
  gtk_widget_show_all (box);

  g_signal_connect (box, "speed-limit-up", G_CALLBACK (on_speed_limit_up), p);
  g_signal_connect (box, "speed-limit-down", G_CALLBACK (on_speed_limit_down), p);
  g_signal_connect (box, "ratio-limit", G_CALLBACK (on_ratio_limit), p);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (w), GTK_WIDGET (pop));

  gtk_box_pack_start (GTK_BOX (box_wrapper), w, FALSE, FALSE, 0);

  /* turtle */
  p->alt_speed_image = gtk_image_new ();
  w = p->alt_speed_button = gtk_toggle_button_new ();
  gtk_button_set_image (GTK_BUTTON (w), p->alt_speed_image);
  g_signal_connect (w, "toggled", G_CALLBACK (alt_speed_toggled_cb), p);
  gtk_box_pack_start (GTK_BOX (box_wrapper), w, FALSE, FALSE, 0);

  /* download */
  w = dl_lb = gtk_label_new (NULL);
  p->dl_lb = GTK_LABEL (w);
  gtk_label_set_single_line_mode (p->dl_lb, TRUE);
  gtk_box_pack_start (GTK_BOX (box_wrapper), w, TRUE, FALSE, 0);

  /* upload */
  w = ul_lb = gtk_label_new (NULL);
  p->ul_lb = GTK_LABEL (w);
  gtk_label_set_single_line_mode (p->ul_lb, TRUE);
  gtk_box_pack_start (GTK_BOX (box_wrapper), w, TRUE, FALSE, 0);

  /* ratio */
  w = gtk_label_new (NULL);
  g_object_set (G_OBJECT(w), "margin-left", GUI_PAD_BIG, NULL);
  p->stats_lb = GTK_LABEL (w);
  gtk_label_set_single_line_mode (p->stats_lb, TRUE);
  gtk_box_pack_start (GTK_BOX (box_wrapper), w, TRUE, FALSE, 0);

  /* statistics button */
  w = gtk_menu_button_new ();
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (w), TRUE);
  img = gtk_image_new_from_icon_name ("statistics-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_image (GTK_BUTTON (w), img);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (w), get_statistics_menu_model());
  gtk_widget_set_tooltip_text (w, _("Statistics"));
  gtk_box_pack_end (GTK_BOX (box_wrapper), w, FALSE, FALSE, 0);
  
  return box_wrapper; 
}

gboolean
gtr_window_is_paused (TrCore *core)
{
  const size_t active_count = gtr_core_get_active_torrent_count (core)
             , total_count  = gtr_core_get_torrent_count (core);

  // printf("total_count = %d, active_count = %d\n", total_count, active_count);

  return (active_count < total_count) || (active_count == 0);
}

static void
load_css_from_resource (const gchar *resource_path)
{
  GError *error = NULL;
  GFile *css_file;
  GtkCssProvider *provider;

  css_file = g_file_new_for_uri (resource_path);

  if (!g_file_query_exists (css_file, NULL)) {
    g_object_unref (css_file);
    return;
  }

  provider = gtk_css_provider_new ();

  if (gtk_css_provider_load_from_file (provider, css_file, &error)) {
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);
  } else {
    g_warning ("Could not load css provider: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (css_file);
}


GtkWidget * 
gtr_window_new( GtkApplication * app, TrCore * core )
{
  PrivateData    * p;
  GtkWidget      * toolbar, *filter, *list, *status;
  GtkWidget      * vbox, *tbox, *w, *self;
  GtkWidget      * button;
  GtkWidget      * image;
  GtkWindow      * win;
  GMenuModel     * model;

  p = g_new0 (PrivateData, 1);

  /* make the window */
  self = gtk_application_window_new (app);
  g_object_set_qdata_full (G_OBJECT (self), private_data_quark (), p, privateFree);
  win = GTK_WINDOW (self);
  gtk_window_set_role (win, "tr-main");
  gtk_window_set_title (win, g_get_application_name ());
  gtk_widget_set_size_request (GTK_WIDGET (win), 620, 220);
  gtk_window_set_default_size (win,
                               gtr_pref_int_get (TR_KEY_main_window_width),
                               gtr_pref_int_get (TR_KEY_main_window_height));
  gtk_window_move (win, gtr_pref_int_get (TR_KEY_main_window_x),
                        gtr_pref_int_get (TR_KEY_main_window_y));
  if (gtr_pref_flag_get (TR_KEY_main_window_is_maximized))
    gtk_window_maximize (win);

  // load CSS theme
  load_css_from_resource ("resource://" TR_CSS_RESOURCE_PATH);

  // load icon theme
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    TR_ICON_RESOURCE_PATH);

  /* window's main container */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (self), vbox);

  /* toolbar */
  toolbar = gtk_header_bar_new();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (toolbar), TRUE);
  gtk_header_bar_set_title (GTK_HEADER_BAR (toolbar), g_get_application_name ());
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (toolbar), "All Torrents");
  gtk_window_set_titlebar (GTK_WINDOW (win), toolbar);

  /* selected torrent actions */

  tbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_style_context_add_class (gtk_widget_get_style_context (tbox),
                               GTK_STYLE_CLASS_RAISED);

  gtk_style_context_add_class (gtk_widget_get_style_context (tbox),
                               GTK_STYLE_CLASS_LINKED);

  button = gtk_button_new_from_icon_name (
    "media-playback-start-symbolic", GTK_ICON_SIZE_MENU);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.torrent-start");
  gtk_box_pack_start (GTK_BOX (tbox), button, TRUE, TRUE, FALSE);

  button = gtk_button_new_from_icon_name (
    "media-playback-pause-symbolic", GTK_ICON_SIZE_MENU);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.torrent-stop");
  gtk_box_pack_start (GTK_BOX (tbox), button, TRUE, TRUE, FALSE);

  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), tbox);

  button = gtk_button_new_from_icon_name (
    "user-trash-symbolic", GTK_ICON_SIZE_MENU);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.remove-torrent");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), button);

  /* Application menu */

  button = gtk_menu_button_new ();
  image = gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);
  model = gtr_action_get_menu_model ("main-window-popup");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), FALSE);

  /* Selected torrent details menu */

  button = gtk_menu_button_new ();
  image = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);
  model = gtr_action_get_menu_model ("torrent-options-popup");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), FALSE);

  /* Show/hide filters */

  button = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);
  g_signal_connect(button, "toggled", G_CALLBACK (onFilterChanged), p);

  /* Filter bar */

  w = filter = p->filter = gtr_filter_bar_new (gtr_core_session (core),
                                               gtr_core_model (core),
                                               &p->filter_model,
                                               GTK_HEADER_BAR (toolbar));
  gtk_container_set_border_width (GTK_CONTAINER (w), GUI_PAD_SMALL);

  /* Status Bar */

  status = p->status = gtr_status_bar_new (p); // gtk_grid_new ();

  /* Torrent list */

  p->view = makeview (p);
  w = list = p->scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w),
                                       GTK_SHADOW_OUT);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "tr-workarea");
  gtk_container_add (GTK_CONTAINER (w), p->view);

  /* lay out the widgets */
  gtk_box_pack_start (GTK_BOX (vbox), filter, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), list, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), status, FALSE, FALSE, 0);

  /* show the window */
  gtk_widget_show_all (GTK_WIDGET (win));

  /* listen for prefs changes that affect the window */
  p->core = core;
  prefsChanged (core, TR_KEY_compact_view, self);
  prefsChanged (core, TR_KEY_show_filterbar, self);
  prefsChanged (core, TR_KEY_show_statusbar, self);
  prefsChanged (core, TR_KEY_statusbar_stats, self);
  prefsChanged (core, TR_KEY_show_toolbar, self);
  prefsChanged (core, TR_KEY_alt_speed_enabled, self);
  
  prefsChanged (core, TR_KEY_speed_limit_down, self);
  prefsChanged (core, TR_KEY_speed_limit_down_enabled, self);

  prefsChanged (core, TR_KEY_speed_limit_up, self);
  prefsChanged (core, TR_KEY_speed_limit_up_enabled, self);
  
  prefsChanged (core, TR_KEY_ratio_limit, self);
  prefsChanged (core, TR_KEY_ratio_limit_enabled, self);

  p->pref_handler_id = g_signal_connect (core, "prefs-changed",
                                         G_CALLBACK (prefsChanged), self);

  tr_sessionSetAltSpeedFunc (gtr_core_session (core), onAltSpeedToggled, p);

  gtr_window_refresh (GTK_WINDOW(self));
  return self;
}

static void
updateStats (PrivateData * p)
{
  const char * pch;
  char up[32];
  char down[32];
  char ratio[32];
  char buf[512];
  struct tr_session_stats stats;
  tr_session * session = gtr_core_session (p->core);

  /* update the stats */
  pch = gtr_pref_string_get (TR_KEY_statusbar_stats);
  if (!g_strcmp0 (pch, "session-ratio"))
    {
      tr_sessionGetStats (session, &stats);
      tr_strlratio (ratio, stats.ratio, sizeof (ratio));
      g_snprintf (buf, sizeof (buf), _("Ratio: %s"), ratio);
    }
  else if (!g_strcmp0 (pch, "session-transfer"))
    {
      tr_sessionGetStats (session, &stats);
      tr_strlsize (up, stats.uploadedBytes, sizeof (up));
      tr_strlsize (down, stats.downloadedBytes, sizeof (down));
      /* Translators: "size|" is here for disambiguation. Please remove it from your translation.
         %1$s is the size of the data we've downloaded
         %2$s is the size of the data we've uploaded */
      g_snprintf (buf, sizeof (buf),
                  Q_("Down: %1$s, Up: %2$s"), down, up);
    }
  else if (!g_strcmp0 (pch, "total-transfer"))
    {
      tr_sessionGetCumulativeStats (session, &stats);
      tr_strlsize (up, stats.uploadedBytes, sizeof (up));
      tr_strlsize (down, stats.downloadedBytes, sizeof (down));
      /* Translators: "size|" is here for disambiguation. Please remove it from your translation.
         %1$s is the size of the data we've downloaded
         %2$s is the size of the data we've uploaded */
      g_snprintf (buf, sizeof (buf),
                  Q_( "size|Down: %1$s, Up: %2$s"), down, up);
    }
  else /* default is total-ratio */
    {
      tr_sessionGetCumulativeStats (session, &stats);
      tr_strlratio (ratio, stats.ratio, sizeof (ratio));
      g_snprintf (buf, sizeof (buf), _("Ratio: %s"), ratio);
    }

  gtr_label_set_text (p->stats_lb, buf);
}

static void
updateSpeeds (PrivateData * p)
{
  tr_session * session = gtr_core_session (p->core);

  if (session != NULL)
    {
      char text_str[256];
      char speed_str[128];
      double upSpeed = 0;
      double downSpeed = 0;
      int upCount = 0;
      int downCount = 0;
      GtkTreeIter iter;
      GtkTreeModel * model = gtr_core_model (p->core);


      if (gtk_tree_model_iter_nth_child (model, &iter, NULL, 0)) do
        {
          int uc, dc;
          double us, ds;
          gtk_tree_model_get (model, &iter, MC_SPEED_UP, &us,
                                            MC_SPEED_DOWN, &ds,
                                            MC_ACTIVE_PEERS_UP, &uc,
                                            MC_ACTIVE_PEERS_DOWN, &dc,
                                            -1);
          upSpeed += us;
          upCount += uc;
          downSpeed += ds;
          downCount += dc;
        }
      while (gtk_tree_model_iter_next (model, &iter));

      tr_formatter_speed_KBps (speed_str, downSpeed, sizeof (speed_str));
      g_snprintf (text_str, sizeof(text_str), "%s %s", speed_str, gtr_get_unicode_string (GTR_UNICODE_DOWN));
      gtr_label_set_text (p->dl_lb, text_str);
      gtk_widget_set_visible (GTK_WIDGET (p->dl_lb), (downCount>0));

      tr_formatter_speed_KBps (speed_str, upSpeed, sizeof (speed_str));
      g_snprintf (text_str, sizeof(text_str), "%s %s", speed_str, gtr_get_unicode_string (GTR_UNICODE_UP));
      gtr_label_set_text (p->ul_lb, text_str);
      gtk_widget_set_visible (GTK_WIDGET (p->ul_lb), ((downCount>0) || (upCount>0)));
    }
}

void
gtr_window_refresh (GtkWindow * self)
{
  PrivateData * p = get_private_data (self);

  if (p && p->core && gtr_core_session (p->core))
    {
      updateSpeeds (p);
      updateStats (p);
    }
}

GtkTreeSelection*
gtr_window_get_selection (GtkWindow * w)
{
    return get_private_data (w)->selection;
}

void
gtr_window_set_busy (GtkWindow * win, gboolean isBusy)
{
  GtkWidget * w = GTK_WIDGET (win);

  if (w && gtk_widget_get_realized (w))
    {
      GdkDisplay * display = gtk_widget_get_display (w);
      GdkCursor * cursor = isBusy ? gdk_cursor_new_for_display (display, GDK_WATCH) : NULL;

      gdk_window_set_cursor (gtk_widget_get_window (w), cursor);
      gdk_display_flush (display);

      g_clear_object (&cursor);
    }
}
