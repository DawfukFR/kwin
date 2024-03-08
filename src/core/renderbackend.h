/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rendertarget.h"
#include "effect/globals.h"

#include <QObject>

#include <memory>

namespace KWin
{

class GraphicsBuffer;
class GraphicsBufferAllocator;
class Output;
class OverlayWindow;
class OutputLayer;
class SurfacePixmap;
class SurfacePixmapX11;
class SurfaceTexture;
class PresentationFeedback;
class RenderLoop;

class PresentationFeedback
{
public:
    explicit PresentationFeedback() = default;
    PresentationFeedback(const PresentationFeedback &copy) = delete;
    PresentationFeedback(PresentationFeedback &&move) = default;
    virtual ~PresentationFeedback() = default;

    virtual void presented(std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode) = 0;
};

class KWIN_EXPORT RenderTimeQuery
{
public:
    virtual ~RenderTimeQuery() = default;
    virtual std::chrono::nanoseconds queryRenderTime() = 0;
};

class KWIN_EXPORT CpuRenderTimeQuery : public RenderTimeQuery
{
public:
    /**
     * marks the start of the query
     */
    explicit CpuRenderTimeQuery();

    void end();

    std::chrono::nanoseconds queryRenderTime() override;

private:
    const std::chrono::steady_clock::time_point m_start;
    std::optional<std::chrono::steady_clock::time_point> m_end;
};

class KWIN_EXPORT OutputFrame
{
public:
    explicit OutputFrame(RenderLoop *loop);
    ~OutputFrame();

    void presented(std::chrono::nanoseconds refreshDuration, std::chrono::nanoseconds timestamp, PresentationMode mode);
    void failed();

    void addFeedback(std::unique_ptr<PresentationFeedback> &&feedback);

    void setContentType(ContentType type);
    std::optional<ContentType> contentType() const;

    void setPresentationMode(PresentationMode mode);
    PresentationMode presentationMode() const;

    void addRenderTimeQuery(std::unique_ptr<RenderTimeQuery> &&query);

private:
    RenderLoop *const m_loop;
    std::vector<std::unique_ptr<PresentationFeedback>> m_feedbacks;
    std::optional<ContentType> m_contentType;
    PresentationMode m_presentationMode = PresentationMode::VSync;
    std::vector<std::unique_ptr<RenderTimeQuery>> m_renderTimeQueries;
};

/**
 * The RenderBackend class is the base class for all rendering backends.
 */
class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    explicit RenderBackend(QObject *parent = nullptr);

    virtual CompositingType compositingType() const = 0;
    virtual OverlayWindow *overlayWindow() const;

    virtual bool checkGraphicsReset();

    virtual OutputLayer *primaryLayer(Output *output) = 0;
    virtual OutputLayer *cursorLayer(Output *output);
    virtual void present(Output *output, const std::shared_ptr<OutputFrame> &frame) = 0;

    virtual GraphicsBufferAllocator *graphicsBufferAllocator() const;

    virtual bool testImportBuffer(GraphicsBuffer *buffer);
    virtual QHash<uint32_t, QList<uint64_t>> supportedFormats() const;

    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap);
    virtual std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap);
};

} // namespace KWin
