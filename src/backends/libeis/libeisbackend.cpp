/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "libeisbackend.h"

#include "backends/libeis/libeis_logging.h"
#include "core/output.h"
#include "device.h"
#include "input.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "main.h"
#include "workspace.h"
#include "xkb.h"

#include <QFlags>
#include <QSocketNotifier>

#include <libeis.h>

#include <ranges>

namespace KWin
{
namespace Libeis
{

static void
eis_log_handler(eis *eis, eis_log_priority priority, const char *message, eis_log_context *context)
{
    switch (priority) {
    case EIS_LOG_PRIORITY_DEBUG:
        qCDebug(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_INFO:
        qCInfo(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_WARNING:
        qCWarning(KWIN_EIS) << "Libeis:" << message;
        break;
    case EIS_LOG_PRIORITY_ERROR:
        qCritical(KWIN_EIS) << "Libeis:" << message;
        break;
    }
}

class ClientSeat
{
public:
    ClientSeat(eis_seat *eis_seat)
        : seat(eis_seat)
    {
        eis_seat_set_user_data(eis_seat, this);
        eis_client_set_user_data(eis_seat_get_client(eis_seat), this);
    }
    ~ClientSeat()
    {
        eis_seat_unref(seat);
    }
    void updateDevices();
    eis_seat *seat;
    std::unique_ptr<Device> absoluteDevice;
    std::unique_ptr<Device> pointer;
    std::unique_ptr<Device> keyboard;
};

eis_device *createDevice(eis_seat *seat, const QByteArray &name)
{
    auto device = eis_seat_new_device(seat);

    auto client = eis_seat_get_client(seat);
    const char *clientName = eis_client_get_name(client);
    const QByteArray deviceName = clientName + (' ' + name);
    eis_device_configure_name(device, deviceName);
    return device;
}
}

eis_device *LibeisBackend::createPointer(eis_seat *seat)
{
    auto device = Libeis::createDevice(seat, "eis pointer");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_POINTER);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_SCROLL);
    eis_device_configure_capability(device, EIS_DEVICE_CAP_BUTTON);
    return device;
}

eis_device *LibeisBackend::createAbsoluteDevice(eis_seat *seat)
{
    auto device = Libeis::createDevice(seat, "eis absolute device");
    auto eisDevice = device;
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_SCROLL);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_BUTTON);
    eis_device_configure_capability(eisDevice, EIS_DEVICE_CAP_TOUCH);

    for (const auto output : workspace()->outputs()) {
        auto region = eis_device_new_region(eisDevice);
        const QRect outputGeometry = output->geometry();
        eis_region_set_offset(region, outputGeometry.x(), outputGeometry.y());
        eis_region_set_size(region, outputGeometry.width(), outputGeometry.height());
        eis_region_set_physical_scale(region, output->scale());
        eis_region_add(region);
        eis_region_unref(region);
    };

    return device;
}

eis_device *LibeisBackend::createKeyboard(eis_seat *seat)
{
    auto device = Libeis::createDevice(seat, "eis keyboard");
    eis_device_configure_capability(device, EIS_DEVICE_CAP_KEYBOARD);

    if (m_keymapFile.isValid()) {
        auto keymap = eis_device_new_keymap(device, EIS_KEYMAP_TYPE_XKB, m_keymapFile.fd(), m_keymapFile.size());
        eis_keymap_add(keymap);
        eis_keymap_unref(keymap);
    }

    return device;
}

LibeisBackend::LibeisBackend(QObject *parent)
    : InputBackend(parent)
{
    qRegisterMetaType<KWin::InputRedirection::PointerButtonState>();
    qRegisterMetaType<InputRedirection::PointerAxis>();
    qRegisterMetaType<InputRedirection::PointerAxisSource>();
    qRegisterMetaType<InputRedirection::KeyboardKeyState>();
}

LibeisBackend::~LibeisBackend()
{
    if (m_eis) {
        eis_unref(m_eis);
    }
}

void LibeisBackend::initialize()
{
    constexpr int maxSocketNumber = 32;
    QByteArray socketName;
    int socketNum = 0;
    m_eis = eis_new(nullptr);
    do {
        if (socketNum == maxSocketNumber) {
            return;
        }
        socketName = QByteArrayLiteral("eis-") + QByteArray::number(socketNum++);
    } while (eis_setup_backend_socket(m_eis, socketName));

    qputenv("LIBEI_SOCKET", socketName);
    auto env = kwinApp()->processStartupEnvironment();
    env.insert("LIBEI_SOCKET", socketName);
    kwinApp()->setProcessStartupEnvironment(env);

    const auto fd = eis_get_fd(m_eis);
    auto m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &LibeisBackend::handleEvents);

    eis_log_set_priority(m_eis, EIS_LOG_PRIORITY_DEBUG);
    eis_log_set_handler(m_eis, Libeis::eis_log_handler);

    connect(
        kwinApp(), &Application::workspaceCreated, this, [this] {
            connect(workspace(), &Workspace::outputsChanged, this, [this] {
                for (const auto &seat : m_seats) {
                    if (seat->absoluteDevice) {
                        seat->absoluteDevice->changeDevice(createAbsoluteDevice(seat->seat));
                    }
                }
            });

            const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
            m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
            connect(input()->keyboard()->keyboardLayout(), &KeyboardLayout::layoutsReconfigured, this, [this] {
                qDebug() << "layouts reconfigured";
                const QByteArray keyMap = input()->keyboard()->xkb()->keymapContents();
                m_keymapFile = RamFile("eis keymap", keyMap.data(), keyMap.size(), RamFile::Flag::SealWrite);
                for (const auto &seat : m_seats) {
                    if (seat->keyboard) {
                        seat->keyboard->changeDevice(createKeyboard(seat->seat));
                    }
                }
            });
        },
        Qt::SingleShotConnection);
}

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

void LibeisBackend::handleEvents()
{
    eis_dispatch(m_eis);
    auto eventDevice = [](eis_event *event) {
        return static_cast<Libeis::Device *>(eis_device_get_user_data(eis_event_get_device(event)));
    };
    while (eis_event *const event = eis_get_event(m_eis)) {
        auto client = eis_event_get_client(event);
        switch (eis_event_get_type(event)) {
        case EIS_EVENT_CLIENT_CONNECT: {
            if (!eis_client_is_sender(client)) {
                qCDebug(KWIN_EIS) << "disconnecting receiving client";
                // TODO right now only sender clients are implemented
                eis_client_disconnect(client);
                return;
            }
            eis_client_connect(client);

            const char *clientName = eis_client_get_name(client);
            auto seat = eis_client_new_seat(client, QByteArrayLiteral(" seat").prepend(clientName));
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_KEYBOARD);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_TOUCH);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_SCROLL);
            eis_seat_configure_capability(seat, EIS_DEVICE_CAP_BUTTON);
            eis_seat_add(seat);
            m_seats.emplace_back(std::make_unique<Libeis::ClientSeat>(seat));
            qCDebug(KWIN_EIS) << "New eis client" << clientName;
            break;
        }
        case EIS_EVENT_CLIENT_DISCONNECT: {
            auto client = eis_event_get_client(event);
            qCDebug(KWIN_EIS) << "Client disconnected" << eis_client_get_name(client);
            if (auto seat = static_cast<Libeis::ClientSeat *>(eis_client_get_user_data(client))) {
                m_seats.erase(std::ranges::find(m_seats, seat, &decltype(m_seats)::value_type::get));
            }
            eis_client_disconnect(client);
            break;
        }
        case EIS_EVENT_SEAT_BIND: {
            auto seat = eis_event_get_seat(event);
            auto clientSeat = static_cast<Libeis::ClientSeat *>(eis_seat_get_user_data(seat));
            qCDebug(KWIN_EIS) << "Client" << eis_client_get_name(eis_event_get_client(event)) << "bound to seat" << eis_seat_get_name(seat);
            auto updateDevice = [event, this](std::unique_ptr<Libeis::Device> &device, auto &&createFunc, bool shouldHave) {
                if (shouldHave) {
                    if (!device) {
                        device = std::make_unique<Libeis::Device>(std::invoke(createFunc, this, (eis_event_get_seat(event))));
                        device->setEnabled(true);
                        Q_EMIT deviceAdded(device.get());
                    }
                } else if (device) {
                    Q_EMIT deviceRemoved(device.get());
                    device.reset();
                }
            };
            updateDevice(clientSeat->absoluteDevice, &LibeisBackend::createAbsoluteDevice, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER_ABSOLUTE) || eis_event_seat_has_capability(event, EIS_DEVICE_CAP_TOUCH));
            updateDevice(clientSeat->pointer, &LibeisBackend::createPointer, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_POINTER));
            updateDevice(clientSeat->keyboard, &LibeisBackend::createKeyboard, eis_event_seat_has_capability(event, EIS_DEVICE_CAP_KEYBOARD));
            break;
        }
        case EIS_EVENT_DEVICE_CLOSED: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "closed by client";
            Q_EMIT deviceRemoved(device);
            auto seat = static_cast<Libeis::ClientSeat *>(eis_seat_get_user_data(eis_device_get_seat(device->eisDevice())));
            if (device == seat->absoluteDevice.get()) {
                seat->absoluteDevice.reset();
            } else if (device == seat->keyboard.get()) {
                seat->keyboard.reset();
            } else if (device == seat->pointer.get()) {
                seat->pointer.reset();
            }
            break;
        }
        case EIS_EVENT_FRAME: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Frame for device" << device->name();
            if (device->isTouch()) {
                Q_EMIT device->touchFrame(device);
            }
            if (device->isPointer()) {
                Q_EMIT device->pointerFrame(device);
            }
            break;
        }
        case EIS_EVENT_DEVICE_START_EMULATING: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "starts emulating";
            break;
        }
        case EIS_EVENT_DEVICE_STOP_EMULATING: {
            auto device = eventDevice(event);
            qCDebug(KWIN_EIS) << "Device" << device->name() << "stops emulating";
            break;
        }
        case EIS_EVENT_POINTER_MOTION: {
            auto device = eventDevice(event);
            const double x = eis_event_pointer_get_dx(event);
            const double y = eis_event_pointer_get_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "pointer motion" << x << y;
            const QPointF delta(x, y);
            Q_EMIT device->pointerMotion(delta, delta, currentTime(), device);
            break;
        }
        case EIS_EVENT_POINTER_MOTION_ABSOLUTE: {
            auto device = eventDevice(event);
            const double x = eis_event_pointer_get_absolute_x(event);
            const double y = eis_event_pointer_get_absolute_y(event);
            qCDebug(KWIN_EIS) << device->name() << "pointer motion absolute" << x << y;
            Q_EMIT device->pointerMotionAbsolute({x, y}, currentTime(), device);
            break;
        }
        case EIS_EVENT_BUTTON_BUTTON: {
            auto device = eventDevice(event);
            const quint32 button = eis_event_button_get_button(event);
            const bool press = eis_event_button_get_is_press(event);
            qCDebug(KWIN_EIS) << device->name() << "button" << button << press;
            Q_EMIT device->pointerButtonChanged(button, press ? InputRedirection::PointerButtonPressed : InputRedirection::PointerButtonReleased, currentTime(), device);
            break;
        }
        case EIS_EVENT_SCROLL_DELTA: {
            auto device = eventDevice(event);
            const auto x = eis_event_scroll_get_dx(event);
            const auto y = eis_event_scroll_get_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "scroll" << x << y;
            if (x != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, x, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, y, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_SCROLL_STOP:
        case EIS_EVENT_SCROLL_CANCEL: {
            auto device = eventDevice(event);
            if (eis_event_scroll_get_stop_x(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll x" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, 0, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (eis_event_scroll_get_stop_y(event)) {
                qCDebug(KWIN_EIS) << device->name() << "scroll y" << (eis_event_get_type(event) == EIS_EVENT_SCROLL_STOP ? "stop" : "cancel");
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, 0, 0, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_SCROLL_DISCRETE: {
            auto device = eventDevice(event);
            const double x = eis_event_scroll_get_discrete_dx(event);
            const double y = eis_event_scroll_get_discrete_dy(event);
            qCDebug(KWIN_EIS) << device->name() << "scroll discrete" << x << y;
            // otherwise no scroll event
            constexpr auto anglePer120Step = 15 / 120.0;
            if (x != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisHorizontal, x * anglePer120Step, x, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            if (y != 0) {
                Q_EMIT device->pointerAxisChanged(InputRedirection::PointerAxisVertical, y * anglePer120Step, y, InputRedirection::PointerAxisSourceUnknown, currentTime(), device);
            }
            break;
        }
        case EIS_EVENT_KEYBOARD_KEY: {
            auto device = eventDevice(event);
            const quint32 key = eis_event_keyboard_get_key(event);
            const bool press = eis_event_keyboard_get_key_is_press(event);
            qCDebug(KWIN_EIS) << device->name() << "key" << key << press;
            Q_EMIT device->keyChanged(key, press ? InputRedirection::KeyboardKeyPressed : InputRedirection::KeyboardKeyReleased, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_DOWN: {
            auto device = eventDevice(event);
            const auto x = eis_event_touch_get_x(event);
            const auto y = eis_event_touch_get_y(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch down" << id << x << y;
            Q_EMIT device->touchDown(id, {x, y}, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_UP: {
            auto device = eventDevice(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch up" << id;
            Q_EMIT device->touchUp(id, currentTime(), device);
            break;
        }
        case EIS_EVENT_TOUCH_MOTION: {
            auto device = eventDevice(event);
            const auto x = eis_event_touch_get_x(event);
            const auto y = eis_event_touch_get_y(event);
            const auto id = eis_event_touch_get_id(event);
            qCDebug(KWIN_EIS) << device->name() << "touch move" << id << x << y;
            Q_EMIT device->touchMotion(id, {x, y}, currentTime(), device);
            break;
        }
        }
        eis_event_unref(event);
    }
}
}
