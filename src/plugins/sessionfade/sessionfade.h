/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "effect/effect.h"
#include "effect/timeline.h"

namespace KWin
{
class GLFramebuffer;
class GLShader;
class GLTexture;

class SessionFadeEffect : public Effect
{
    Q_OBJECT

public:
    SessionFadeEffect();
    ~SessionFadeEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen) override;
    void prePaintWindow(KWin::EffectWindow *w, KWin::WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }
    static bool supported();

public Q_SLOTS:
    void startFadeOut();
    void startFadeIn(bool fadeWithState);

private:
    void saveCurrentState();
    void removeScreen(Output *output);

    struct Snapshot
    {
        std::shared_ptr<GLTexture> texture;
        std::shared_ptr<GLFramebuffer> framebuffer;
    };

    TimeLine m_timeLine;
    struct ScreenState
    {
        Snapshot m_prev;
        Snapshot m_current;
    };

    QHash<Output *, ScreenState> m_states;
    bool m_capturing = false;
    std::optional<std::chrono::milliseconds> m_firstPresent;
    std::unique_ptr<GLShader> m_shader;
    int m_previousTextureLocation = -1;
    int m_currentTextureLocation = -1;
    int m_modelViewProjectioMatrixLocation = -1;
    int m_blendFactorLocation = -1;
};

} // namespace KWin
