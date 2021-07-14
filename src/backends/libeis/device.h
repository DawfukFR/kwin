/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputdevice.h"

struct eis_device;

namespace KWin
{
namespace Libeis
{
class Device : public InputDevice
{
    Q_OBJECT
public:
    explicit Device(eis_device *device, QObject *parent = nullptr);
    ~Device() override;

    eis_device *eisDevice() const {return m_device;}
    void changeDevice(eis_device *device);

    virtual QString sysName() const override;
    virtual QString name() const override;

    virtual bool isEnabled() const override;
    virtual void setEnabled(bool enabled) override;

    virtual LEDs leds() const override;
    virtual void setLeds(LEDs leds) override;

    virtual bool isKeyboard() const override;
    virtual bool isPointer() const override;
    virtual bool isTouchpad() const override;
    virtual bool isTouch() const override;
    virtual bool isTabletTool() const override;
    virtual bool isTabletPad() const override;
    virtual bool isTabletModeSwitch() const override;
    virtual bool isLidSwitch() const override;

private:
    eis_device *m_device;
    bool m_enabled;
};

}
}
