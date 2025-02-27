/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ScrollbarThemeGtk_h
#define ScrollbarThemeGtk_h

#include "ScrollbarThemeComposite.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class Scrollbar;

class ScrollbarThemeGtk final : public ScrollbarThemeComposite {
public:
    virtual ~ScrollbarThemeGtk();

    bool hasButtons(Scrollbar&) override { return true; }
    bool hasThumb(Scrollbar&) override;
    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect trackRect(Scrollbar&, bool) override;

#ifndef GTK_API_VERSION_2
    ScrollbarThemeGtk();

    using ScrollbarThemeComposite::thumbRect;
    IntRect thumbRect(Scrollbar&, const IntRect& unconstrainedTrackRect);
    bool paint(Scrollbar&, GraphicsContext&, const IntRect& damageRect) override;
    void paintScrollbarBackground(GraphicsContext&, Scrollbar&) override;
    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    ScrollbarButtonPressAction handleMousePressEvent(Scrollbar&, const PlatformMouseEvent&, ScrollbarPart) override;
    int scrollbarThickness(ScrollbarControlSize) override;
    int minimumThumbLength(Scrollbar&) override;

    // TODO: These are the default GTK+ values. At some point we should pull these from the theme itself.
    double initialAutoscrollTimerDelay() override { return 0.20; }
    double autoscrollTimerDelay() override { return 0.02; }
    void themeChanged() override;
    bool usesOverlayScrollbars() const override { return m_usesOverlayScrollbars; }
    // When using overlay scrollbars, always invalidate the whole scrollbar when entering/leaving.
    bool invalidateOnMouseEnterExit() override { return m_usesOverlayScrollbars; }

private:
    void updateThemeProperties();
    enum class StyleContextMode { Layout, Paint };
    GRefPtr<GtkStyleContext> getOrCreateStyleContext(Scrollbar* = nullptr, StyleContextMode = StyleContextMode::Layout);

    IntSize buttonSize(Scrollbar&, ScrollbarPart);
    int stepperSize(Scrollbar&, ScrollbarPart);
    int thumbFatness(Scrollbar&);
    int thumbFatness(GtkStyleContext*, ScrollbarOrientation = VerticalScrollbar);
    void getTroughBorder(Scrollbar&, GtkBorder*);
    void getTroughBorder(GtkStyleContext*, GtkBorder*);
    int scrollbarThickness(GtkStyleContext*, ScrollbarOrientation = VerticalScrollbar);
    void getStepperSpacing(Scrollbar&, ScrollbarPart, GtkBorder*);
    bool troughUnderSteppers(Scrollbar&);

    GRefPtr<GtkStyleContext> m_cachedStyleContext;
    gboolean m_hasForwardButtonStartPart;
    gboolean m_hasForwardButtonEndPart;
    gboolean m_hasBackButtonStartPart;
    gboolean m_hasBackButtonEndPart;
    bool m_usesOverlayScrollbars { false };
#endif // GTK_API_VERSION_2
};

}

#endif
