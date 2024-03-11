#include "inputcapturefilter.h"

#include "device.h"
#include "libeisbackend.h"

#include "input_event.h"

#include <libeis.h>

namespace KWin
{
namespace Libeis
{
InputCaptureFilter::InputCaptureFilter(const LibeisBackend &backend)
    : m_backend(backend)
{
}

bool InputCaptureFilter::pointerEvent(MouseEvent *event, quint32 nativeButton)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &pointer = m_backend.capturingClient()->pointer) {
        if (event->type() == QMouseEvent::MouseMove) {
            eis_device_pointer_motion(pointer->eisDevice(), event->delta().x(), event->delta().y());
        } else if (event->type() == QMouseEvent::MouseButtonPress || event->type() == QMouseEvent::MouseButtonRelease) {
            eis_device_button_button(pointer->eisDevice(), nativeButton, event->type() == QMouseEvent::MouseButtonPress);
        }
    }
    return false;
}

bool InputCaptureFilter::pointerFrame()
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &pointer = m_backend.capturingClient()->pointer) {
        eis_device_frame(pointer->eisDevice(), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return false;
}

bool InputCaptureFilter::wheelEvent(WheelEvent *event)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &pointer = m_backend.capturingClient()->pointer) {
        if (event->delta()) {
            if (event->deltaV120()) {
                if (event->orientation() == Qt::Horizontal) {
                    eis_device_scroll_discrete(pointer->eisDevice(), event->deltaV120(), 0);
                } else {
                    eis_device_scroll_discrete(pointer->eisDevice(), 0, event->deltaV120());
                }
            } else {
                if (event->orientation() == Qt::Horizontal) {
                    eis_device_scroll_delta(pointer->eisDevice(), event->delta(), 0);
                } else {
                    eis_device_scroll_delta(pointer->eisDevice(), 0, event->delta());
                }
            }
        } else {
            if (event->orientation() == Qt::Horizontal) {
                eis_device_scroll_stop(pointer->eisDevice(), true, false);
            } else {
                eis_device_scroll_stop(pointer->eisDevice(), false, true);
            }
        }
    }
    return false;
}

bool InputCaptureFilter::keyEvent(KeyEvent *event)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &keyboard = m_backend.capturingClient()->keyboard) {
        eis_device_keyboard_key(keyboard->eisDevice(), event->nativeScanCode(), event->type() == QKeyEvent::KeyPress);
        eis_device_frame(keyboard->eisDevice(), std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp()).count());
    }
    return false;
}

bool InputCaptureFilter::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &abs = m_backend.capturingClient()->absoluteDevice) {
        auto touch = eis_device_touch_new(abs->eisDevice());
        m_touches.insert(id, touch);
        eis_touch_down(touch, pos.x(), pos.y());
    }
    return false;
}

bool InputCaptureFilter::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (auto touch = m_touches.value(id)) {
        eis_touch_motion(touch, pos.x(), pos.y());
    }
    return false;
}

bool InputCaptureFilter::touchUp(qint32 id, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (auto touch = m_touches.value(id)) {
        eis_touch_up(touch);
    }
    return false;
}
bool InputCaptureFilter::touchCancel()
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::touchFrame()
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    if (const auto &abs = m_backend.capturingClient()->absoluteDevice) {
        eis_device_frame(abs->eisDevice(), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return false;
}

bool InputCaptureFilter::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}

bool InputCaptureFilter::pinchGestureEnd(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::pinchGestureCancelled(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}

bool InputCaptureFilter::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::swipeGestureEnd(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::swipeGestureCancelled(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}

bool InputCaptureFilter::holdGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::holdGestureEnd(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
bool InputCaptureFilter::holdGestureCancelled(std::chrono::microseconds time)
{
    if (!m_backend.capturingClient()) {
        return false;
    }
    return false;
}
}
}
