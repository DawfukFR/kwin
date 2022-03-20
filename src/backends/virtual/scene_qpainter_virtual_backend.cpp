/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_virtual_backend.h"
#include "cursor.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_output.h"

#include <QPainter>

namespace KWin
{
VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    connect(screens(), &Screens::changed, this, &VirtualQPainterBackend::createOutputs);
    createOutputs();
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

QImage *VirtualQPainterBackend::bufferForScreen(RenderOutput *output)
{
    return &m_backBuffers[output->platformOutput()];
}

QRegion VirtualQPainterBackend::beginFrame(RenderOutput *output)
{
    return output->geometry();
}

void VirtualQPainterBackend::createOutputs()
{
    m_backBuffers.clear();
    const auto outputs = m_backend->enabledOutputs();
    for (const auto &output : outputs) {
        QImage buffer(output->pixelSize(), QImage::Format_RGB32);
        buffer.fill(Qt::black);
        m_backBuffers.insert(output, buffer);
    }
}

void VirtualQPainterBackend::endFrame(RenderOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)

    static_cast<VirtualOutput *>(output->platformOutput())->vsyncMonitor()->arm();

    if (m_backend->saveFrames()) {
        m_backBuffers[output->platformOutput()].save(QStringLiteral("%1/%s-%3.png").arg(m_backend->screenshotDirPath(), output->platformOutput()->name(), QString::number(m_frameCounter++)));
    }
}

}
