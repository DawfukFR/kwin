/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/outputlayer.h"
#include "options.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "platformsupport/scenes/opengl/openglsurfacetexture_x11.h"
#include "utils/damagejournal.h"

#include "opengl/gltexture.h"
#include "opengl/gltexture_p.h"

typedef struct _XDisplay Display;

namespace KWin
{

class EglPixmapTexturePrivate;
class SoftwareVsyncMonitor;
class X11StandaloneBackend;
class EglBackend;
class GLRenderTimeQuery;

class EglLayer : public OutputLayer
{
public:
    EglLayer(EglBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

private:
    EglBackend *const m_backend;
};

class EglBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    EglBackend(::Display *display, X11StandaloneBackend *platform);
    ~EglBackend() override;

    void init() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *texture) override;
    OutputLayerBeginFrameInfo beginFrame();
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame);
    void present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    OverlayWindow *overlayWindow() const override;
    OutputLayer *primaryLayer(Output *output) override;

protected:
    EGLConfig chooseBufferConfig();
    bool initRenderingContext();

private:
    void screenGeometryChanged();
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);
    void vblank(std::chrono::nanoseconds timestamp);
    EGLSurface createSurface(xcb_window_t window);

    X11StandaloneBackend *m_backend;
    std::unique_ptr<SoftwareVsyncMonitor> m_vsyncMonitor;
    std::unique_ptr<OverlayWindow> m_overlayWindow;
    DamageJournal m_damageJournal;
    std::unique_ptr<GLFramebuffer> m_fbo;
    int m_bufferAge = 0;
    QRegion m_lastRenderedRegion;
    std::unique_ptr<EglLayer> m_layer;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    int m_havePostSubBuffer = false;
    bool m_havePlatformBase = false;
    Options::GlSwapStrategy m_swapStrategy = Options::AutoSwapStrategy;
    std::shared_ptr<OutputFrame> m_frame;
};

class EglPixmapTexture : public GLTexture
{
public:
    explicit EglPixmapTexture(EglBackend *backend);
    ~EglPixmapTexture() override;

    bool create(SurfacePixmapX11 *texture);

private:
    void onDamage() override;

    EglBackend *const m_backend;
    EGLImageKHR m_image = EGL_NO_IMAGE_KHR;
};

class EglSurfaceTextureX11 : public OpenGLSurfaceTextureX11
{
public:
    EglSurfaceTextureX11(EglBackend *backend, SurfacePixmapX11 *texture);

    bool create() override;
    void update(const QRegion &region) override;
};

} // namespace KWin
