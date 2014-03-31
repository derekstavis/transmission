/*
 * This file Copyright(C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: formatter.h 13996 2013-02-09 18:59:05Z jordan $
 */

#ifndef QTR_UNITS
#define QTR_UNITS

#include <inttypes.h> /* uint64_t */

#include <QString>
#include <QObject>
#include <QIcon>

class Speed;

class Formatter: public QObject
{
    Q_OBJECT

  public:

    Formatter() {}
    virtual ~Formatter() {}

  public:

    static QString memToString (int64_t bytes);
    static QString sizeToString (int64_t bytes);
    static QString speedToString (const Speed& speed);
    static QString percentToString (double x);
    static QString ratioToString (double ratio);
    static QString timeToString (int seconds);
    static QString uploadSpeedToString(const Speed& up);
    static QString downloadSpeedToString(const Speed& down);

  public:

    typedef enum { B, KB, MB, GB, TB } Size;
    typedef enum { SPEED, SIZE, MEM } Type;
    static QString unitStr (Type t, Size s) { return unitStrings[t][s]; }
    static void initUnits ();

  private:

    static QString unitStrings[3][5];
};

#endif
