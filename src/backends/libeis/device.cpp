/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "device.h"

#include <libeis.h>

namespace KWin
{
namespace Libeis
{
Device::Device(eis_device *device, QObject *parent)
    : InputDevice(parent)
    , m_device(device)
{
    eis_device_set_user_data(device, this);
    eis_device_add(device);
}

Device::~Device()
{
    eis_device_remove(m_device);
    eis_device_unref(m_device);
}

void Device::changeDevice(eis_device *device)
{
    eis_device_remove(m_device);
    eis_device_unref(m_device);
    m_device = device;
    eis_device_set_user_data(device, this);
    eis_device_add(device);
    if (m_enabled) {
        eis_device_resume(device);
    }
}

QString Device::sysName() const
{
    return QString();
}

QString Device::name() const
{
    return QString::fromUtf8(eis_device_get_name(m_device));
}

bool Device::isEnabled() const
{
    return m_enabled;
}

void Device::setEnabled(bool enabled)
{
    m_enabled = enabled;
    enabled ? eis_device_resume(m_device) : eis_device_pause(m_device);
}

LEDs Device::leds() const
{
    return LEDs();
}

void Device::setLeds(LEDs leds)
{
    Q_UNUSED(leds);
}

bool Device::isKeyboard() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_KEYBOARD);
}

bool Device::isPointer() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_POINTER) || eis_device_has_capability(m_device, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
}

bool Device::isTouchpad() const
{
    return false;
}

bool Device::isTouch() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_TOUCH);
}

bool Device::isTabletTool() const
{
    return false;
}

bool Device::isTabletPad() const
{
    return false;
}

bool Device::isTabletModeSwitch() const
{
    return false;
}

bool Device::isLidSwitch() const
{
    return false;
}

}
}
