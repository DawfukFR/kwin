/*
 *    KWin - the KDE window manager
 *    This file is part of the KDE project.
 *
 *    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
 *
 *    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "core/graphicsbuffer.h"
#include "drm_buffer.h"
#include "drm_commit.h"
#include "drm_commit_thread.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_pipeline.h"

#include <errno.h>
#include <gbm.h>

namespace KWin
{

DrmPipeline::Error DrmPipeline::presentLegacy(const std::shared_ptr<OutputFrame> &frame)
{
    if (Error err = applyPendingChangesLegacy(); err != Error::None) {
        return err;
    }
    const auto buffer = m_primaryLayer->currentBuffer();
    auto commit = std::make_unique<DrmLegacyCommit>(this, buffer, frame);
    if (!commit->doPageflip(m_pending.presentationMode)) {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
        return errnoToError();
    }
    m_commitThread->setPendingCommit(std::move(commit));
    return Error::None;
}

void DrmPipeline::forceLegacyModeset()
{
    legacyModeset();
    setLegacyGamma();
}

DrmPipeline::Error DrmPipeline::legacyModeset()
{
    if (!m_primaryLayer->checkTestBuffer()) {
        return Error::TestBufferFailed;
    }
    auto commit = std::make_unique<DrmLegacyCommit>(this, m_primaryLayer->currentBuffer(), nullptr);
    if (!commit->doModeset(m_connector, m_pending.mode.get())) {
        qCWarning(KWIN_DRM) << "Modeset failed!" << strerror(errno);
        return errnoToError();
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::commitPipelinesLegacy(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects)
{
    Error err = Error::None;
    for (const auto &pipeline : pipelines) {
        err = pipeline->applyPendingChangesLegacy();
        if (err != Error::None) {
            break;
        }
    }
    if (err != Error::None) {
        // at least try to revert the config
        for (const auto &pipeline : pipelines) {
            pipeline->revertPendingChanges();
            pipeline->applyPendingChangesLegacy();
        }
    } else {
        for (const auto &pipeline : pipelines) {
            pipeline->applyPendingChanges();
            if (mode == CommitMode::CommitModeset && pipeline->activePending()) {
                pipeline->pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
            }
        }
        for (const auto &obj : unusedObjects) {
            if (auto crtc = dynamic_cast<DrmCrtc *>(obj)) {
                drmModeSetCrtc(pipelines.front()->gpu()->fd(), crtc->id(), 0, 0, 0, nullptr, 0, nullptr);
            }
        }
    }
    return err;
}

DrmPipeline::Error DrmPipeline::applyPendingChangesLegacy()
{
    if (!m_pending.active && m_pending.crtc) {
        drmModeSetCursor(gpu()->fd(), m_pending.crtc->id(), 0, 0, 0);
    }
    if (activePending()) {
        const bool shouldEnableVrr = m_pending.presentationMode == PresentationMode::AdaptiveSync || m_pending.presentationMode == PresentationMode::AdaptiveAsync;
        if (m_pending.crtc->vrrEnabled.isValid() && !m_pending.crtc->vrrEnabled.setPropertyLegacy(shouldEnableVrr)) {
            qCWarning(KWIN_DRM) << "Setting vrr failed!" << strerror(errno);
            return errnoToError();
        }
        if (m_connector->broadcastRGB.isValid()) {
            m_connector->broadcastRGB.setEnumLegacy(DrmConnector::rgbRangeToBroadcastRgb(m_pending.rgbRange));
        }
        if (m_connector->overscan.isValid()) {
            m_connector->overscan.setPropertyLegacy(m_pending.overscan);
        } else if (m_connector->underscan.isValid()) {
            const uint32_t hborder = calculateUnderscan();
            m_connector->underscan.setEnumLegacy(m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
            m_connector->underscanVBorder.setPropertyLegacy(m_pending.overscan);
            m_connector->underscanHBorder.setPropertyLegacy(hborder);
        }
        if (m_connector->scalingMode.isValid() && m_connector->scalingMode.hasEnum(DrmConnector::ScalingMode::None)) {
            m_connector->scalingMode.setEnumLegacy(DrmConnector::ScalingMode::None);
        }
        if (m_connector->hdrMetadata.isValid()) {
            const auto blob = createHdrMetadata(m_pending.colorDescription.transferFunction());
            m_connector->hdrMetadata.setPropertyLegacy(blob ? blob->blobId() : 0);
        }
        if (m_connector->colorspace.isValid()) {
            if (m_pending.colorDescription.colorimetry() == NamedColorimetry::BT2020) {
                m_connector->colorspace.setEnumLegacy(DrmConnector::Colorspace::BT2020_RGB);
            } else {
                m_connector->colorspace.setEnumLegacy(DrmConnector::Colorspace::Default);
            }
        }
        const auto currentModeContent = m_pending.crtc->queryCurrentMode();
        if (m_pending.crtc != m_next.crtc || *m_pending.mode != currentModeContent) {
            qCDebug(KWIN_DRM) << "Using legacy path to set mode" << m_pending.mode->nativeMode()->name;
            Error err = legacyModeset();
            if (err != Error::None) {
                return err;
            }
        }
        if (m_pending.gamma != m_currentLegacyGamma) {
            if (Error err = setLegacyGamma(); err != Error::None) {
                return err;
            }
        }
        if (m_connector->contentType.isValid()) {
            m_connector->contentType.setEnumLegacy(m_pending.contentType);
        }
        setCursorLegacy();
    }
    if (!m_connector->dpms.setPropertyLegacy(activePending() ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF)) {
        qCWarning(KWIN_DRM) << "Setting legacy dpms failed!" << strerror(errno);
        return errnoToError();
    }
    return Error::None;
}

DrmPipeline::Error DrmPipeline::setLegacyGamma()
{
    if (m_pending.gamma) {
        if (drmModeCrtcSetGamma(gpu()->fd(), m_pending.crtc->id(), m_pending.gamma->lut().size(), m_pending.gamma->lut().red(), m_pending.gamma->lut().green(), m_pending.gamma->lut().blue()) != 0) {
            qCWarning(KWIN_DRM) << "Setting gamma failed!" << strerror(errno);
            return errnoToError();
        }
        m_currentLegacyGamma = m_pending.gamma;
    }
    return DrmPipeline::Error::None;
}

bool DrmPipeline::setCursorLegacy()
{
    const auto bo = cursorLayer()->currentBuffer();
    uint32_t handle = 0;
    if (bo && bo->buffer() && cursorLayer()->isEnabled()) {
        const DmaBufAttributes *attributes = bo->buffer()->dmabufAttributes();
        if (drmPrimeFDToHandle(gpu()->fd(), attributes->fd[0].get(), &handle) != 0) {
            qCWarning(KWIN_DRM) << "drmPrimeFDToHandle() failed";
            return false;
        }
    }

    struct drm_mode_cursor2 arg = {
        .flags = DRM_MODE_CURSOR_BO | DRM_MODE_CURSOR_MOVE,
        .crtc_id = m_pending.crtc->id(),
        .x = int32_t(m_cursorLayer->position().x()),
        .y = int32_t(m_cursorLayer->position().y()),
        .width = (uint32_t)gpu()->cursorSize().width(),
        .height = (uint32_t)gpu()->cursorSize().height(),
        .handle = handle,
        .hot_x = int32_t(m_cursorLayer->hotspot().x()),
        .hot_y = int32_t(m_cursorLayer->hotspot().y()),
    };
    const int ret = drmIoctl(gpu()->fd(), DRM_IOCTL_MODE_CURSOR2, &arg);

    if (handle != 0) {
        drmCloseBufferHandle(gpu()->fd(), handle);
    }
    return ret == 0;
}
}
