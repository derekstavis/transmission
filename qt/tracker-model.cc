/*
 * This file Copyright (C) Mnemosyne LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * $Id: tracker-model.cc 11092 2010-08-01 20:36:13Z charles $
 */

#include <algorithm> // std::sort()

#include <QUrl>

#include "app.h" // MyApp
#include "tracker-model.h"

int
TrackerModel :: rowCount( const QModelIndex& parent ) const
{
    Q_UNUSED( parent );

    return parent.isValid() ? 0 : myRows.size();
}

QVariant
TrackerModel :: data( const QModelIndex& index, int role ) const
{
    QVariant var;

    const int row = index.row( );
    if( ( 0<=row ) && ( row<myRows.size( ) ) )
    {
        const TrackerInfo& trackerInfo = myRows.at( row );

        switch( role )
        {
            case Qt::DisplayRole:
                var = QString( trackerInfo.st.announce );
                break;

            case Qt::DecorationRole:
                var = trackerInfo.st.getFavicon( );
                break;

            case TrackerRole:
                var = qVariantFromValue( trackerInfo );
                break;

            default:
                break;
        }
    }

    return var;
}

/***
****
***/

struct CompareTrackers {
    bool operator()( const TrackerInfo& a, const TrackerInfo& b ) const {
        if( a.torrentId   != b.torrentId   ) return a.torrentId < b.torrentId;
        if( a.st.tier     != b.st.tier     ) return a.st.tier < b.st.tier;
        if( a.st.isBackup != b.st.isBackup ) return !a.st.isBackup;
        return a.st.announce < b.st.announce;
    }
};

void
TrackerModel :: refresh( const TorrentModel& torrentModel, const QSet<int>& ids )
{
    // build a list of the TrackerInfos
    QVector<TrackerInfo> trackers;
    foreach( int id, ids ) {
        const Torrent * tor = torrentModel.getTorrentFromId( id );
        if( tor != 0 ) {
            const TrackerStatsList trackerList = tor->trackerStats( );
            foreach( const TrackerStat& st, trackerList ) {
                TrackerInfo trackerInfo;
                trackerInfo.st = st;
                trackerInfo.torrentId = id;
                trackers.append( trackerInfo );
            }
        }
    }

    // sort 'em
    CompareTrackers comp;
    std::sort( trackers.begin(), trackers.end(), comp );

    // merge 'em with the existing list
    int old_index = 0;
    int new_index = 0;

    while( ( old_index < myRows.size() ) || ( new_index < trackers.size() ) )
    {
        if( old_index == myRows.size() )
        {
            // add this new row
            beginInsertRows( QModelIndex( ), old_index, old_index );
            myRows.insert( old_index, trackers.at( new_index ) );
            endInsertRows( );
            ++old_index;
            ++new_index;
        }
        else if( new_index == trackers.size() )
        {
            // remove this old row
            beginRemoveRows( QModelIndex( ), old_index, old_index );
            myRows.remove( old_index );
            endRemoveRows( );
        }
        else if( comp( myRows.at(old_index), trackers.at(new_index) ) )
        {
            // remove this old row
            beginRemoveRows( QModelIndex( ), old_index, old_index );
            myRows.remove( old_index );
            endRemoveRows( );
        }
        else if( comp( trackers.at(new_index), myRows.at(old_index) ) )
        {
            // add this new row
            beginInsertRows( QModelIndex( ), old_index, old_index );
            myRows.insert( old_index, trackers.at( new_index ) );
            endInsertRows( );
            ++old_index;
            ++new_index;
        }
        else // update existing row
        {
            myRows[old_index].st = trackers.at(new_index).st;
            QModelIndex topLeft;
            QModelIndex bottomRight;
            dataChanged( index(old_index,0), index(old_index,0) );
            ++old_index;
            ++new_index;
        }
    }
}

int
TrackerModel :: find( int torrentId, const QString& url ) const
{
    for( int i=0, n=myRows.size(); i<n; ++i ) {
        const TrackerInfo& inf = myRows.at(i);
        if( ( inf.torrentId == torrentId ) && ( url == inf.st.announce ) )
            return i;
    }

    return -1;
}
