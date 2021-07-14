/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputbackend.h"
#include "utils/ramfile.h"

#include <memory>

extern "C" {
struct eis;
struct eis_seat;
struct eis_device;
}

namespace KWin
{
class InputEventFilter;
class Output;
namespace Libeis
{
class Device;
class ClientSeat;
}

class LibeisBackend : public InputBackend
{
    Q_OBJECT
public:
    explicit LibeisBackend(QObject *parent = nullptr);
    ~LibeisBackend() override;
    void initialize() override;

private:
    void handleEvents();
    eis_device *createKeyboard(eis_seat *seat);
    eis_device *createPointer(eis_seat *seat);
    eis_device *createAbsoluteDevice(eis_seat *seat);

    eis *m_eis = nullptr;
    RamFile m_keymapFile;
    std::vector<std::unique_ptr<Libeis::ClientSeat>> m_seats;
};

}
