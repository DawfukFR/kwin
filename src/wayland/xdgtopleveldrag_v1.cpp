/*
    SPDX-FileCopyrightText: 2023 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgtopleveldrag_v1.h"
#include "dataoffer.h"
#include "datasource.h"
#include "datasource_p.h"
#include "display.h"
#include "surface.h"
#include "utils/resource.h"
#include "xdgshell.h"

#include <qwayland-server-xdg-toplevel-drag-v1.h>

#include <QPointer>

#include <wayland-util.h>

#include <memory>

namespace KWin
{
constexpr int version = 1;

class XdgToplevelDragV1InterfacePrivate : public QtWaylandServer::xdg_toplevel_drag_v1
{
public:
    XdgToplevelDragV1InterfacePrivate(wl_resource *resource, XdgToplevelDragV1Interface *q)
        : xdg_toplevel_drag_v1(resource)
        , q(q)
    {
    }
    XdgToplevelDragV1Interface *q;
    QPointer<DataSourceInterface> dataSource;
    QPointer<XdgToplevelInterface> toplevel;
    QPoint pos;

private:
    void xdg_toplevel_drag_v1_attach(Resource *resource, wl_resource *toplevelResource, int32_t x_offset, int32_t y_offset) override
    {
        if (toplevel) {
            wl_resource_post_error(resource->handle, error_toplevel_attached, "Valid toplevel already attached");
            return;
        }
        toplevel = XdgToplevelInterface::get(toplevelResource);
        QObject::connect(toplevel, &XdgToplevelInterface::resetOccurred, q, [this] {
            toplevel = nullptr;
        });
        pos = QPoint(x_offset, y_offset);
        Q_EMIT q->toplevelChanged();
    }

    void xdg_toplevel_drag_v1_destroy_resource(Resource *resource) override
    {
        delete q;
    }
    void xdg_toplevel_drag_v1_destroy(Resource *resource) override
    {
        if (!dataSource || dataSource->isDndCancelled() || dataSource->isDropPerformed()) {
            wl_resource_destroy(resource->handle);
            return;
        }
        wl_resource_post_error(resource->handle, error_ongoing_drag, "Drag has not ended");
    }
};

XdgToplevelDragV1Interface::XdgToplevelDragV1Interface(wl_resource *resource, DataSourceInterface *dataSource)
    : d(std::make_unique<XdgToplevelDragV1InterfacePrivate>(resource, this))
{
    d->dataSource = dataSource;
    DataSourceInterfacePrivate::get(dataSource)->xdgToplevelDrag = this;
}

XdgToplevelDragV1Interface::~XdgToplevelDragV1Interface()
{
    if (d->dataSource) {
        DataSourceInterfacePrivate::get(d->dataSource)->xdgToplevelDrag = nullptr;
    }
}

XdgToplevelInterface *XdgToplevelDragV1Interface::toplevel() const
{
    return d->toplevel;
}

QPoint XdgToplevelDragV1Interface::offset() const
{
    return d->pos;
}

static const wl_interface qt_toplevel_drag_manager_interface{
    .name = "qt_toplevel_drag_manager_v1",
    .version = 1,
    .method_count = xdg_toplevel_drag_manager_v1_interface.method_count,
    .methods = xdg_toplevel_drag_manager_v1_interface.methods,
    .event_count = 0,
    .events = nullptr,
};

class XdgToplevelDragManagerV1InterfacePrivate : public QtWaylandServer::xdg_toplevel_drag_manager_v1
{
public:
    XdgToplevelDragManagerV1InterfacePrivate(XdgToplevelDragManagerV1Interface *q, Display *display)
        : xdg_toplevel_drag_manager_v1(*display, version)
        , q(q)
    {
        auto bind = [](struct ::wl_client *client, void *data, uint32_t version, uint32_t id) {
            static_cast<XdgToplevelDragManagerV1InterfacePrivate *>(data)->add(client, id, version);
        };
        wl_global_create(*display, &qt_toplevel_drag_manager_interface, 1, this, bind);
    }

protected:
    void xdg_toplevel_drag_manager_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(Resource *resource, uint32_t id, wl_resource *data_source) override
    {
        auto dataSource = DataSourceInterface::get(data_source);

        wl_resource *xdg_toplevel_drag = wl_resource_create(resource->client(), &xdg_toplevel_drag_v1_interface, resource->version(), id);
        if (!xdg_toplevel_drag) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        new XdgToplevelDragV1Interface(xdg_toplevel_drag, dataSource);
    }

private:
    XdgToplevelDragManagerV1Interface *q;
    std::unique_ptr<wl_global, std::integral_constant<decltype(&wl_global_remove), wl_global_remove>> qt_toplevel_drag_manager;
};

XdgToplevelDragManagerV1Interface::XdgToplevelDragManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XdgToplevelDragManagerV1InterfacePrivate>(this, display))
{
}

XdgToplevelDragManagerV1Interface::~XdgToplevelDragManagerV1Interface() = default;
}

#include "moc_xdgtopleveldrag_v1.cpp"
