/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: favicon.cc 14150 2013-07-27 21:58:14Z jordan $
 */

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include "favicon.h"

/***
****
***/

Favicons :: Favicons( )
{
    myNAM = new QNetworkAccessManager( );
    connect( myNAM, SIGNAL(finished(QNetworkReply*)), this, SLOT(onRequestFinished(QNetworkReply*)) );
}

Favicons :: ~Favicons( )
{
    delete myNAM;
}

/***
****
***/

QString
Favicons :: getCacheDir( )
{
    const QString base = QStandardPaths::writableLocation (QStandardPaths::CacheLocation);
    return QDir( base ).absoluteFilePath( "favicons" );
}

void
Favicons :: ensureCacheDirHasBeenScanned( )
{
    static bool hasBeenScanned = false;

    if( !hasBeenScanned )
    {
        hasBeenScanned = true;

        QDir cacheDir( getCacheDir( ) );
        cacheDir.mkpath( cacheDir.absolutePath( ) );

        QStringList files = cacheDir.entryList( QDir::Files|QDir::Readable );
        foreach( QString file, files ) {
            QPixmap pixmap;
            pixmap.load( cacheDir.absoluteFilePath( file ) );
            if( !pixmap.isNull( ) )
                myPixmaps.insert( file, pixmap );
        }
    }
}

QString
Favicons :: getHost( const QUrl& url )
{
    QString host = url.host( );
    const int first_dot = host.indexOf( '.' );
    const int last_dot = host.lastIndexOf( '.' );

    if( ( first_dot != -1 ) && ( last_dot != -1 ) && ( first_dot != last_dot ) )
        host.remove( 0, first_dot + 1 );

    return host;
}

QPixmap
Favicons :: find( const QUrl& url )
{
    return findFromHost( getHost( url ) );
}

namespace
{
    const QSize rightSize( 16, 16 );
};

QPixmap
Favicons :: findFromHost( const QString& host )
{
    ensureCacheDirHasBeenScanned( );

    const QPixmap pixmap = myPixmaps[ host ];
    return pixmap.size()==rightSize ? pixmap : pixmap.scaled(rightSize);
}

void
Favicons :: add( const QUrl& url )
{
    ensureCacheDirHasBeenScanned( );

    const QString host = getHost( url );

    if( !myPixmaps.contains( host ) )
    {
        // add a placholder s.t. we only ping the server once per session
        QPixmap tmp( rightSize );
        tmp.fill( Qt::transparent );
        myPixmaps.insert( host, tmp );

        // try to download the favicon
        const QString path = "http://" + host + "/favicon.";
        QStringList suffixes;
        suffixes << "ico" << "png" << "gif" << "jpg";
        foreach( QString suffix, suffixes )
            myNAM->get( QNetworkRequest( path + suffix ) );
    }
}

void
Favicons :: onRequestFinished( QNetworkReply * reply )
{
    const QString host = reply->url().host();

    QPixmap pixmap;

    const QByteArray content = reply->readAll( );
    if( !reply->error( ) )
        pixmap.loadFromData( content );

    if( !pixmap.isNull( ) )
    {
        // save it in memory...
        myPixmaps.insert( host, pixmap );

        // save it on disk...
        QDir cacheDir( getCacheDir( ) );
        cacheDir.mkpath( cacheDir.absolutePath( ) );
        QFile file( cacheDir.absoluteFilePath( host ) );
        file.open( QIODevice::WriteOnly );
        file.write( content );
        file.close( );

        // notify listeners
        emit pixmapReady( host );
    }
}
