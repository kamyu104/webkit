/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollableArea.h"

#if PLATFORM(MAC)
#include "NSScrollerImpSPI.h"
#endif

namespace WebCore {

// It's conceivable that in the future, we may want some scrollbars to be on the right while other scrollbars in the same document are on the left.
// This non-static function on the ScrollableArea makes that possible.
bool ScrollableArea::verticalScrollbarIsOnLeft() const
{
    return systemLanguageIsRTL();
}

bool ScrollableArea::systemLanguageIsRTL()
{
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    static bool result = [[NSScrollerImpPair class] respondsToSelector:@selector(scrollerLayoutDirection)] && [NSScrollerImpPair scrollerLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft;
    return result;
#else
    return false;
#endif
}

}
