/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: options.cc 14150 2013-07-27 21:58:14Z jordan $
 */

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> /* mime64 */
#include <libtransmission/variant.h>

#include "add-data.h"
#include "file-tree.h"
#include "freespace-label.h"
#include "hig.h"
#include "options.h"
#include "prefs.h"
#include "session.h"
#include "torrent.h"
#include "utils.h"

/***
****
***/

void
FileAdded :: executed (int64_t tag, const QString& result, struct tr_variant * arguments)
{
  Q_UNUSED (arguments);

  if (tag != myTag)
    return;

  if ( (result == "success") && !myDelFile.isEmpty ())
    {
      QFile file (myDelFile);
      file.setPermissions (QFile::ReadOwner | QFile::WriteOwner);
      file.remove ();
    }

  if (result != "success")
    {
      QString text = result;

      for (int i=0, n=text.size (); i<n; ++i)
        if (!i || text[i-1].isSpace ())
          text[i] = text[i].toUpper ();

      QMessageBox::warning (QApplication::activeWindow (),
                            tr ("Error Adding Torrent"),
                            QString ("<p><b>%1</b></p><p>%2</p>").arg (text).arg (myName));
    }

  deleteLater ();
}

/***
****
***/

Options :: Options (Session& session, const Prefs& prefs, const AddData& addme, QWidget * parent):
  QDialog (parent, Qt::Dialog),
  mySession (session),
  myAdd (addme),
  myHaveInfo (false),
  mySourceButton (0),
  mySourceEdit (0),
  myDestinationButton (0),
  myDestinationEdit (0),
  myVerifyButton (0),
  myVerifyFile (0),
  myVerifyHash (QCryptographicHash::Sha1),
  myEditTimer (this)
{
  QFontMetrics fontMetrics (font ());
  QGridLayout * layout = new QGridLayout (this);
  int row = 0;

  QString title;
  if (myAdd.type == AddData::FILENAME)
    title = tr ("Open Torrent from File");
  else
    title = tr ("Open Torrent from URL or Magnet Link");
  setWindowTitle (title);

  myEditTimer.setInterval (2000);
  myEditTimer.setSingleShot (true);
  connect (&myEditTimer, SIGNAL (timeout ()), this, SLOT (onDestinationEditedIdle ()));

  const int iconSize (style ()->pixelMetric (QStyle :: PM_SmallIconSize));
  QIcon fileIcon = style ()->standardIcon (QStyle::SP_FileIcon);
  const QPixmap filePixmap = fileIcon.pixmap (iconSize);

  QLabel * l = new QLabel (tr ("&Source:"));
  layout->addWidget (l, row, 0, Qt::AlignLeft);

  QWidget * w;
  QPushButton * p;

  if (myAdd.type == AddData::FILENAME)
    {
      p = mySourceButton =  new QPushButton;
      p->setIcon (filePixmap);
      p->setStyleSheet (QString::fromUtf8 ("text-align: left; padding-left: 5; padding-right: 5"));
      p->installEventFilter (this);
      w = p;
      connect (p, SIGNAL (clicked (bool)), this, SLOT (onFilenameClicked ()));
    }
  else
    {
      QLineEdit * e = mySourceEdit = new QLineEdit;
      e->setText (myAdd.readableName());
      e->setCursorPosition (0);
      e->selectAll ();
      w = e;
      connect (e, SIGNAL(editingFinished()), this, SLOT(onSourceEditingFinished()));
    }

  const int width = fontMetrics.size (0, QString::fromUtf8 ("This is a pretty long torrent filename indeed.torrent")).width ();
  w->setMinimumWidth (width);
  layout->addWidget (w, row, 1);
  l->setBuddy (w);

  l = new QLabel (tr ("&Destination folder:"));
  layout->addWidget (l, ++row, 0, Qt::AlignLeft);
  const QString downloadDir (prefs.getString (Prefs::DOWNLOAD_DIR));
  myFreespaceLabel = new FreespaceLabel (mySession, downloadDir, this);

  if (session.isLocal ())
    {
      const QFileIconProvider iconProvider;
      const QIcon folderIcon = iconProvider.icon (QFileIconProvider::Folder);
      const QPixmap folderPixmap = folderIcon.pixmap (iconSize);

      myLocalDestination.setPath (downloadDir);
      p = myDestinationButton = new QPushButton;
      p->setIcon (folderPixmap);
      p->setStyleSheet ("text-align: left; padding-left: 5; padding-right: 5");
      p->installEventFilter (this);
      layout->addWidget (p, row, 1);
      l->setBuddy (p);
      connect (p, SIGNAL (clicked (bool)), this, SLOT (onDestinationClicked ()));
    }
  else
    {
      QLineEdit * e = myDestinationEdit = new QLineEdit;
      e->setText (downloadDir);
      layout->addWidget (e, row, 1);
      l->setBuddy (e);
      connect (e, SIGNAL (textEdited (const QString&)), this, SLOT (onDestinationEdited (const QString&)));
    }

  l = myFreespaceLabel;
  layout->addWidget (l, ++row, 0, 1, 2, Qt::Alignment (Qt::AlignRight | Qt::AlignTop));
  layout->setRowMinimumHeight (row, l->height () + HIG::PAD_SMALL);

  myTree = new FileTreeView (0, false);
  layout->addWidget (myTree, ++row, 0, 1, 2);
  if (!session.isLocal ())
    myTree->hideColumn (2); // hide the % done, since we've no way of knowing

  QComboBox * m = new QComboBox;
  m->addItem (tr ("High"),   TR_PRI_HIGH);
  m->addItem (tr ("Normal"), TR_PRI_NORMAL);
  m->addItem (tr ("Low"),    TR_PRI_LOW);
  m->setCurrentIndex (1); // Normal
  myPriorityCombo = m;
  l = new QLabel (tr ("&Priority:"));
  l->setBuddy (m);
  layout->addWidget (l, ++row, 0, Qt::AlignLeft);
  layout->addWidget (m, row, 1);

  if (session.isLocal ())
    {
      p = myVerifyButton = new QPushButton (tr ("&Verify Local Data"));
      layout->addWidget (p, ++row, 0, Qt::AlignLeft);
    }

  QCheckBox * c;
  c = myStartCheck = new QCheckBox (tr ("S&tart when added"));
  c->setChecked (prefs.getBool (Prefs :: START));
  layout->addWidget (c, ++row, 0, 1, 2, Qt::AlignLeft);

  c = myTrashCheck = new QCheckBox (tr ("Mo&ve .torrent file to the trash"));
  c->setChecked (prefs.getBool (Prefs :: TRASH_ORIGINAL));
  layout->addWidget (c, ++row, 0, 1, 2, Qt::AlignLeft);

  QDialogButtonBox * b = new QDialogButtonBox (QDialogButtonBox::Open|QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect (b, SIGNAL (rejected ()), this, SLOT (deleteLater ()));
  connect (b, SIGNAL (accepted ()), this, SLOT (onAccepted ()));
  layout->addWidget (b, ++row, 0, 1, 2);

  layout->setRowStretch (3, 2);
  layout->setColumnStretch (1, 2);
  layout->setSpacing (HIG :: PAD);

  connect (myTree, SIGNAL (priorityChanged (const QSet<int>&,int)), this, SLOT (onPriorityChanged (const QSet<int>&,int)));
  connect (myTree, SIGNAL (wantedChanged (const QSet<int>&,bool)), this, SLOT (onWantedChanged (const QSet<int>&,bool)));
  if (session.isLocal ())
    connect (myVerifyButton, SIGNAL (clicked (bool)), this, SLOT (onVerify ()));

  connect (&myVerifyTimer, SIGNAL (timeout ()), this, SLOT (onTimeout ()));

  reload ();
}

Options :: ~Options ()
{
  clearInfo ();
}

/***
****
***/

void
Options :: refreshButton (QPushButton * p, const QString& text, int width)
{
  if (width <= 0)
    width = p->width ();
  width -= 15;
  QFontMetrics fontMetrics (font ());
  QString str = fontMetrics.elidedText (text, Qt::ElideRight, width);
  p->setText (str);
}

void
Options :: refreshSource (int width)
{
  QString text = myAdd.readableName ();

  if (mySourceButton)
    refreshButton (mySourceButton, text, width);

  if (mySourceEdit)
    mySourceEdit->setText (text);
}

void
Options :: refreshDestinationButton (int width)
{
  if (myDestinationButton != 0)
    refreshButton (myDestinationButton, myLocalDestination.absolutePath (), width);
}


bool
Options :: eventFilter (QObject * o, QEvent * event)
{
  if (event->type() == QEvent::Resize)
    {
      if (o == mySourceButton)
        refreshSource (dynamic_cast<QResizeEvent*> (event)->size ().width ());

      else if (o == myDestinationButton)
        refreshDestinationButton (dynamic_cast<QResizeEvent*> (event)->size ().width ());
    }

  return false;
}

/***
****
***/

void
Options :: clearInfo ()
{
  if (myHaveInfo)
    tr_metainfoFree (&myInfo);

  myHaveInfo = false;
  myFiles.clear ();
}

void
Options :: reload ()
{
  clearInfo ();
  clearVerify ();

  tr_ctor * ctor = tr_ctorNew (0);

  switch (myAdd.type)
    {
      case AddData::MAGNET:
        tr_ctorSetMetainfoFromMagnetLink (ctor, myAdd.magnet.toUtf8 ().constData ());
        break;

      case AddData::FILENAME:
        tr_ctorSetMetainfoFromFile (ctor, myAdd.filename.toUtf8 ().constData ());
        break;

      case AddData::METAINFO:
        tr_ctorSetMetainfo (ctor, (const uint8_t*)myAdd.metainfo.constData (), myAdd.metainfo.size ());
        break;

      default:
        break;
    }

  const int err = tr_torrentParse (ctor, &myInfo);
  myHaveInfo = !err;
  tr_ctorFree (ctor);

  myTree->clear ();
  myTree->setVisible (myHaveInfo && (myInfo.fileCount>0));
  myFiles.clear ();
  myPriorities.clear ();
  myWanted.clear ();

  if (myVerifyButton)
    myVerifyButton->setVisible (myHaveInfo && (myInfo.fileCount>0));

  if (myHaveInfo)
    {
      myPriorities.insert (0, myInfo.fileCount, TR_PRI_NORMAL);
      myWanted.insert (0, myInfo.fileCount, true);

      for (tr_file_index_t i=0; i<myInfo.fileCount; ++i)
        {
          TrFile file;
          file.index = i;
          file.priority = myPriorities[i];
          file.wanted = myWanted[i];
          file.size = myInfo.files[i].length;
          file.have = 0;
          file.filename = QString::fromUtf8 (myInfo.files[i].name);
          myFiles.append (file);
        }
    }

  myTree->update (myFiles);
}

void
Options :: onPriorityChanged (const QSet<int>& fileIndices, int priority)
{
  foreach (int i, fileIndices)
    myPriorities[i] = priority;
}

void
Options :: onWantedChanged (const QSet<int>& fileIndices, bool isWanted)
{
  foreach (int i, fileIndices)
    myWanted[i] = isWanted;
}

void
Options :: onAccepted ()
{
  // rpc spec section 3.4 "adding a torrent"

  const int64_t tag = mySession.getUniqueTag ();
  tr_variant top;
  tr_variantInitDict (&top, 3);
  tr_variantDictAddStr (&top, TR_KEY_method, "torrent-add");
  tr_variantDictAddInt (&top, TR_KEY_tag, tag);
  tr_variant * args (tr_variantDictAddDict (&top, TR_KEY_arguments, 10));
  QString downloadDir;

  // "download-dir"
  if (myDestinationButton)
    downloadDir = myLocalDestination.absolutePath ();
  else
    downloadDir = myDestinationEdit->text ();

  tr_variantDictAddStr (args, TR_KEY_download_dir, downloadDir.toUtf8 ().constData ());

  // "metainfo"
  switch (myAdd.type)
    {
      case AddData::MAGNET:
        tr_variantDictAddStr (args, TR_KEY_filename, myAdd.magnet.toUtf8 ().constData ());
        break;

      case AddData::URL:
        tr_variantDictAddStr (args, TR_KEY_filename, myAdd.url.toString ().toUtf8 ().constData ());
        break;

      case AddData::FILENAME:
      case AddData::METAINFO: {
        const QByteArray b64 = myAdd.toBase64 ();
        tr_variantDictAddRaw (args, TR_KEY_metainfo, b64.constData (), b64.size ());
        break;
      }

      default:
        qWarning ("unhandled AddData.type: %d", myAdd.type);
    }

  // paused
  tr_variantDictAddBool (args, TR_KEY_paused, !myStartCheck->isChecked ());

  // priority
  const int index = myPriorityCombo->currentIndex ();
  const int priority = myPriorityCombo->itemData (index).toInt ();
  tr_variantDictAddInt (args, TR_KEY_bandwidthPriority, priority);

  // files-unwanted
  int count = myWanted.count (false);
  if (count > 0)
    {
      tr_variant * l = tr_variantDictAddList (args, TR_KEY_files_unwanted, count);
      for (int i=0, n=myWanted.size (); i<n; ++i)
        if (myWanted.at (i) == false)
          tr_variantListAddInt (l, i);
    }

  // priority-low
  count = myPriorities.count (TR_PRI_LOW);
  if (count > 0)
    {
      tr_variant * l = tr_variantDictAddList (args, TR_KEY_priority_low, count);
      for (int i=0, n=myPriorities.size (); i<n; ++i)
        if (myPriorities.at (i) == TR_PRI_LOW)
          tr_variantListAddInt (l, i);
    }

  // priority-high
  count = myPriorities.count (TR_PRI_HIGH);
  if (count > 0)
    {
      tr_variant * l = tr_variantDictAddList (args, TR_KEY_priority_high, count);
      for (int i=0, n=myPriorities.size (); i<n; ++i)
        if (myPriorities.at (i) == TR_PRI_HIGH)
          tr_variantListAddInt (l, i);
    }

  // maybe delete the source .torrent
  FileAdded * fileAdded = new FileAdded (tag, myAdd.readableName ());
  if (myTrashCheck->isChecked () && (myAdd.type==AddData::FILENAME))
    fileAdded->setFileToDelete (myAdd.filename);
  connect (&mySession, SIGNAL (executed (int64_t,const QString&, struct tr_variant*)),
           fileAdded, SLOT (executed (int64_t,const QString&, struct tr_variant*)));

  mySession.exec (&top);

  tr_variantFree (&top);
  deleteLater ();
}

void
Options :: onFilenameClicked ()
{
  if (myAdd.type == AddData::FILENAME)
    {
      QFileDialog * d = new QFileDialog (this,
                                         tr ("Open Torrent"),
                                         QFileInfo (myAdd.filename).absolutePath (),
                                         tr ("Torrent Files (*.torrent);;All Files (*.*)"));
      d->setFileMode (QFileDialog::ExistingFile);
      d->setAttribute (Qt::WA_DeleteOnClose);
      connect (d, SIGNAL (filesSelected (const QStringList&)), this, SLOT (onFilesSelected (const QStringList&)));
      d->show ();
    }
}

void
Options :: onFilesSelected (const QStringList& files)
{
  if (files.size () == 1)
    {
      myAdd.set (files.at (0));
      refreshSource ();
      reload ();
    }
}

void
Options :: onSourceEditingFinished ()
{
  myAdd.set (mySourceEdit->text());
}

void
Options :: onDestinationClicked ()
{
  QFileDialog * d = new QFileDialog (this, tr ("Select Destination"), myLocalDestination.absolutePath ());
  d->setFileMode (QFileDialog::Directory);
  d->setAttribute (Qt::WA_DeleteOnClose);
  connect (d, SIGNAL (filesSelected (const QStringList&)), this, SLOT (onDestinationsSelected (const QStringList&)));
  d->show ();
}

void
Options :: onDestinationsSelected (const QStringList& destinations)
{
  if (destinations.size () == 1)
    {
      const QString& destination (destinations.first ());
      myFreespaceLabel->setPath (destination);
      myLocalDestination.setPath (destination);
      refreshDestinationButton ();
    }
}

void
Options :: onDestinationEdited (const QString& text)
{
  Q_UNUSED (text);

  myEditTimer.start ();
}

void
Options :: onDestinationEditedIdle ()
{
  myFreespaceLabel->setPath (myDestinationEdit->text());
}

/***
****
****  VERIFY
****
***/

void
Options :: clearVerify ()
{
  myVerifyHash.reset ();
  myVerifyFile.close ();
  myVerifyFilePos = 0;
  myVerifyFlags.clear ();
  myVerifyFileIndex = 0;
  myVerifyPieceIndex = 0;
  myVerifyPiecePos = 0;
  myVerifyTimer.stop ();

  for (int i=0, n=myFiles.size (); i<n; ++i)
    myFiles[i].have = 0;

  myTree->update (myFiles);
}

void
Options :: onVerify ()
{
  clearVerify ();
  myVerifyFlags.insert (0, myInfo.pieceCount, false);
  myVerifyTimer.setSingleShot (false);
  myVerifyTimer.start (0);
}

namespace
{
  uint64_t getPieceSize (const tr_info * info, tr_piece_index_t pieceIndex)
  {
    if (pieceIndex != info->pieceCount - 1)
      return info->pieceSize;
    return info->totalSize % info->pieceSize;
  }
}

void
Options :: onTimeout ()
{
  if (myFiles.isEmpty())
    {
      myVerifyTimer.stop ();
      return;
    }

  const tr_file * file = &myInfo.files[myVerifyFileIndex];

  if (!myVerifyFilePos && !myVerifyFile.isOpen ())
    {
      const QFileInfo fileInfo (myLocalDestination, QString::fromUtf8 (file->name));
      myVerifyFile.setFileName (fileInfo.absoluteFilePath ());
      myVerifyFile.open (QIODevice::ReadOnly);
    }

  int64_t leftInPiece = getPieceSize (&myInfo, myVerifyPieceIndex) - myVerifyPiecePos;
  int64_t leftInFile = file->length - myVerifyFilePos;
  int64_t bytesThisPass = std::min (leftInFile, leftInPiece);
  bytesThisPass = std::min (bytesThisPass, (int64_t)sizeof (myVerifyBuf));

  if (myVerifyFile.isOpen () && myVerifyFile.seek (myVerifyFilePos))
    {
      int64_t numRead = myVerifyFile.read (myVerifyBuf, bytesThisPass);
      if (numRead == bytesThisPass)
        myVerifyHash.addData (myVerifyBuf, numRead);
    }

  leftInPiece -= bytesThisPass;
  leftInFile -= bytesThisPass;
  myVerifyPiecePos += bytesThisPass;
  myVerifyFilePos += bytesThisPass;

  myVerifyBins[myVerifyFileIndex] += bytesThisPass;

  if (leftInPiece == 0)
    {
      const QByteArray result (myVerifyHash.result ());
      const bool matches = !memcmp (result.constData (),
                                    myInfo.pieces[myVerifyPieceIndex].hash,
                                    SHA_DIGEST_LENGTH);
      myVerifyFlags[myVerifyPieceIndex] = matches;
      myVerifyPiecePos = 0;
      ++myVerifyPieceIndex;
      myVerifyHash.reset ();

      FileList changedFiles;
      if (matches)
        {
          mybins_t::const_iterator i;
          for (i=myVerifyBins.begin (); i!=myVerifyBins.end (); ++i)
            {
              TrFile& f (myFiles[i.key ()]);
              f.have += i.value ();
              changedFiles.append (f);
            }
        }
      myTree->update (changedFiles);
      myVerifyBins.clear ();
    }

  if (leftInFile == 0)
    {
      myVerifyFile.close ();
      ++myVerifyFileIndex;
      myVerifyFilePos = 0;
    }

  bool done = myVerifyPieceIndex >= myInfo.pieceCount;
  if (done)
    {
      uint64_t have = 0;
      foreach (const TrFile& f, myFiles)
        have += f.have;

      if (!have) // everything failed
        {
          // did the user accidentally specify the child directory instead of the parent?
          const QStringList tokens = QString (file->name).split ('/');
          if (!tokens.empty () && myLocalDestination.dirName ()==tokens.at (0))
            {
              // move up one directory and try again
              myLocalDestination.cdUp ();
              refreshDestinationButton (-1);
              onVerify ();
              done = false;
            }
        }
    }

  if (done)
    myVerifyTimer.stop ();
}
