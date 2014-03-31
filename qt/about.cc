/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: about.cc 14150 2013-07-27 21:58:14Z jordan $
 */

#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <libtransmission/transmission.h>
#include <libtransmission/version.h>

#include "about.h"
#include "hig.h"
#include "license.h"

AboutDialog :: AboutDialog (QWidget * parent):
  QDialog (parent, Qt::Dialog),
  myLicenseDialog (new LicenseDialog (this))
{
  setWindowTitle (tr ("About Transmission"));
  QLabel * l;
  QVBoxLayout * v = new QVBoxLayout (this);

  l = new QLabel;
  l->setPixmap (QPixmap (QString::fromUtf8 (":/icons/transmission-48.png")));
  l->setAlignment (Qt::AlignCenter);
  v->addWidget (l);

  QFont f (font ());
  f.setWeight (QFont::Bold);
  f.setPointSize (int (f.pointSize () * 1.2));
  l = new QLabel (tr ("<big>Transmission %1</big>").arg (QString::fromUtf8 (LONG_VERSION_STRING)));
  l->setAlignment (Qt::AlignCenter);
  l->setFont (f);
  l->setMargin (8);
  v->addWidget (l);

  l = new QLabel (tr ("A fast and easy BitTorrent client"));
  l->setStyleSheet (QString::fromUtf8 ("text-align: center"));
  l->setAlignment (Qt::AlignCenter);
  v->addWidget (l);

  l = new QLabel (tr ("Copyright (c) The Transmission Project"));
  l->setAlignment (Qt::AlignCenter);
  v->addWidget (l);

  l = new QLabel (QString::fromUtf8 ("<a href=\"http://www.transmissionbt.com/\">http://www.transmissionbt.com/</a>"));
  l->setOpenExternalLinks (true);
  l->setAlignment (Qt::AlignCenter);
  v->addWidget (l);

  v->addSpacing (HIG::PAD_BIG);

  QPushButton * b;
  QDialogButtonBox * box = new QDialogButtonBox;

  b = new QPushButton (tr ("C&redits"), this);
  box->addButton (b, QDialogButtonBox::ActionRole);
  connect (b, SIGNAL (clicked ()), this, SLOT (showCredits ()));

  b = new QPushButton (tr ("&License"), this);
  box->addButton (b, QDialogButtonBox::ActionRole);
  connect (b, SIGNAL (clicked ()), myLicenseDialog, SLOT (show ()));

  box->addButton (QDialogButtonBox::Close);
  box->setCenterButtons (true);
  v->addWidget (box);
  connect (box, SIGNAL (rejected ()), this, SLOT (hide ()));
}

void
AboutDialog :: showCredits ()
{
  QMessageBox::about (
    this,
    tr ("Credits"),
    QString::fromUtf8 ("Jordan Lee (Backend; Daemon; GTK+; Qt)\n"
                        "Michell Livingston (OS X)\n"));
}

