/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: filter.h 13625 2012-12-05 17:29:46Z jordan $
 */

#ifndef GTR_FILTER_H
#define GTR_FILTER_H

#include <gtk/gtk.h>
#include <libtransmission/transmission.h>

GtkWidget * gtr_filter_bar_new (tr_session     * session,
                                GtkTreeModel   * torrent_model,
                                GtkTreeModel  ** filter_model);

#endif
