/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBKeyRange_h
#define IDBKeyRange_h

#if ENABLE(INDEXED_DATABASE)

#include "Dictionary.h"
#include "IDBKey.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>

namespace WebCore {

typedef int ExceptionCode;

class IDBKeyRange : public ScriptWrappable, public RefCounted<IDBKeyRange> {
public:
    enum LowerBoundType {
        LowerBoundOpen,
        LowerBoundClosed
    };
    enum UpperBoundType {
        UpperBoundOpen,
        UpperBoundClosed
    };

    static Ref<IDBKeyRange> create(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, LowerBoundType lowerType, UpperBoundType upperType)
    {
        return adoptRef(*new IDBKeyRange(WTFMove(lower), WTFMove(upper), lowerType, upperType));
    }
    static Ref<IDBKeyRange> create(RefPtr<IDBKey>&&);
    ~IDBKeyRange() { }

    IDBKey* lower() const { return m_lower.get(); }
    IDBKey* upper() const { return m_upper.get(); }

    Deprecated::ScriptValue lowerValue(ScriptExecutionContext&) const;
    Deprecated::ScriptValue upperValue(ScriptExecutionContext&) const;
    bool lowerOpen() const { return m_lowerType == LowerBoundOpen; }
    bool upperOpen() const { return m_upperType == UpperBoundOpen; }

    static RefPtr<IDBKeyRange> only(RefPtr<IDBKey>&& value, ExceptionCode&);
    static RefPtr<IDBKeyRange> only(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCode&);

    static RefPtr<IDBKeyRange> lowerBound(ScriptExecutionContext& context, const Deprecated::ScriptValue& bound, ExceptionCode& ec) { return lowerBound(context, bound, false, ec); }
    static RefPtr<IDBKeyRange> lowerBound(ScriptExecutionContext&, const Deprecated::ScriptValue& bound, bool open, ExceptionCode&);

    static RefPtr<IDBKeyRange> upperBound(ScriptExecutionContext& context, const Deprecated::ScriptValue& bound, ExceptionCode& ec) { return upperBound(context, bound, false, ec); }
    static RefPtr<IDBKeyRange> upperBound(ScriptExecutionContext&, const Deprecated::ScriptValue& bound, bool open, ExceptionCode&);

    static RefPtr<IDBKeyRange> bound(ScriptExecutionContext& context, const Deprecated::ScriptValue& lower, const Deprecated::ScriptValue& upper, ExceptionCode& ec) { return bound(context, lower, upper, false, false, ec); }
    static RefPtr<IDBKeyRange> bound(ScriptExecutionContext& context, const Deprecated::ScriptValue& lower, const Deprecated::ScriptValue& upper, bool lowerOpen, ExceptionCode& ec) { return bound(context, lower, upper, lowerOpen, false, ec); }
    static RefPtr<IDBKeyRange> bound(ScriptExecutionContext&, const Deprecated::ScriptValue& lower, const Deprecated::ScriptValue& upper, bool lowerOpen, bool upperOpen, ExceptionCode&);

    WEBCORE_EXPORT bool isOnlyKey() const;

private:
    IDBKeyRange(RefPtr<IDBKey>&& lower, RefPtr<IDBKey>&& upper, LowerBoundType lowerType, UpperBoundType upperType);

    RefPtr<IDBKey> m_lower;
    RefPtr<IDBKey> m_upper;
    LowerBoundType m_lowerType;
    UpperBoundType m_upperType;
};

} // namespace WebCore

#endif

#endif // IDBKeyRange_h
