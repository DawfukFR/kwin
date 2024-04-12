/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

#include <QObject>
#include <epoxy/egl.h>

struct wl_display;
struct wl_resource;

namespace KWin
{

struct DmaBufAttributes;
class Output;

class KWIN_EXPORT AbstractEglBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    ~AbstractEglBackend() override;
    bool makeCurrent() override;
    void doneCurrent() override;

    EGLConfig config() const;
    EglDisplay *eglDisplayObject() const override;
    EglContext *openglContext() const override;
    std::shared_ptr<EglContext> openglContextRef() const;

    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QList<uint64_t>> supportedFormats() const override;

    QList<LinuxDmaBufV1Feedback::Tranche> tranches() const;
    dev_t deviceId() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes, int plane, int format, const QSize &size) const;
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size);

protected:
    AbstractEglBackend(dev_t deviceId = 0);
    void cleanup();
    virtual void cleanupSurfaces();
    void setEglDisplay(const std::shared_ptr<EglDisplay> &display);
    void initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext(EGLConfig config);

    bool ensureGlobalShareContext(EGLConfig config);
    void destroyGlobalShareContext();
    ::EGLContext createContextInternal(::EGLContext sharedContext);
    void teardown();

    std::shared_ptr<EglDisplay> m_display;
    std::shared_ptr<EglContext> m_context;
    QList<QByteArray> m_clientExtensions;
    const dev_t m_deviceId;
    QList<LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<std::pair<GraphicsBuffer *, int>, EGLImageKHR> m_importedBuffers;
};

}
