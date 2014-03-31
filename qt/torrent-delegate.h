/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: torrent-delegate.h 14019 2013-02-14 23:32:37Z jordan $
 */

#ifndef QTR_TORRENT_DELEGATE_H
#define QTR_TORRENT_DELEGATE_H

#include <QStyledItemDelegate>
#include <QSize>

class QStyleOptionProgressBar;
class QStyleOptionViewItem;
class QStyle;
class Session;
class Torrent;

class TorrentDelegate: public QStyledItemDelegate
{
        Q_OBJECT

    public:
      static QColor blueBrush, greenBrush, silverBrush;
      static QColor blueBack,  greenBack, silverBack;

    protected:
        QStyleOptionProgressBar * myProgressBarStyle;

    protected:
        QString statusString( const Torrent& tor ) const;
        QString progressString( const Torrent& tor ) const;
        QString shortStatusString( const Torrent& tor ) const;
        QString shortTransferString( const Torrent& tor ) const;

    protected:
        QSize margin( const QStyle& style ) const;
        virtual QSize sizeHint( const QStyleOptionViewItem&, const Torrent& ) const;
        virtual void setProgressBarPercentDone( const QStyleOptionViewItem& option, const Torrent& ) const;
        virtual void drawTorrent( QPainter* painter, const QStyleOptionViewItem& option, const Torrent& ) const;

    public:
        explicit TorrentDelegate( QObject * parent=0 );
        virtual ~TorrentDelegate( );

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

};

#endif
