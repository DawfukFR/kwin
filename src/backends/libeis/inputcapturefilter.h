#pragma once

#include "input.h"

extern "C" {
struct eis_touch;
};

namespace KWin
{
class LibeisBackend;
namespace Libeis
{
class InputCaptureFilter : public KWin::InputEventFilter
{
public:
    InputCaptureFilter(const LibeisBackend &backend);
    ~InputCaptureFilter() override = default;

    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override;
    bool pointerFrame() override;
    bool wheelEvent(WheelEvent *event) override;

    bool keyEvent(KeyEvent *event) override;

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
    bool touchCancel() override;
    bool touchFrame() override;

    bool pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override;
    bool pinchGestureEnd(std::chrono::microseconds time) override;
    bool pinchGestureCancelled(std::chrono::microseconds time) override;

    bool swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    bool swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override;
    bool swipeGestureEnd(std::chrono::microseconds time) override;
    bool swipeGestureCancelled(std::chrono::microseconds time) override;

    bool holdGestureBegin(int fingerCount, std::chrono::microseconds time) override;
    bool holdGestureEnd(std::chrono::microseconds time) override;
    bool holdGestureCancelled(std::chrono::microseconds time) override;

private:
    const LibeisBackend &m_backend;
    QHash<qint32, eis_touch *> m_touches;
};
}
}
