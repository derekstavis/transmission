/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: hig.h 11092 2010-08-01 20:36:13Z charles $
 */

#ifndef QTR_HIG_H
#define QTR_HIG_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QString;
class QGridLayout;
class QLayout;

class HIG: public QWidget
{
        Q_OBJECT

    public:
        enum {
            PAD_SMALL = 3,
            PAD = 6,
            PAD_BIG = 12,
            PAD_LARGE = PAD_BIG
        };

    public:
        HIG( QWidget * parent = 0 );
        virtual ~HIG( );

    public:
        void addSectionDivider( );
        void addSectionTitle( const QString& );
        void addSectionTitle( QWidget* );
        void addSectionTitle( QLayout* );
        void addWideControl( QLayout * );
        void addWideControl( QWidget * );
        QCheckBox* addWideCheckBox( const QString&, bool isChecked );
        QLabel* addLabel( const QString& );
        QLabel* addTallLabel( const QString& );
        void addLabel( QWidget * );
        void addTallLabel( QWidget * );
        void addControl( QWidget * );
        void addControl( QLayout * );
        QLabel* addRow( const QString & label, QWidget * control, QWidget * buddy=0 );
        QLabel* addRow( const QString & label, QLayout * control, QWidget * buddy );
        void addRow( QWidget * label, QWidget * control, QWidget * buddy=0 );
        void addRow( QWidget * label, QLayout * control, QWidget * buddy );
        QLabel* addTallRow( const QString & label, QWidget * control, QWidget * buddy=0 );
        void finish( );

    private:
        QLayout* addRow( QWidget* w );

    private:
        int myRow;
        bool myHasTall;
        QGridLayout * myGrid;
};

#endif // QTR_HIG_H
