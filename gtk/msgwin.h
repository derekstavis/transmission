/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This file is licensed by the GPL version 2. Works owned by the
 * Transmission project are granted a special exemption to clause 2 (b)
 * so that the bulk of its code can remain under the MIT license.
 * This exemption does not extend to derived works not owned by
 * the Transmission project.
 *
 * $Id: msgwin.h 13625 2012-12-05 17:29:46Z jordan $
 */

#ifndef GTR_MSGWIN_H
#define GTR_MSGWIN_H

#include "tr-core.h"

GtkWidget * gtr_message_log_window_new (GtkWindow * parent, TrCore * core);

#endif
