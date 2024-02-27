/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "utils/version.h"

#include <stdint.h>
#include <string_view>

#include <QByteArray>
#include <QSet>

namespace KWin
{

class ShaderManager;
class GLFramebuffer;

class KWIN_EXPORT OpenGlContext
{
public:
    explicit OpenGlContext();
    virtual ~OpenGlContext() = default;

    bool hasVersion(const Version &version) const;

    QByteArrayView openglVersionString() const;
    Version openglVersion() const;
    QByteArrayView vendor() const;
    QByteArrayView renderer() const;
    bool isOpenglES() const;
    bool hasOpenglExtension(QByteArrayView name) const;
    bool isSoftwareRenderer() const;
    bool supportsTimerQueries() const;
    bool supportsTextureSwizzle() const;
    bool supportsTextureStorage() const;
    bool supportsARGB32Textures() const;
    bool supportsTextureUnpack() const;
    bool supportsRGTextures() const;
    bool supports16BitTextures() const;
    ShaderManager *shaderManager() const;

    /**
     * checks whether or not this context supports all the features that KWin requires
     */
    bool checkSupported() const;

    static OpenGlContext *currentContext();

protected:
    bool checkTimerQuerySupport() const;
    void setShaderManager(ShaderManager *manager);

    static OpenGlContext *s_currentContext;

    const QByteArrayView m_versionString;
    const Version m_version;
    const QByteArrayView m_vendor;
    const QByteArrayView m_renderer;
    const bool m_isOpenglES;
    const QSet<QByteArray> m_extensions;
    const bool m_supportsTimerQueries;
    const bool m_supportsTextureStorage;
    const bool m_supportsTextureSwizzle;
    const bool m_supportsARGB32Textures;
    const bool m_supportsTextureUnpack;
    const bool m_supportsRGTextures;
    const bool m_supports16BitTextures;
    ShaderManager *m_shaderManager = nullptr;
};

}
