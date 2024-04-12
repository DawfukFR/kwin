/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_cursor_layer.h"
#include "core/iccprofile.h"
#include "drm_buffer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"

#include <gbm.h>

namespace KWin
{

EglGbmCursorLayer::EglGbmCursorLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend, pipeline->gpu()->atomicModeSetting() ? EglGbmLayerSurface::BufferTarget::Linear : EglGbmLayerSurface::BufferTarget::Dumb, EglGbmLayerSurface::FormatOption::RequireAlpha)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmCursorLayer::beginFrame()
{
    if (m_pipeline->amdgpuVrrWorkaroundActive()) {
        return std::nullopt;
    }
    // note that this allows blending to happen in sRGB or PQ encoding.
    // That's technically incorrect, but it looks okay and is intentionally allowed
    // as the hardware cursor is more important than an incorrectly blended cursor edge
    return m_surface.startRendering(m_pipeline->gpu()->cursorSize(), m_pipeline->output()->transform().combine(OutputTransform::FlipY), m_pipeline->cursorFormats(), m_pipeline->colorDescription(), m_pipeline->output()->channelFactors(), m_pipeline->iccProfile(), m_pipeline->output()->needsColormanagement());
}

bool EglGbmCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return m_surface.endRendering(damagedRegion, frame);
}

QRegion EglGbmCursorLayer::currentDamage() const
{
    return {};
}

std::shared_ptr<DrmFramebuffer> EglGbmCursorLayer::currentBuffer() const
{
    return m_surface.currentBuffer();
}

bool EglGbmCursorLayer::checkTestBuffer()
{
    return false;
}

void EglGbmCursorLayer::releaseBuffers()
{
    m_surface.destroyResources();
}

std::optional<QSize> EglGbmCursorLayer::fixedSize() const
{
    return m_pipeline->gpu()->cursorSize();
}
}
