/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "GraphicsContext3D.h"
#if PLATFORM(IOS)
#include "GraphicsContext3DIOS.h"
#endif

#import "BlockExceptions.h"

#include "CanvasRenderingContext.h"
#include <CoreGraphics/CGBitmapContext.h>
#include "Extensions3DOpenGL.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "ImageBuffer.h"
#include "Logging.h"
#if PLATFORM(IOS)
#import "OpenGLESSPI.h"
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>
#else
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#endif
#include "WebGLLayer.h"
#include "WebGLObject.h"
#include "WebGLRenderingContextBase.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/ArrayBufferView.h>
#include <runtime/Int32Array.h>
#include <runtime/Float32Array.h>
#include <runtime/Uint8Array.h>
#include <sysexits.h>
#include <wtf/text/CString.h>

namespace WebCore {

const int maxActiveContexts = 64;
int GraphicsContext3D::numActiveContexts = 0;
const int GPUStatusCheckThreshold = 5;
int GraphicsContext3D::GPUCheckCounter = 0;

// FIXME: This class is currently empty on Mac, but will get populated as 
// the restructuring in https://bugs.webkit.org/show_bug.cgi?id=66903 is done
class GraphicsContext3DPrivate {
public:
    GraphicsContext3DPrivate(GraphicsContext3D*) { }
    
    ~GraphicsContext3DPrivate() { }
};

#if !PLATFORM(IOS)
static void setPixelFormat(Vector<CGLPixelFormatAttribute>& attribs, int colorBits, int depthBits, bool accelerated, bool supersample, bool closest, bool antialias)
{
    attribs.clear();
    
    attribs.append(kCGLPFAColorSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(colorBits));
    attribs.append(kCGLPFADepthSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(depthBits));
    
    if (accelerated)
        attribs.append(kCGLPFAAccelerated);
    else {
        attribs.append(kCGLPFARendererID);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLRendererGenericFloatID));
    }
        
    if (supersample && !antialias)
        attribs.append(kCGLPFASupersample);
        
    if (closest)
        attribs.append(kCGLPFAClosestPolicy);

    if (antialias) {
        attribs.append(kCGLPFAMultisample);
        attribs.append(kCGLPFASampleBuffers);
        attribs.append(static_cast<CGLPixelFormatAttribute>(1));
        attribs.append(kCGLPFASamples);
        attribs.append(static_cast<CGLPixelFormatAttribute>(4));
    }
        
    attribs.append(static_cast<CGLPixelFormatAttribute>(0));
}
#endif // !PLATFORM(IOS)

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return nullptr;

    if (numActiveContexts >= maxActiveContexts)
        return nullptr;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attrs, hostWindow, renderStyle));

    if (!context->m_contextObj)
        return nullptr;

    numActiveContexts++;

    return context.release();
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_contextObj(0)
#if PLATFORM(IOS)
    , m_compiler(SH_ESSL_OUTPUT)
#endif
    , m_attrs(attrs)
    , m_texture(0)
    , m_compositorTexture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_layerComposited(false)
    , m_internalColorFormat(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
    , m_private(std::make_unique<GraphicsContext3DPrivate>(this))
    , m_webglContext(0)
{
    UNUSED_PARAM(hostWindow);
    UNUSED_PARAM(renderStyle);

#if PLATFORM(IOS)
    m_contextObj = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    makeContextCurrent();
#else
    Vector<CGLPixelFormatAttribute> attribs;
    CGLPixelFormatObj pixelFormatObj = 0;
    GLint numPixelFormats = 0;
    
    // If we're configured to demand the software renderer, we'll
    // do so. We attempt to create contexts in this order:
    //
    // 1) 32 bit RGBA/32 bit depth/supersampled
    // 2) 32 bit RGBA/32 bit depth
    // 3) 32 bit RGBA/16 bit depth
    //
    // If we were not forced into software mode already, our final attempt is
    // to try that:
    //
    // 4) closest to 32 bit RGBA/16 bit depth/software renderer
    //
    // If none of that works, we simply fail and set m_contextObj to 0.

    bool useMultisampling = m_attrs.antialias;
    
    setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, true, false, useMultisampling);
    CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

    if (numPixelFormats == 0) {
        setPixelFormat(attribs, 32, 32, !attrs.forceSoftwareRenderer, false, false, useMultisampling);
        CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

        if (numPixelFormats == 0) {
            setPixelFormat(attribs, 32, 16, !attrs.forceSoftwareRenderer, false, false, useMultisampling);
            CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);

             if (!attrs.forceSoftwareRenderer && numPixelFormats == 0) {
                 setPixelFormat(attribs, 32, 16, false, false, true, false);
                 CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
                 useMultisampling = false;
            }
        }
    }

    if (numPixelFormats == 0)
        return;

    CGLError err = CGLCreateContext(pixelFormatObj, 0, &m_contextObj);
    GLint abortOnBlacklist = 0;
#if PLATFORM(MAC)
    CGLSetParameter(m_contextObj, kCGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);
#elif PLATFORM(IOS)
    CGLSetParameter(m_contextObj, kEAGLCPAbortOnGPURestartStatusBlacklisted, &abortOnBlacklist);
#endif

    CGLDestroyPixelFormat(pixelFormatObj);
    
    if (err != kCGLNoError || !m_contextObj) {
        // We were unable to create the context.
        m_contextObj = 0;
        return;
    }

    // Set the current context to the one given to us.
    CGLSetCurrentContext(m_contextObj);

#endif // !PLATFORM(IOS)
    
    validateAttributes();

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer = adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:this]);
#ifndef NDEBUG
        [m_webGLLayer setName:@"WebGL Layer"];
#endif
    END_BLOCK_OBJC_EXCEPTIONS

#if !PLATFORM(IOS)
    if (useMultisampling)
        ::glEnable(GL_MULTISAMPLE);
#endif

#if PLATFORM(IOS)
    ::glGenRenderbuffers(1, &m_texture);
#else
    // create a texture to render into
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glGenTextures(1, &m_compositorTexture);
    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glBindTexture(GL_TEXTURE_2D, 0);
#endif

    // create an FBO
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    m_state.boundFBO = m_fbo;
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);

    // create an multisample FBO
    if (m_attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_state.boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }
    
    // ANGLE initialization.

    ShBuiltInResources ANGLEResources;
    ShInitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    
    GC3Dint range[2], precision;
    getShaderPrecisionFormat(GraphicsContext3D::FRAGMENT_SHADER, GraphicsContext3D::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);
    
#if !PLATFORM(IOS)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    ::glEnable(GL_POINT_SPRITE);
#endif

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    if (m_contextObj) {
#if PLATFORM(IOS)
        makeContextCurrent();
        [m_contextObj renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
        ::glDeleteRenderbuffers(1, &m_texture);
#else
        CGLSetCurrentContext(m_contextObj);
        ::glDeleteTextures(1, &m_texture);
        ::glDeleteTextures(1, &m_compositorTexture);
#endif
        if (m_attrs.antialias) {
            ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        ::glDeleteFramebuffersEXT(1, &m_fbo);
#if PLATFORM(IOS)
        [EAGLContext setCurrentContext:0];
        [static_cast<EAGLContext*>(m_contextObj) release];
#else
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_contextObj);
#endif
        [m_webGLLayer setContext:nullptr];
        numActiveContexts--;
    }
}

#if PLATFORM(IOS)
bool GraphicsContext3D::setRenderbufferStorageFromDrawable(GC3Dsizei width, GC3Dsizei height)
{
    [m_webGLLayer setBounds:CGRectMake(0, 0, width, height)];
    [m_webGLLayer setOpaque:(m_internalColorFormat != GL_RGBA8)];

    return [m_contextObj renderbufferStorage:GL_RENDERBUFFER fromDrawable:static_cast<id<EAGLDrawable>>(m_webGLLayer.get())];
}
#endif

bool GraphicsContext3D::makeContextCurrent()
{
    if (!m_contextObj)
        return false;

#if PLATFORM(IOS)
    if ([EAGLContext currentContext] != m_contextObj)
        return [EAGLContext setCurrentContext:static_cast<EAGLContext*>(m_contextObj)];
#else
    CGLContextObj currentContext = CGLGetCurrentContext();
    if (currentContext != m_contextObj)
        return CGLSetCurrentContext(m_contextObj) == kCGLNoError;
#endif
    return true;
}

void GraphicsContext3D::checkGPUStatusIfNecessary()
{
    bool needsCheck = !GPUCheckCounter;
    GPUCheckCounter = (GPUCheckCounter + 1) % GPUStatusCheckThreshold;

    if (!needsCheck)
        return;

    GLint restartStatus = 0;
#if PLATFORM(MAC)
    CGLGetParameter(platformGraphicsContext3D(), kCGLCPGPURestartStatus, &restartStatus);
    if (restartStatus == kCGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has blacklisted us. Terminating.");
        exit(EX_OSERR);
    }
    if (restartStatus == kCGLCPGPURestartStatusCaused) {
        LOG(WebGL, "The GPU has reset us. Lose the context.");
        forceContextLost();
        CGLSetCurrentContext(0);
    }
#elif PLATFORM(IOS)
    EAGLContext* currentContext = static_cast<EAGLContext*>(PlatformGraphicsContext3D());
    [currentContext getParameter:kEAGLCPGPURestartStatus to:&restartStatus];
    if (restartStatus == kEAGLCPGPURestartStatusCaused || restartStatus == kEAGLCPGPURestartStatusBlacklisted) {
        LOG(WebGL, "The GPU has either reset or blacklisted us. Lose the context.");
        forceContextLost();
        [EAGLContext setCurrentContext:0];
    }
#endif
}

#if PLATFORM(IOS)
void GraphicsContext3D::endPaint()
{
    makeContextCurrent();
    if (m_attrs.antialias)
        resolveMultisamplingIfNecessary();
    ::glFlush();
    ::glBindRenderbuffer(GL_RENDERBUFFER, m_texture);
    [static_cast<EAGLContext*>(m_contextObj) presentRenderbuffer:GL_RENDERBUFFER];
    [EAGLContext setCurrentContext:nil];
}
#endif

bool GraphicsContext3D::isGLES2Compliant() const
{
    return false;
}

void GraphicsContext3D::setContextLostCallback(std::unique_ptr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(std::unique_ptr<ErrorMessageCallback>)
{
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
