/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: utils.cc 13730 2012-12-31 22:43:29Z jordan $
 */

#include <iostream>

#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QObject>
#include <QSet>
#include <QStyle>

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> // tr_formatter

#include "utils.h"

/***
****
***/

QString
Utils :: remoteFileChooser( QWidget * parent, const QString& title, const QString& myPath, bool dir, bool local )
{
    QString path;

    if( local )
    {
        if( dir )
            path = QFileDialog::getExistingDirectory( parent, title, myPath );
        else
            path = QFileDialog::getOpenFileName( parent, title, myPath );
    }
    else
        path = QInputDialog::getText( parent, title, tr( "Enter a location:" ), QLineEdit::Normal, myPath, NULL );

    return path;
}

void
Utils :: toStderr( const QString& str )
{
    std::cerr << qPrintable(str) << std::endl;
}

const QIcon&
Utils :: guessMimeIcon( const QString& filename )
{
    enum { DISK, DOCUMENT, PICTURE, VIDEO, ARCHIVE, AUDIO, APP, TYPE_COUNT };
    static QIcon fallback;
    static QIcon fileIcons[TYPE_COUNT];
    static QSet<QString> suffixes[TYPE_COUNT];

    if( fileIcons[0].isNull( ) )
    {
        fallback = QApplication::style()->standardIcon( QStyle :: SP_FileIcon );

        suffixes[DISK] << QString::fromLatin1("iso");
        fileIcons[DISK]= QIcon::fromTheme( QString::fromLatin1("media-optical"), fallback );

        const char * doc_types[] = {
            "abw", "csv", "doc", "dvi", "htm", "html", "ini", "log", "odp",
            "ods", "odt", "pdf", "ppt", "ps",  "rtf", "tex", "txt", "xml" };
        for( int i=0, n=sizeof(doc_types)/sizeof(doc_types[0]); i<n; ++i )
            suffixes[DOCUMENT] << QString::fromLatin1(doc_types[i] );
        fileIcons[DOCUMENT] = QIcon::fromTheme( QString::fromLatin1("text-x-generic"), fallback );

        const char * pic_types[] = {
            "bmp", "gif", "jpg", "jpeg", "pcx", "png", "psd", "ras", "tga", "tiff" };
        for( int i=0, n=sizeof(pic_types)/sizeof(pic_types[0]); i<n; ++i )
            suffixes[PICTURE] << QString::fromLatin1(pic_types[i]);
        fileIcons[PICTURE]  = QIcon::fromTheme( QString::fromLatin1("image-x-generic"), fallback );

        const char * vid_types[] = {
            "3gp", "asf", "avi", "mkv", "mov", "mpeg", "mpg", "mp4",
            "ogm", "ogv", "qt", "rm", "wmv" };
        for( int i=0, n=sizeof(vid_types)/sizeof(vid_types[0]); i<n; ++i )
            suffixes[VIDEO] << QString::fromLatin1(vid_types[i]);
        fileIcons[VIDEO] = QIcon::fromTheme( QString::fromLatin1("video-x-generic"), fallback );

        const char * arc_types[] = {
            "7z", "ace", "bz2", "cbz", "gz", "gzip", "lzma", "rar", "sft", "tar", "zip" };
        for( int i=0, n=sizeof(arc_types)/sizeof(arc_types[0]); i<n; ++i )
            suffixes[ARCHIVE] << QString::fromLatin1(arc_types[i]);
        fileIcons[ARCHIVE]  = QIcon::fromTheme( QString::fromLatin1("package-x-generic"), fallback );

        const char * aud_types[] = {
            "aac", "ac3", "aiff", "ape", "au", "flac", "m3u", "m4a", "mid", "midi", "mp2",
            "mp3", "mpc", "nsf", "oga", "ogg", "ra", "ram", "shn", "voc", "wav", "wma" };
        for( int i=0, n=sizeof(aud_types)/sizeof(aud_types[0]); i<n; ++i )
            suffixes[AUDIO] << QString::fromLatin1(aud_types[i]);
        fileIcons[AUDIO] = QIcon::fromTheme( QString::fromLatin1("audio-x-generic"), fallback );

        const char * exe_types[] = { "bat", "cmd", "com", "exe" };
        for( int i=0, n=sizeof(exe_types)/sizeof(exe_types[0]); i<n; ++i )
            suffixes[APP] << QString::fromLatin1(exe_types[i]);
        fileIcons[APP] = QIcon::fromTheme( QString::fromLatin1("application-x-executable"), fallback );
    }

    QString suffix( QFileInfo( filename ).suffix( ).toLower( ) );

    for( int i=0; i<TYPE_COUNT; ++i )
        if( suffixes[i].contains( suffix ) )
            return fileIcons[i];

    return fallback;
}

bool
Utils :: isValidUtf8 ( const char *s )
{
    int n;  // number of bytes in a UTF-8 sequence

    for ( const char *c = s;  *c;  c += n )
    {
        if ( (*c & 0x80) == 0x00 )    n = 1;        // ASCII
        else if ((*c & 0xc0) == 0x80) return false; // not valid
        else if ((*c & 0xe0) == 0xc0) n = 2;
        else if ((*c & 0xf0) == 0xe0) n = 3;
        else if ((*c & 0xf8) == 0xf0) n = 4;
        else if ((*c & 0xfc) == 0xf8) n = 5;
        else if ((*c & 0xfe) == 0xfc) n = 6;
        else return false;
        for ( int m = 1; m < n; m++ )
            if ( (c[m] & 0xc0) != 0x80 )
                return false;
    } 
    return true;
}
