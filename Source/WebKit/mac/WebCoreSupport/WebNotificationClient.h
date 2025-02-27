/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebCore/NotificationClient.h>

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#import <WebCore/Notification.h>
#import <wtf/HashMap.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#endif

namespace WebCore {
class Notification;
class NotificationPermissionCallback;
class ScriptExecutionContext;
class VoidCallback;
}

@class WebNotification;
@class WebNotificationPolicyListener;
@class WebView;

class WebNotificationClient : public WebCore::NotificationClient {
public:
    WebNotificationClient(WebView *);
    WebView *webView() { return m_webView; }

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // For testing purposes.
    uint64_t notificationIDForTesting(WebCore::Notification*);
#endif

private:
    bool show(WebCore::Notification*) override;
    void cancel(WebCore::Notification*) override;
    void clearNotifications(WebCore::ScriptExecutionContext*) override;
    void notificationObjectDestroyed(WebCore::Notification*) override;
    void notificationControllerDestroyed() override;
#if ENABLE(LEGACY_NOTIFICATIONS)
    void requestPermission(WebCore::ScriptExecutionContext*, PassRefPtr<WebCore::VoidCallback>) override;
#endif
#if ENABLE(NOTIFICATIONS)
    void requestPermission(WebCore::ScriptExecutionContext*, PassRefPtr<WebCore::NotificationPermissionCallback>) override;
#endif
    void cancelRequestsForPermission(WebCore::ScriptExecutionContext*) override { }
    bool hasPendingPermissionRequests(WebCore::ScriptExecutionContext*) const override;
    WebCore::NotificationClient::Permission checkPermission(WebCore::ScriptExecutionContext*) override;

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    void requestPermission(WebCore::ScriptExecutionContext*, WebNotificationPolicyListener *);
#endif

    WebView *m_webView;
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    HashMap<RefPtr<WebCore::Notification>, RetainPtr<WebNotification>> m_notificationMap;
    
    typedef HashMap<RefPtr<WebCore::ScriptExecutionContext>, Vector<RetainPtr<WebNotification>>> NotificationContextMap;
    NotificationContextMap m_notificationContextMap;

    bool m_everRequestedPermission { false };
#endif
};
