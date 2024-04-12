/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glrendertimequery.h"
#include "opengl/glplatform.h"

namespace KWin
{

GLRenderTimeQuery::GLRenderTimeQuery(const std::shared_ptr<OpenGlContext> &context)
    : m_context(context)
{
    if (context->supportsTimerQueries()) {
        glGenQueries(1, &m_gpuProbe.query);
    }
}

GLRenderTimeQuery::~GLRenderTimeQuery()
{
    if (m_gpuProbe.query) {
        m_context->makeCurrent();
        glDeleteQueries(1, &m_gpuProbe.query);
    }
}

void GLRenderTimeQuery::begin()
{
    if (m_gpuProbe.query) {
        glGetInteger64v(GL_TIMESTAMP, &m_gpuProbe.start);
    }
    m_cpuProbe.start = std::chrono::steady_clock::now().time_since_epoch();
}

void GLRenderTimeQuery::end()
{
    m_hasResult = true;

    if (m_gpuProbe.query) {
        glQueryCounter(m_gpuProbe.query, GL_TIMESTAMP);
    }
    m_cpuProbe.end = std::chrono::steady_clock::now().time_since_epoch();
}

std::chrono::nanoseconds GLRenderTimeQuery::queryRenderTime()
{
    if (!m_hasResult) {
        return std::chrono::nanoseconds::zero();
    }
    m_hasResult = false;

    if (m_gpuProbe.query) {
        const auto current = OpenGlContext::currentContext();
        m_context->makeCurrent();
        glGetQueryObjecti64v(m_gpuProbe.query, GL_QUERY_RESULT, &m_gpuProbe.end);
        if (current && current != m_context.get()) {
            // restore the old OpenGL context, to not cause problems in other OpenGL code
            current->makeCurrent();
        }
    }

    const std::chrono::nanoseconds gpuTime(m_gpuProbe.end - m_gpuProbe.start);
    const std::chrono::nanoseconds cpuTime = m_cpuProbe.end - m_cpuProbe.start;
    // timings are pretty unpredictable in the sub-millisecond range; this minimum
    // ensures that when CPU or GPU power states change, we don't drop any frames
    const std::chrono::nanoseconds minimumTime = std::chrono::milliseconds(2);

    return std::max({gpuTime, cpuTime, minimumTime});
}

}
