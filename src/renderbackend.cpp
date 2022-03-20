/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"

namespace KWin
{

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

OverlayWindow *RenderBackend::overlayWindow() const
{
    return nullptr;
}

bool RenderBackend::checkGraphicsReset()
{
    return false;
}

void RenderBackend::aboutToStartPainting(RenderOutput *output, const QRegion &damage)
{
    Q_UNUSED(output)
    Q_UNUSED(damage)
}

bool RenderBackend::scanout(RenderOutput *output, SurfaceItem *surfaceItem)
{
    Q_UNUSED(output)
    Q_UNUSED(surfaceItem)
    return false;
}

} // namespace KWin
