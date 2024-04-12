/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer.h"
#include "core/iccprofile.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend)
    , m_dmabufFeedback(pipeline->gpu(), eglBackend)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayer::beginFrame()
{
    m_scanoutBuffer.reset();
    m_dmabufFeedback.renderingSurface();

    return m_surface.startRendering(m_pipeline->mode()->size(), m_pipeline->output()->transform().combine(OutputTransform::FlipY), m_pipeline->formats(), m_pipeline->colorDescription(), m_pipeline->output()->channelFactors(), m_pipeline->iccProfile(), m_pipeline->output()->needsColormanagement());
}

bool EglGbmLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    const bool ret = m_surface.endRendering(damagedRegion, frame);
    if (ret) {
        m_currentDamage = damagedRegion;
    }
    return ret;
}

QRegion EglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

bool EglGbmLayer::checkTestBuffer()
{
    return m_surface.renderTestBuffer(m_pipeline->mode()->size(), m_pipeline->formats()) != nullptr;
}

std::shared_ptr<GLTexture> EglGbmLayer::texture() const
{
    if (m_scanoutBuffer) {
        const auto ret = m_surface.eglBackend()->importDmaBufAsTexture(*m_scanoutBuffer->buffer()->dmabufAttributes());
        ret->setContentTransform(m_scanoutBufferTransform.combine(OutputTransform::FlipY));
        return ret;
    } else {
        return m_surface.texture();
    }
}

ColorDescription EglGbmLayer::colorDescription() const
{
    return m_surface.colorDescription();
}

bool EglGbmLayer::scanout(SurfaceItem *surfaceItem)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }
    if (surfaceItem->colorDescription() != m_pipeline->colorDescription() || m_pipeline->output()->channelFactors() != QVector3D(1, 1, 1) || m_pipeline->iccProfile()) {
        // TODO use GAMMA_LUT, CTM and DEGAMMA_LUT to allow direct scanout with HDR
        return false;
    }

    SurfaceItemWayland *item = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!item || !item->surface()) {
        return false;
    }
    const auto surface = item->surface();
    // kernel documentation says that
    // "Devices that don’t support subpixel plane coordinates can ignore the fractional part."
    // so we need to make sure that doesn't cause a difference vs the composited result
    m_bufferSourceBox = surface->bufferSourceBox().toRect();
    if (surface->bufferSourceBox() != m_bufferSourceBox) {
        return false;
    }
    const auto neededTransform = surface->bufferTransform().combine(m_pipeline->output()->transform().inverted());
    const auto plane = m_pipeline->crtc()->primaryPlane();
    if (neededTransform != OutputTransform::Kind::Normal && (!plane || !plane->supportsTransformation(neededTransform))) {
        return false;
    }
    const auto buffer = surface->buffer();
    if (!buffer) {
        return false;
    }

    const DmaBufAttributes *dmabufAttributes = buffer->dmabufAttributes();
    if (!dmabufAttributes) {
        return false;
    }

    const auto formats = m_pipeline->formats();
    if (!formats.contains(dmabufAttributes->format)) {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        return false;
    }
    if (dmabufAttributes->modifier == DRM_FORMAT_MOD_INVALID && m_pipeline->gpu()->platform()->gpuCount() > 1) {
        // importing a buffer from another GPU without an explicit modifier can mess up the buffer format
        return false;
    }
    if (!formats[dmabufAttributes->format].contains(dmabufAttributes->modifier)) {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        return false;
    }
    m_scanoutTransform = neededTransform;
    m_scanoutBufferTransform = surface->bufferTransform();
    m_scanoutBuffer = m_pipeline->gpu()->importBuffer(buffer, FileDescriptor{});
    if (m_scanoutBuffer && m_pipeline->testScanout()) {
        m_dmabufFeedback.scanoutSuccessful(surface);
        m_currentDamage = surfaceItem->mapFromBuffer(surfaceItem->damage());
        surfaceItem->resetDamage();
        // ensure the pixmap is updated when direct scanout ends
        surfaceItem->destroyPixmap();
        m_surface.forgetDamage(); // TODO: Use absolute frame sequence numbers for indexing the DamageJournal. It's more flexible and less error-prone
        return true;
    } else {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        m_scanoutBuffer.reset();
        return false;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_surface.currentBuffer();
}

void EglGbmLayer::releaseBuffers()
{
    m_scanoutBuffer.reset();
    m_surface.destroyResources();
}

OutputTransform EglGbmLayer::hardwareTransform() const
{
    return m_scanoutBuffer ? m_scanoutTransform : OutputTransform::Normal;
}

QRect EglGbmLayer::bufferSourceBox() const
{
    return m_scanoutBuffer ? m_bufferSourceBox : QRect(QPoint(0, 0), m_surface.currentBuffer()->buffer()->size());
}
}
