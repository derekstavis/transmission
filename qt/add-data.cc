/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: add-data.cc 14150 2013-07-27 21:58:14Z jordan $
 */

#include <QFile>

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h> // tr_base64_encode()

#include "add-data.h"
#include "utils.h"

int
AddData :: set( const QString& key )
{
    if( Utils::isMagnetLink( key ) )
    {
        magnet = key;
        type = MAGNET;
    }
    else if ( Utils::isUriWithSupportedScheme( key ) )
    {
        url = key;
        type = URL;
    }
    else if( QFile(key).exists( ) )
    {
        filename = key;
        type = FILENAME;

        QFile file( key );
        file.open( QIODevice::ReadOnly );
        metainfo = file.readAll( );
        file.close( );
    }
    else if( Utils::isHexHashcode( key ) )
    {
        magnet = QString::fromUtf8("magnet:?xt=urn:btih:") + key;
        type = MAGNET;
    }
    else
    {
        int len;
        char * raw = tr_base64_decode( key.toUtf8().constData(), key.toUtf8().size(), &len );
        if( raw ) {
            metainfo.append( raw, len );
            tr_free( raw );
            type = METAINFO;
        }
        else type = NONE;
    }

    return type;
}

QByteArray
AddData :: toBase64( ) const
{
    QByteArray ret;

    if( !metainfo.isEmpty( ) )
    {
        int len = 0;
        char * b64 = tr_base64_encode( metainfo.constData(), metainfo.size(), &len );
        ret = QByteArray( b64, len );
        tr_free( b64 );
    }

    return ret;
}

QString
AddData :: readableName( ) const
{
    QString ret;

    switch( type )
    {
        case FILENAME: ret = filename; break;

        case MAGNET: ret = magnet; break;

        case URL: ret = url.toString(); break;

        case METAINFO: {
            tr_info inf;
            tr_ctor * ctor = tr_ctorNew( NULL );
            tr_ctorSetMetainfo( ctor, (const uint8_t*)metainfo.constData(), metainfo.size() );
            if( tr_torrentParse( ctor, &inf ) == TR_PARSE_OK  ) {
                ret = QString::fromUtf8( inf.name ); // metainfo is required to be UTF-8
                tr_metainfoFree( &inf );
            }
            tr_ctorFree( ctor );
            break;
        }
    }

   return ret;
}
