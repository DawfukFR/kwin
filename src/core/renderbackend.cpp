/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"

#include <QApplication>
#include <QThread>
#include <drm_fourcc.h>
#include <ranges>

namespace KWin
{

CpuRenderTimeQuery::CpuRenderTimeQuery()
    : m_start(std::chrono::steady_clock::now())
{
}

void CpuRenderTimeQuery::end()
{
    m_end = std::chrono::steady_clock::now();
}

std::chrono::nanoseconds CpuRenderTimeQuery::queryRenderTime()
{
    Q_ASSERT(m_end);
    return *m_end - m_start;
}

OutputFrame::OutputFrame(RenderLoop *loop, std::chrono::nanoseconds refreshDuration)
    : m_loop(loop)
    , m_refreshDuration(refreshDuration)
{
}

OutputFrame::~OutputFrame()
{
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());
    if (!m_presented && m_loop) {
        RenderLoopPrivate::get(m_loop)->notifyFrameDropped();
    }
}

void OutputFrame::addFeedback(std::unique_ptr<PresentationFeedback> &&feedback)
{
    m_feedbacks.push_back(std::move(feedback));
}

void OutputFrame::presented(std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    const auto view = m_renderTimeQueries | std::views::transform([](const auto &query) {
                          return query->queryRenderTime();
                      });
    const auto renderTime = std::accumulate(view.begin(), view.end(), std::chrono::nanoseconds::zero());
    if (m_loop) {
        RenderLoopPrivate::get(m_loop)->notifyFrameCompleted(timestamp, renderTime, mode);
    }
    m_presented = true;
    for (const auto &feedback : m_feedbacks) {
        feedback->presented(m_refreshDuration, timestamp, mode);
    }
}

void OutputFrame::failed()
{
}

void OutputFrame::setContentType(ContentType type)
{
    m_contentType = type;
}

std::optional<ContentType> OutputFrame::contentType() const
{
    return m_contentType;
}

void OutputFrame::setPresentationMode(PresentationMode mode)
{
    m_presentationMode = mode;
}

PresentationMode OutputFrame::presentationMode() const
{
    return m_presentationMode;
}

void OutputFrame::addRenderTimeQuery(std::unique_ptr<RenderTimeQuery> &&query)
{
    m_renderTimeQueries.push_back(std::move(query));
}

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

OutputLayer *RenderBackend::cursorLayer(Output *output)
{
    return nullptr;
}

OverlayWindow *RenderBackend::overlayWindow() const
{
    return nullptr;
}

bool RenderBackend::checkGraphicsReset()
{
    return false;
}

GraphicsBufferAllocator *RenderBackend::graphicsBufferAllocator() const
{
    return nullptr;
}

bool RenderBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return false;
}

QHash<uint32_t, QList<uint64_t>> RenderBackend::supportedFormats() const
{
    return QHash<uint32_t, QList<uint64_t>>{{DRM_FORMAT_XRGB8888, QList<uint64_t>{DRM_FORMAT_MOD_LINEAR}}};
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return nullptr;
}

} // namespace KWin

#include "moc_renderbackend.cpp"
