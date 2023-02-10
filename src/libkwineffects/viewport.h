/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects_export.h"

#include <QMatrix4x4>
#include <QRectF>
#include <QRegion>

namespace KWin
{

class RenderTarget;

class KWINEFFECTS_EXPORT ViewPort
{
public:
    explicit ViewPort(const QRectF &renderRect, double scale);

    QMatrix4x4 projectionMatrix() const;
    QRectF renderRect() const;
    double scale() const;

    QRectF mapToRenderTarget(const QRectF &logicalGeometry) const;
    QRect mapToRenderTarget(const QRect &logicalGeometry) const;
    QRegion mapToRenderTarget(const QRegion &logicalGeometry) const;

private:
    const QRectF m_renderRect;
    const QMatrix4x4 m_projectionMatrix;
    const QMatrix4x4 m_logicalToLocal;
    const double m_scale;
};

} // namespace KWin
