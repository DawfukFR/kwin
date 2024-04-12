/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/renderbackend.h"
#include "drm_layer.h"
#include "utils/damagejournal.h"

#include <QImage>

namespace KWin
{

class QPainterSwapchain;
class QPainterSwapchainSlot;
class DrmPipeline;
class DrmVirtualOutput;
class DrmQPainterBackend;
class DrmFramebuffer;

class DrmQPainterLayer : public DrmPipelineLayer
{
public:
    DrmQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    bool doesSwapchainFit() const;

    std::shared_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    DamageJournal m_damageJournal;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class DrmCursorQPainterLayer : public DrmPipelineLayer
{
public:
    DrmCursorQPainterLayer(DrmPipeline *pipeline);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    bool checkTestBuffer() override;
    std::shared_ptr<DrmFramebuffer> currentBuffer() const override;
    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    std::shared_ptr<QPainterSwapchain> m_swapchain;
    std::shared_ptr<QPainterSwapchainSlot> m_currentBuffer;
    std::shared_ptr<DrmFramebuffer> m_currentFramebuffer;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};

class DrmVirtualQPainterLayer : public DrmOutputLayer
{
public:
    DrmVirtualQPainterLayer(DrmVirtualOutput *output);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    QRegion currentDamage() const override;
    void releaseBuffers() override;

private:
    QImage m_image;
    QRegion m_currentDamage;
    DrmVirtualOutput *const m_output;
    std::unique_ptr<CpuRenderTimeQuery> m_renderTime;
};
}
