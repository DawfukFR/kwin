/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QRegion>
#include <chrono>
#include <optional>

#include "core/outputlayer.h"
#include "drm_plane.h"
#include "opengl/gltexture.h"
#include "utils/damagejournal.h"
#include "utils/filedescriptor.h"

struct gbm_bo;

namespace KWin
{

class DrmFramebuffer;
class EglSwapchain;
class EglSwapchainSlot;
class QPainterSwapchain;
class ShadowBuffer;
class EglContext;
class EglGbmBackend;
class GraphicsBuffer;
class SurfaceItem;
class GLTexture;
class GLRenderTimeQuery;
class ColorTransformation;
class GlLookUpTable;
class IccProfile;
class IccShader;

class EglGbmLayerSurface : public QObject
{
    Q_OBJECT
public:
    enum class BufferTarget {
        Normal,
        Linear,
        Dumb
    };
    enum class FormatOption {
        PreferAlpha,
        RequireAlpha
    };
    EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target = BufferTarget::Normal, FormatOption formatOption = FormatOption::PreferAlpha);
    ~EglGbmLayerSurface();

    std::optional<OutputLayerBeginFrameInfo> startRendering(const QSize &bufferSize, OutputTransform transformation, const QHash<uint32_t, QList<uint64_t>> &formats, const ColorDescription &colorDescription, const QVector3D &channelFactors, const std::shared_ptr<IccProfile> &iccProfile, bool enableColormanagement);
    bool endRendering(const QRegion &damagedRegion);
    std::chrono::nanoseconds queryRenderTime() const;

    bool doesSurfaceFit(const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats) const;
    std::shared_ptr<GLTexture> texture() const;
    void destroyResources();
    EglGbmBackend *eglBackend() const;
    std::shared_ptr<DrmFramebuffer> renderTestBuffer(const QSize &bufferSize, const QHash<uint32_t, QList<uint64_t>> &formats);
    void forgetDamage();

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;
    const ColorDescription &colorDescription() const;

private:
    enum class MultiGpuImportMode {
        None,
        Dmabuf,
        LinearDmabuf,
        Egl,
        DumbBuffer
    };
    struct Surface
    {
        ~Surface();

        std::shared_ptr<EglContext> context;
        std::shared_ptr<EglSwapchain> gbmSwapchain;
        std::shared_ptr<EglSwapchainSlot> currentSlot;
        DamageJournal damageJournal;
        std::unique_ptr<QPainterSwapchain> importDumbSwapchain;
        std::shared_ptr<EglContext> importContext;
        std::shared_ptr<EglSwapchain> importGbmSwapchain;
        QHash<GraphicsBuffer *, std::shared_ptr<GLTexture>> importedTextureCache;
        QImage cpuCopyCache;
        MultiGpuImportMode importMode;
        std::shared_ptr<DrmFramebuffer> currentFramebuffer;
        BufferTarget bufferTarget;

        // for color management
        bool colormanagementEnabled = false;
        std::shared_ptr<EglSwapchain> shadowSwapchain;
        std::shared_ptr<EglSwapchainSlot> currentShadowSlot;
        ColorDescription targetColorDescription = ColorDescription::sRGB;
        ColorDescription intermediaryColorDescription = ColorDescription::sRGB;
        QVector3D channelFactors = {1, 1, 1};
        std::unique_ptr<IccShader> iccShader;
        std::shared_ptr<IccProfile> iccProfile;

        // for render timing
        std::unique_ptr<GLRenderTimeQuery> timeQuery;
        std::unique_ptr<GLRenderTimeQuery> importTimeQuery;
        std::chrono::steady_clock::time_point renderStart;
        std::chrono::steady_clock::time_point renderEnd;
    };
    bool checkSurface(const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats);
    bool doesSurfaceFit(Surface *surface, const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget) const;
    std::shared_ptr<EglSwapchain> createGbmSwapchain(DrmGpu *gpu, EglContext *context, const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget) const;

    std::shared_ptr<DrmFramebuffer> doRenderTestBuffer(Surface *surface) const;
    std::shared_ptr<DrmFramebuffer> importBuffer(Surface *surface, EglSwapchainSlot *source, const FileDescriptor &readFence) const;
    std::shared_ptr<DrmFramebuffer> importWithEgl(Surface *surface, GraphicsBuffer *sourceBuffer, const FileDescriptor &readFence) const;
    std::shared_ptr<DrmFramebuffer> importWithCpu(Surface *surface, EglSwapchainSlot *source) const;

    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<Surface> m_oldSurface;

    DrmGpu *const m_gpu;
    EglGbmBackend *const m_eglBackend;
    const BufferTarget m_requestedBufferTarget;
    const FormatOption m_formatOption;
};

}
