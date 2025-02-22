
/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "atoms.h"
#include "core/output.h"
#include "cursor.h"
#include "placement.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>
#include <xcb/xcb_icccm.h>

Q_DECLARE_METATYPE(KWin::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_quick_tiling-0");

class MoveResizeWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testMove();
    void testResize();
    void testPackTo_data();
    void testPackTo();
    void testPackAgainstClient_data();
    void testPackAgainstClient();
    void testGrowShrink_data();
    void testGrowShrink();
    void testPointerMoveEnd_data();
    void testPointerMoveEnd();
    void testClientSideMove();
    void testNetMove();
    void testAdjustClientGeometryOfHiddenX11Panel_data();
    void testAdjustClientGeometryOfHiddenX11Panel();
    void testAdjustClientGeometryOfHiddenWaylandPanel_data();
    void testAdjustClientGeometryOfHiddenWaylandPanel();
    void testResizeForVirtualKeyboard_data();
    void testResizeForVirtualKeyboard();
    void testResizeForVirtualKeyboardWithMaximize();
    void testResizeForVirtualKeyboardWithFullScreen();
    void testDestroyMoveClient();
    void testDestroyResizeClient();
    void testCancelInteractiveMoveResize_data();
    void testCancelInteractiveMoveResize();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void MoveResizeWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 1);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1 | Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();

    workspace()->setActiveOutput(QPoint(640, 512));
}

void MoveResizeWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MoveResizeWindowTest::testMove()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 50));
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy moveResizedChangedSpy(window, &Window::moveResizedChanged);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    // begin move
    QVERIFY(workspace()->moveResizeWindow() == nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(window->isInteractiveMove(), true);
    QCOMPARE(window->geometryRestore(), QRect());

    // send some key events, not going through input redirection
    const QPointF cursorPos = Cursors::self()->mouse()->pos();
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    interactiveMoveResizeSteppedSpy.clear();

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(16, 0));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);

    window->keyPressEvent(Qt::Key_Down | Qt::ALT);
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);
    QCOMPARE(window->frameGeometry(), QRect(16, 32, 100, 50));
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(16, 32));

    // let's end
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(window->frameGeometry(), QRect(16, 32, 100, 50));
    QCOMPARE(window->isInteractiveMove(), false);
    QVERIFY(workspace()->moveResizeWindow() == nullptr);
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void MoveResizeWindowTest::testResize()
{
    // a test case which manually resizes a window

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QVERIFY(shellSurface != nullptr);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Let's render.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // We have to receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 50));
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy moveResizedChangedSpy(window, &Window::moveResizedChanged);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &Window::interactiveMoveResizeStepped);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    // begin resize
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(window->isInteractiveResize(), true);
    QCOMPARE(window->geometryRestore(), QRect());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));

    // Trigger a change.
    const QPointF cursorPos = Cursors::self()->mouse()->pos();
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));

    // The client should receive a configure event with the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 4);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(108, 50));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);

    // Now render new size.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(108, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 108, 50));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);

    // Go down.
    window->keyPressEvent(Qt::Key_Down);
    window->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));

    // The client should receive another configure event.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(108, 58));

    // Now render new size.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(108, 58), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 108, 58));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 2);

    // Let's finalize the resize operation.
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 0);
    window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(window->isInteractiveResize(), false);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 6);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 6);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void MoveResizeWindowTest::testPackTo_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::newRow("left") << QStringLiteral("slotWindowMoveLeft") << QRectF(0, 487, 100, 50);
    QTest::newRow("up") << QStringLiteral("slotWindowMoveUp") << QRectF(590, 0, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowMoveRight") << QRectF(1180, 487, 100, 50);
    QTest::newRow("down") << QStringLiteral("slotWindowMoveDown") << QRectF(590, 974, 100, 50);
}

void MoveResizeWindowTest::testPackTo()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 50));

    // let's place it centered
    workspace()->placement()->placeCentered(window, QRect(0, 0, 1280, 1024));
    QCOMPARE(window->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(window->frameGeometry(), "expectedGeometry");
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void MoveResizeWindowTest::testPackAgainstClient_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::newRow("left") << QStringLiteral("slotWindowMoveLeft") << QRectF(10, 487, 100, 50);
    QTest::newRow("up") << QStringLiteral("slotWindowMoveUp") << QRectF(590, 10, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowMoveRight") << QRectF(1170, 487, 100, 50);
    QTest::newRow("down") << QStringLiteral("slotWindowMoveDown") << QRectF(590, 964, 100, 50);
}

void MoveResizeWindowTest::testPackAgainstClient()
{
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    QVERIFY(surface1 != nullptr);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    QVERIFY(surface2 != nullptr);
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    QVERIFY(surface3 != nullptr);
    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    QVERIFY(surface4 != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QVERIFY(shellSurface1 != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QVERIFY(shellSurface2 != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    QVERIFY(shellSurface3 != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    QVERIFY(shellSurface4 != nullptr);
    auto renderWindow = [](KWayland::Client::Surface *surface, const QString &methodCall, const QRect &expectedGeometry) {
        // let's render
        auto window = Test::renderAndWaitForShown(surface, QSize(10, 10), Qt::blue);

        QVERIFY(window);
        QCOMPARE(workspace()->activeWindow(), window);
        QCOMPARE(window->frameGeometry().size(), QSize(10, 10));
        // let's place it centered
        workspace()->placement()->placeCentered(window, QRect(0, 0, 1280, 1024));
        QCOMPARE(window->frameGeometry(), QRect(635, 507, 10, 10));
        QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
        QCOMPARE(window->frameGeometry(), expectedGeometry);
    };
    renderWindow(surface1.get(), QStringLiteral("slotWindowMoveLeft"), QRect(0, 507, 10, 10));
    renderWindow(surface2.get(), QStringLiteral("slotWindowMoveUp"), QRect(635, 0, 10, 10));
    renderWindow(surface3.get(), QStringLiteral("slotWindowMoveRight"), QRect(1270, 507, 10, 10));
    renderWindow(surface4.get(), QStringLiteral("slotWindowMoveDown"), QRect(635, 1014, 10, 10));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    // let's place it centered
    workspace()->placement()->placeCentered(window, QRect(0, 0, 1280, 1024));
    QCOMPARE(window->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(window->frameGeometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testGrowShrink_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRectF>("expectedGeometry");

    QTest::newRow("grow vertical") << QStringLiteral("slotWindowExpandVertical") << QRectF(590, 487, 100, 537);
    QTest::newRow("grow horizontal") << QStringLiteral("slotWindowExpandHorizontal") << QRectF(590, 487, 690, 50);
    QTest::newRow("shrink vertical") << QStringLiteral("slotWindowShrinkVertical") << QRectF(590, 487, 100, 23);
    QTest::newRow("shrink horizontal") << QStringLiteral("slotWindowShrinkHorizontal") << QRectF(590, 487, 40, 50);
}

void MoveResizeWindowTest::testGrowShrink()
{
    // block geometry helper
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    QVERIFY(surface1 != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QVERIFY(shellSurface1 != nullptr);
    Test::render(surface1.get(), QSize(650, 514), Qt::blue);
    QVERIFY(Test::waitForWaylandWindowShown());
    workspace()->slotWindowMoveRight();
    workspace()->slotWindowMoveDown();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(toplevelConfigureRequestedSpy.wait());

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // let's place it centered
    workspace()->placement()->placeCentered(window, QRect(0, 0, 1280, 1024));
    QCOMPARE(window->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::red);

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    m_connection->flush();
    QVERIFY(frameGeometryChangedSpy.wait());
    QTEST(window->frameGeometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testPointerMoveEnd_data()
{
    QTest::addColumn<int>("additionalButton");

    QTest::newRow("BTN_RIGHT") << BTN_RIGHT;
    QTest::newRow("BTN_MIDDLE") << BTN_MIDDLE;
    QTest::newRow("BTN_SIDE") << BTN_SIDE;
    QTest::newRow("BTN_EXTRA") << BTN_EXTRA;
    QTest::newRow("BTN_FORWARD") << BTN_FORWARD;
    QTest::newRow("BTN_BACK") << BTN_BACK;
    QTest::newRow("BTN_TASK") << BTN_TASK;
    for (int i = BTN_TASK + 1; i < BTN_JOYSTICK; i++) {
        QTest::newRow(QByteArray::number(i, 16).constData()) << i;
    }
}

void MoveResizeWindowTest::testPointerMoveEnd()
{
    // this test verifies that moving a window through pointer only ends if all buttons are released

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(window, workspace()->activeWindow());
    QVERIFY(!window->isInteractiveMove());

    // let's trigger the left button
    quint32 timestamp = 1;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    workspace()->slotWindowMove();
    QVERIFY(window->isInteractiveMove());

    // let's press another button
    QFETCH(int, additionalButton);
    Test::pointerButtonPressed(additionalButton, timestamp++);
    QVERIFY(window->isInteractiveMove());

    // release the left button, should still have the window moving
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(window->isInteractiveMove());

    // but releasing the other button should now end moving
    Test::pointerButtonReleased(additionalButton, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}
void MoveResizeWindowTest::testClientSideMove()
{
    input()->pointer()->warp(QPointF(640, 512));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy buttonSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // move pointer into center of geometry
    const QRectF startGeometry = window->frameGeometry();
    input()->pointer()->warp(startGeometry.center());
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(pointerEnteredSpy.first().last().toPoint(), QPoint(50, 25));
    // simulate press
    quint32 timestamp = 1;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(buttonSpy.wait());
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    shellSurface->move(*Test::waylandSeat(), buttonSpy.first().first().value<quint32>());
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(window->isInteractiveMove(), true);
    QVERIFY(pointerLeftSpy.wait());

    // move a bit
    QSignalSpy clientMoveStepSpy(window, &Window::interactiveMoveResizeStepped);
    const QPointF startPoint = startGeometry.center();
    const int dragDistance = QApplication::startDragDistance();
    // Why?
    Test::pointerMotion(startPoint + QPoint(dragDistance, dragDistance) + QPoint(6, 6), timestamp++);
    QCOMPARE(clientMoveStepSpy.count(), 1);

    // and release again
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->frameGeometry(), startGeometry.translated(QPoint(dragDistance, dragDistance) + QPoint(6, 6)));
    QCOMPARE(pointerEnteredSpy.last().last().toPoint(), QPoint(50, 25));
}

void MoveResizeWindowTest::testNetMove()
{
    // this test verifies that a move request for an X11 window through NET API works
    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      0, 0, 100, 100,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, 0, 0);
    xcb_icccm_size_hints_set_size(&hints, 1, 100, 100);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    // let's set a no-border
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::WMWindowType, NET::Properties2());
    winInfo.setWindowType(NET::Override);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    const QRectF origGeo = window->frameGeometry();

    // let's move the cursor outside the window
    input()->pointer()->warp(workspace()->activeOutput()->geometry().center());
    QVERIFY(!exclusiveContains(origGeo, Cursors::self()->mouse()->pos()));

    QSignalSpy interactiveMoveResizeStartedSpy(window, &X11Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &X11Window::interactiveMoveResizeFinished);
    QSignalSpy interactiveMoveResizeSteppedSpy(window, &X11Window::interactiveMoveResizeStepped);
    QVERIFY(!workspace()->moveResizeWindow());

    // use NETRootInfo to trigger a move request
    NETRootInfo root(c.get(), NET::Properties());
    root.moveResizeRequest(windowId, origGeo.center().x(), origGeo.center().y(), NET::Move, XCB_BUTTON_INDEX_1);
    xcb_flush(c.get());

    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(window->geometryRestore(), origGeo);
    QCOMPARE(Cursors::self()->mouse()->pos(), origGeo.center());

    // let's move a step
    input()->pointer()->warp(Cursors::self()->mouse()->pos() + QPoint(10, 10));
    QCOMPARE(interactiveMoveResizeSteppedSpy.count(), 1);
    QCOMPARE(interactiveMoveResizeSteppedSpy.first().last(), origGeo.translated(10, 10));

    // let's cancel the move resize again through the net API
    root.moveResizeRequest(windowId, window->frameGeometry().center().x(), window->frameGeometry().center().y(), NET::MoveResizeCancel, XCB_BUTTON_INDEX_1);
    xcb_flush(c.get());
    QVERIFY(interactiveMoveResizeFinishedSpy.wait());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testAdjustClientGeometryOfHiddenX11Panel_data()
{
    QTest::addColumn<QRect>("panelGeometry");
    QTest::addColumn<QPoint>("targetPoint");
    QTest::addColumn<QPoint>("expectedAdjustedPoint");
    QTest::addColumn<quint32>("hideLocation");

    QTest::newRow("top") << QRect(0, 0, 100, 20) << QPoint(50, 25) << QPoint(50, 20) << 0u;
    QTest::newRow("bottom") << QRect(0, 1024 - 20, 100, 20) << QPoint(50, 1024 - 25 - 50) << QPoint(50, 1024 - 20 - 50) << 2u;
    QTest::newRow("left") << QRect(0, 0, 20, 100) << QPoint(25, 50) << QPoint(20, 50) << 3u;
    QTest::newRow("right") << QRect(1280 - 20, 0, 20, 100) << QPoint(1280 - 25 - 100, 50) << QPoint(1280 - 20 - 100, 50) << 1u;
}

void MoveResizeWindowTest::testAdjustClientGeometryOfHiddenX11Panel()
{
    // this test verifies that auto hiding panels are ignored when adjusting client geometry
    // see BUG 365892

    // first create our panel
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_window_t windowId = xcb_generate_id(c.get());
    QFETCH(QRect, panelGeometry);
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      panelGeometry.x(), panelGeometry.y(), panelGeometry.width(), panelGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, panelGeometry.x(), panelGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, panelGeometry.width(), panelGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::WMWindowType, NET::Properties2());
    winInfo.setWindowType(NET::Dock);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *panel = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(panel);
    QCOMPARE(panel->window(), windowId);
    QCOMPARE(panel->frameGeometry(), panelGeometry);
    QVERIFY(panel->isDock());

    // let's create a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    auto testWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(testWindow);
    QVERIFY(testWindow->isMovable());
    // panel is not yet hidden, we should snap against it
    QFETCH(QPoint, targetPoint);
    QTEST(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false).toPoint(), "expectedAdjustedPoint");

    // now let's hide the panel
    QSignalSpy panelHiddenSpy(panel, &Window::windowHidden);
    QFETCH(quint32, hideLocation);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->kde_screen_edge_show, XCB_ATOM_CARDINAL, 32, 1, &hideLocation);
    xcb_flush(c.get());
    QVERIFY(panelHiddenSpy.wait());

    // now try to snap again
    QCOMPARE(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false), targetPoint);

    // and destroy the panel again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy panelClosedSpy(panel, &X11Window::closed);
    QVERIFY(panelClosedSpy.wait());

    // snap once more
    QCOMPARE(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false), targetPoint);

    // and close
    QSignalSpy windowClosedSpy(testWindow, &Window::closed);
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testAdjustClientGeometryOfHiddenWaylandPanel_data()
{
    QTest::addColumn<uint32_t>("anchor");
    QTest::addColumn<QRect>("panelGeometry");
    QTest::addColumn<QPoint>("targetPoint");
    QTest::addColumn<QPoint>("expectedAdjustedPoint");

    QTest::newRow("top") << uint32_t(Test::LayerSurfaceV1::anchor_top) << QRect(0, 0, 1280, 20) << QPoint(50, 25) << QPoint(50, 20);
    QTest::newRow("bottom") << uint32_t(Test::LayerSurfaceV1::anchor_bottom) << QRect(0, 1024 - 20, 1280, 20) << QPoint(50, 1024 - 25 - 50) << QPoint(50, 1024 - 20 - 50);
    QTest::newRow("left") << uint32_t(Test::LayerSurfaceV1::anchor_left) << QRect(0, 0, 20, 1024) << QPoint(25, 50) << QPoint(20, 50);
    QTest::newRow("right") << uint32_t(Test::LayerSurfaceV1::anchor_right) << QRect(1280 - 20, 0, 20, 1024) << QPoint(1280 - 25 - 100, 50) << QPoint(1280 - 20 - 100, 50);
}

void MoveResizeWindowTest::testAdjustClientGeometryOfHiddenWaylandPanel()
{
    // this test verifies that hidden panels are ignored when adjusting client geometry
    // see BUG 365892

    // first create our panel
    std::unique_ptr<KWayland::Client::Surface> panelSurface(Test::createSurface());
    std::unique_ptr<Test::LayerSurfaceV1> panelShellSurface(Test::createLayerSurfaceV1(panelSurface.get(), QStringLiteral("dock")));
    QFETCH(QRect, panelGeometry);
    QFETCH(uint32_t, anchor);
    panelShellSurface->set_anchor(anchor);
    panelShellSurface->set_size(panelGeometry.width(), panelGeometry.height());
    panelSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    // let's render
    QSignalSpy panelConfigureRequestedSpy(panelShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(panelConfigureRequestedSpy.wait());
    auto panel = Test::renderAndWaitForShown(panelSurface.get(), panelConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);
    QVERIFY(panel);
    QCOMPARE(panel->frameGeometry(), panelGeometry);
    QVERIFY(panel->isDock());

    // let's create a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    auto testWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(testWindow);
    QVERIFY(testWindow->isMovable());
    // panel is not yet hidden, we should snap against it
    QFETCH(QPoint, targetPoint);
    QTEST(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false).toPoint(), "expectedAdjustedPoint");

    // now let's hide the panel
    panel->setHidden(true);

    // now try to snap again
    QCOMPARE(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false), targetPoint);

    // and destroy the panel again
    QSignalSpy panelClosedSpy(panel, &Window::closed);
    panelShellSurface.reset();
    panelSurface.reset();
    QVERIFY(panelClosedSpy.wait());

    // snap once more
    QCOMPARE(Workspace::self()->adjustWindowPosition(testWindow, targetPoint, false), targetPoint);

    // and close
    QSignalSpy windowClosedSpy(testWindow, &Window::closed);
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testResizeForVirtualKeyboard_data()
{
    QTest::addColumn<QRect>("windowRect");
    QTest::addColumn<QRect>("keyboardRect");
    QTest::addColumn<QRect>("resizedWindowRect");

    QTest::newRow("standard") << QRect(100, 300, 500, 800) << QRect(0, 100, 1280, 500) << QRect(100, 0, 500, 100);
    QTest::newRow("same size") << QRect(100, 300, 500, 500) << QRect(0, 600, 1280, 400) << QRect(100, 100, 500, 500);
    QTest::newRow("smaller width") << QRect(100, 300, 500, 800) << QRect(300, 100, 100, 500) << QRect(100, 0, 500, 100);
    QTest::newRow("no height change") << QRect(100, 300, 500, 500) << QRect(0, 900, 1280, 124) << QRect(100, 300, 500, 500);
    QTest::newRow("no width change") << QRect(100, 300, 500, 500) << QRect(0, 400, 100, 500) << QRect(100, 300, 500, 500);
}

void MoveResizeWindowTest::testResizeForVirtualKeyboard()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    QFETCH(QRect, windowRect);
    QFETCH(QRect, keyboardRect);
    QFETCH(QRect, resizedWindowRect);

    // There are three things that may happen when the virtual keyboard geometry
    // is set: We move the window to the top and resize it, we move the window
    // but don't change its size (if the window is already small enough) or we
    // do not change anything because the virtual keyboard does not overlap the
    // window. We should verify that, for the first, we get both a position and
    // a size change, for the second we only get a position change and for the
    // last we get no changes.
    bool sizeChange = windowRect.size() != resizedWindowRect.size();
    bool positionChange = windowRect.topLeft() != resizedWindowRect.topLeft();

    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), windowRect.size(), Qt::blue);
    QVERIFY(window);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    surfaceConfigureRequestedSpy.clear();

    window->move(windowRect.topLeft());
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    QCOMPARE(window->frameGeometry(), windowRect);
    window->setVirtualKeyboardGeometry(keyboardRect);

    if (sizeChange) {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    } else {
        QVERIFY(surfaceConfigureRequestedSpy.count() == 0);
    }
    // render at the new size
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);

    if (positionChange || sizeChange) {
        QVERIFY(frameGeometryChangedSpy.count() > 0 || frameGeometryChangedSpy.wait());
        frameGeometryChangedSpy.clear();
    } else {
        QVERIFY(frameGeometryChangedSpy.count() == 0);
    }

    QCOMPARE(window->frameGeometry(), resizedWindowRect);
    window->setVirtualKeyboardGeometry(QRect());

    if (sizeChange) {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    }
    // render at the new size
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);

    if (positionChange || sizeChange) {
        QVERIFY(frameGeometryChangedSpy.count() > 0 || frameGeometryChangedSpy.wait());
    } else {
        QVERIFY(frameGeometryChangedSpy.count() == 0);
    }

    QCOMPARE(window->frameGeometry(), windowRect);
}

void MoveResizeWindowTest::testResizeForVirtualKeyboardWithMaximize()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 800), Qt::blue);
    QVERIFY(window);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    window->move(QPoint(100, 300));
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    QCOMPARE(window->frameGeometry(), QRect(100, 300, 500, 800));
    window->setVirtualKeyboardGeometry(QRect(0, 100, 1280, 500));
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(100, 0, 500, 100));

    window->setMaximize(true, true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));

    window->setVirtualKeyboardGeometry(QRect());
    QVERIFY(!surfaceConfigureRequestedSpy.wait(10));

    // render at the size of the configureRequested.. it won't have changed
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(!frameGeometryChangedSpy.wait(10));

    // Size will NOT be restored
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::testResizeForVirtualKeyboardWithFullScreen()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 800), Qt::blue);
    QVERIFY(window);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    window->move(QPoint(100, 300));
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    QCOMPARE(window->frameGeometry(), QRect(100, 300, 500, 800));
    window->setVirtualKeyboardGeometry(QRect(0, 100, 1280, 500));
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(100, 0, 500, 100));

    window->setFullScreen(true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));

    window->setVirtualKeyboardGeometry(QRect());
    QVERIFY(!surfaceConfigureRequestedSpy.wait(10));

    // render at the size of the configureRequested.. it won't have changed
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    // Size will NOT be restored
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::testDestroyMoveClient()
{
    // This test verifies that active move operation gets finished when
    // the associated client is destroyed.

    // Create the test client.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Start moving the client.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowMove();
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(window->isInteractiveMove(), true);
    QCOMPARE(window->isInteractiveResize(), false);

    // Let's pretend that the client crashed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
}

void MoveResizeWindowTest::testDestroyResizeClient()
{
    // This test verifies that active resize operation gets finished when
    // the associated client is destroyed.

    // Create the test client.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Start resizing the client.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), true);

    // Let's pretend that the client crashed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
}

void MoveResizeWindowTest::testCancelInteractiveMoveResize_data()
{
    QTest::addColumn<QuickTileMode>("quickTileMode");
    QTest::addColumn<MaximizeMode>("maximizeMode");

    QTest::newRow("quicktile_bottom") << QuickTileMode(QuickTileFlag::Bottom) << MaximizeMode::MaximizeRestore;
    QTest::newRow("quicktile_top") << QuickTileMode(QuickTileFlag::Top) << MaximizeMode::MaximizeRestore;
    QTest::newRow("quicktile_left") << QuickTileMode(QuickTileFlag::Left) << MaximizeMode::MaximizeRestore;
    QTest::newRow("quicktile_right") << QuickTileMode(QuickTileFlag::Right) << MaximizeMode::MaximizeRestore;
    QTest::newRow("maximize_vertical") << QuickTileMode(QuickTileFlag::None) << MaximizeMode::MaximizeVertical;
    QTest::newRow("maximize_horizontal") << QuickTileMode(QuickTileFlag::None) << MaximizeMode::MaximizeHorizontal;
    QTest::newRow("maximize_full") << QuickTileMode(QuickTileFlag::Maximize) << MaximizeMode::MaximizeFull;
}

void MoveResizeWindowTest::testCancelInteractiveMoveResize()
{
    // This test verifies that after moveresize is cancelled, all relevant window states are restored
    // to what they were before moveresize began

    // Create the test client.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // tile / maximize window
    QFETCH(QuickTileMode, quickTileMode);
    QFETCH(MaximizeMode, maximizeMode);
    if (maximizeMode) {
        window->setMaximize(maximizeMode & MaximizeMode::MaximizeVertical, maximizeMode & MaximizeMode::MaximizeHorizontal);
    } else {
        window->setQuickTileMode(quickTileMode, true);
    }
    QCOMPARE(window->quickTileMode(), quickTileMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);

    const QRectF geometry = window->moveResizeGeometry();
    const QRectF geometryRestore = window->geometryRestore();

    // Start resizing the client.
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(window->isInteractiveMove(), false);
    QCOMPARE(window->isInteractiveResize(), true);

    Test::pointerMotionRelative(QPoint(1, 1), 1);
    QCOMPARE(window->quickTileMode(), QuickTileMode());
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // cancel moveresize, all state from before should be restored
    window->keyPressEvent(Qt::Key::Key_Escape);
    QCOMPARE(window->moveResizeGeometry(), geometry);
    QCOMPARE(window->quickTileMode(), quickTileMode);
    QCOMPARE(window->requestedMaximizeMode(), maximizeMode);
    QCOMPARE(window->geometryRestore(), geometryRestore);
}
}

WAYLANDTEST_MAIN(KWin::MoveResizeWindowTest)
#include "move_resize_window_test.moc"
