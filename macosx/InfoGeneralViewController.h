/******************************************************************************
 * $Id: InfoGeneralViewController.h 13481 2012-09-08 20:22:05Z livings124 $
 *
 * Copyright (c) 2010-2012 Transmission authors and contributors
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
#import "InfoViewController.h"

@interface InfoGeneralViewController : NSViewController <InfoViewController>
{
    NSArray * fTorrents;
    
    BOOL fSet;
    
    IBOutlet NSTextField * fPiecesField, * fHashField, * fSecureField,
                        * fDataLocationField,
                        * fCreatorField, * fDateCreatedField;
    
    IBOutlet NSTextView * fCommentView;
    
    IBOutlet NSButton * fRevealDataButton;
    
    //remove when we switch to auto layout on 10.7
    IBOutlet NSTextField * fPiecesLabel, * fHashLabel, * fSecureLabel,
                        * fCreatorLabel, * fDateCreatedLabel,
                        * fCommentLabel,
                        * fDataLocationLabel;
    IBOutlet NSTextField * fInfoSectionLabel, * fWhereSectionLabel;
    IBOutlet NSScrollView * fCommentScrollView;
}

- (void) setInfoForTorrents: (NSArray *) torrents;
- (void) updateInfo;

- (void) revealDataFile: (id) sender;

@end
