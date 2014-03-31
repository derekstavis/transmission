/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: options.h 14002 2013-02-09 23:11:17Z jordan $
 */

#ifndef OPTIONS_DIALOG_H
#define OPTIONS_DIALOG_H

#include <iostream>

#include <QDialog>
#include <QEvent>
#include <QString>
#include <QDir>
#include <QVector>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <QFile>
#include <QTimer>
#include <QLineEdit>

#include "add-data.h" // AddData
#include "file-tree.h" // FileList

class QCheckBox;
class QComboBox;
class QPushButton;

class FileTreeView;
class FreespaceLabel;
class Prefs;
class Session;

extern "C" { struct tr_variant; };

class FileAdded: public QObject
{
    Q_OBJECT

  public:
    FileAdded (int tag, const QString& name): myTag (tag), myName (name) { }
    ~FileAdded () { }
    void setFileToDelete (const QString& file) { myDelFile = file; }

  public slots:
    void executed (int64_t tag, const QString& result, struct tr_variant * arguments);

  private:
    const int64_t myTag;
    QString myName;
    QString myDelFile;
};

class Options: public QDialog
{
    Q_OBJECT

  public:
    Options (Session& session, const Prefs& prefs, const AddData& addme, QWidget * parent = 0);
    ~Options ();

  private:
    void reload ();
    void clearInfo ();
    void refreshSource (int width=-1);
    void refreshDestinationButton (int width=-1);
    void refreshButton (QPushButton *, const QString&, int width=-1);
    bool eventFilter (QObject *, QEvent *);

  private slots:
    void onAccepted ();
    void onPriorityChanged (const QSet<int>& fileIndices, int);
    void onWantedChanged (const QSet<int>& fileIndices, bool);
    void onVerify ();
    void onTimeout ();
    void onFilenameClicked ();
    void onDestinationClicked ();
    void onFilesSelected (const QStringList&);
    void onSourceEditingFinished ();
    void onDestinationsSelected (const QStringList&);
    void onDestinationEdited (const QString&);
    void onDestinationEditedIdle ();

  private:
    Session& mySession;
    AddData myAdd;
    QDir myLocalDestination;
    bool myHaveInfo;
    tr_info myInfo;
    FileTreeView * myTree;
    FreespaceLabel * myFreespaceLabel;
    QCheckBox * myStartCheck;
    QCheckBox * myTrashCheck;
    QComboBox * myPriorityCombo;
    QPushButton * mySourceButton;
    QLineEdit * mySourceEdit;
    QPushButton * myDestinationButton;
    QLineEdit * myDestinationEdit;
    QPushButton * myVerifyButton;
    QVector<int> myPriorities;
    QVector<bool> myWanted;
    FileList myFiles;

  private:
    QTimer myVerifyTimer;
    char myVerifyBuf[2048*4];
    QFile myVerifyFile;
    uint64_t myVerifyFilePos;
    int myVerifyFileIndex;
    uint32_t myVerifyPieceIndex;
    uint32_t myVerifyPiecePos;
    void clearVerify ();
    QVector<bool> myVerifyFlags;
    QCryptographicHash myVerifyHash;
    typedef QMap<uint32_t,int32_t> mybins_t;
    mybins_t myVerifyBins;
    QTimer myEditTimer;
};

#endif
