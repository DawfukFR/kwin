/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "renderlayerdelegate.h"

namespace KWin
{

class OutputLayer;

class KWIN_EXPORT CursorView : public QObject
{
    Q_OBJECT

public:
    explicit CursorView(QObject *parent = nullptr);

    virtual void paint(OutputLayer *layer, const QRegion &region) = 0;
};

class KWIN_EXPORT CursorDelegate : public RenderLayerDelegate
{
    Q_OBJECT

public:
    CursorDelegate(CursorView *view);

    void paint(const QRegion &region) override;

private:
    CursorView *const m_view;
};

} // namespace KWin
