/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: open-dialog.h 13625 2012-12-05 17:29:46Z jordan $
 */

#ifndef GTR_OPEN_DIALOG_H
#define GTR_OPEN_DIALOG_H

#include <gtk/gtk.h>
#include "tr-core.h"

GtkWidget* gtr_torrent_open_from_url_dialog_new (GtkWindow * parent, TrCore * core);
GtkWidget* gtr_torrent_open_from_file_dialog_new (GtkWindow * parent, TrCore * core);

/* This dialog assumes ownership of the ctor */
GtkWidget* gtr_torrent_options_dialog_new (GtkWindow * parent, TrCore * core, tr_ctor * ctor);

#endif /* GTR_ADD_DIALOG */
