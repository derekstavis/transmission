/******************************************************************************
 * $Id: TrackerNode.h 13251 2012-03-13 02:52:11Z livings124 $
 *
 * Copyright (c) 2009-2012 Transmission authors and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>
#import <transmission.h>

@class Torrent;

@interface TrackerNode : NSObject
{
    tr_tracker_stat fStat;
    
    Torrent * fTorrent;
}

@property (nonatomic, readonly) Torrent * torrent;

- (id) initWithTrackerStat: (tr_tracker_stat *) stat torrent: (Torrent *) torrent;

- (BOOL) isEqual: (id) object;

- (NSString *) host;
- (NSString *) fullAnnounceAddress;

- (NSInteger) tier;

- (NSUInteger) identifier;

- (NSInteger) totalSeeders;
- (NSInteger) totalLeechers;
- (NSInteger) totalDownloaded;

- (NSString *) lastAnnounceStatusString;
- (NSString *) nextAnnounceStatusString;
- (NSString *) lastScrapeStatusString;

@end
