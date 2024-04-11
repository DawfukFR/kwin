/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"
#include "effect/globals.h"
#include "scene/itemgeometry.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTransform>

#include <optional>

namespace KWin
{

class SceneDelegate;
class Scene;
class SyncReleasePoint;
class DrmDevice;

/**
 * The Item class is the base class for items in the scene.
 */
class KWIN_EXPORT Item : public QObject
{
    Q_OBJECT

public:
    explicit Item(Item *parent = nullptr);
    ~Item() override;

    Scene *scene() const;

    qreal opacity() const;
    void setOpacity(qreal opacity);

    QPointF position() const;
    void setPosition(const QPointF &point);

    QSizeF size() const;
    void setSize(const QSizeF &size);

    int z() const;
    void setZ(int z);

    /**
     * Returns the enclosing rectangle of the item. The rect equals QRect(0, 0, width(), height()).
     */
    QRectF rect() const;
    /**
     * Returns the enclosing rectangle of the item and all of its descendants.
     */
    QRectF boundingRect() const;

    virtual QList<QRectF> shape() const;
    virtual QRegion opaque() const;

    /**
     * Returns the visual parent of the item. Note that the visual parent differs from
     * the QObject parent.
     */
    Item *parentItem() const;
    void setParentItem(Item *parent);
    QList<Item *> childItems() const;
    QList<Item *> sortedChildItems() const;

    QTransform transform() const;
    void setTransform(const QTransform &transform);

    /**
     * Maps the given @a region from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRegion mapToScene(const QRegion &region) const;
    /**
     * Maps the given @a rect from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRectF mapToScene(const QRectF &rect) const;
    /**
     * Maps the given @a rect from the scene's coordinate system to the item's coordinate
     * system.
     */
    QRectF mapFromScene(const QRectF &rect) const;

    /**
     * Moves this item right before the specified @a sibling in the parent's children list.
     */
    void stackBefore(Item *sibling);
    /**
     * Moves this item right after the specified @a sibling in the parent's children list.
     */
    void stackAfter(Item *sibling);

    bool explicitVisible() const;
    bool isVisible() const;
    void setVisible(bool visible);

    void scheduleRepaint(const QRectF &region);
    void scheduleRepaint(const QRegion &region);
    void scheduleRepaint(SceneDelegate *delegate, const QRegion &region);
    void scheduleFrame();
    QRegion repaints(SceneDelegate *delegate) const;
    void resetRepaints(SceneDelegate *delegate);

    WindowQuadList quads() const;
    virtual void preprocess();
    const ColorDescription &colorDescription() const;
    PresentationModeHint presentationHint() const;
    virtual void setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats);

Q_SIGNALS:
    void childAdded(Item *item);
    /**
     * This signal is emitted when the position of this item has changed.
     */
    void positionChanged();
    /**
     * This signal is emitted when the size of this item has changed.
     */
    void sizeChanged();

    /**
     * This signal is emitted when the rectangle that encloses this item and all of its children
     * has changed.
     */
    void boundingRectChanged();

protected:
    virtual WindowQuadList buildQuads() const;
    void discardQuads();
    void setColorDescription(const ColorDescription &description);
    void setPresentationHint(PresentationModeHint hint);
    void setScene(Scene *scene);

private:
    void addChild(Item *item);
    void removeChild(Item *item);
    void updateBoundingRect();
    void updateItemToSceneTransform();
    void scheduleRepaintInternal(const QRegion &region);
    void scheduleRepaintInternal(SceneDelegate *delegate, const QRegion &region);
    void markSortedChildItemsDirty();

    bool computeEffectiveVisibility() const;
    void updateEffectiveVisibility();
    void removeRepaints(SceneDelegate *delegate);

    Scene *m_scene = nullptr;
    QPointer<Item> m_parentItem;
    QList<Item *> m_childItems;
    QTransform m_transform;
    QTransform m_itemToSceneTransform;
    QTransform m_sceneToItemTransform;
    QRectF m_boundingRect;
    QPointF m_position;
    QSizeF m_size = QSize(0, 0);
    qreal m_opacity = 1;
    int m_z = 0;
    bool m_explicitVisible = true;
    bool m_effectiveVisible = true;
    QMap<SceneDelegate *, QRegion> m_repaints;
    mutable std::optional<WindowQuadList> m_quads;
    mutable std::optional<QList<Item *>> m_sortedChildItems;
    ColorDescription m_colorDescription = ColorDescription::sRGB;
    PresentationModeHint m_presentationHint = PresentationModeHint::VSync;
};

} // namespace KWin
