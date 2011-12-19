/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include "qdeclarativeroutemapitem_p.h"
#include "qdeclarativepolylinemapitem_p.h"
#include "qdeclarativegeoroute_p.h"
#include <QtDeclarative/QDeclarativeInfo>
#include <QtGui/QPainter>


static void updatePolyline(QPolygonF& points,const Map& map, const QList<QGeoCoordinate> &path, qreal& w, qreal& h)
{

    qreal minX, maxX, minY, maxY;
    //TODO: dateline handling

    for (int i = 0; i < path.size(); ++i) {

        const QGeoCoordinate &coord = path.at(i);

        if (!coord.isValid())
            continue;

        QPointF point = map.coordinateToScreenPosition(coord, false);

        if (i == 0) {
            minX = point.x();
            maxX = point.x();
            minY = point.y();
            maxY = point.y();
        } else {
            minX = qMin(point.x(), minX);
            maxX = qMax(point.x(), maxX);
            minY = qMin(point.y(), minY);
            maxY = qMax(point.y(), maxY);
        }
        points.append(point);
    }

    points.translate(-minX, -minY);

    w = maxX - minX;
    h = maxY - minY;
}

QDeclarativeRouteMapItem::QDeclarativeRouteMapItem(QQuickItem *parent):
    QDeclarativeGeoMapItemBase(parent),
    route_(0),
    zoomLevel_(0.0)
{
    setFlag(ItemHasContents, true);
    line_.setWidth(3.0);
    QObject::connect(&line_, SIGNAL(colorChanged(QColor)),
                     this, SLOT(updateAfterLinePropertiesChanged()));
    QObject::connect(&line_, SIGNAL(widthChanged(qreal)),
                     this, SLOT(updateAfterLinePropertiesChanged()));
}

QDeclarativeRouteMapItem::~QDeclarativeRouteMapItem()
{
}

void QDeclarativeRouteMapItem::updateAfterLinePropertiesChanged()
{
    // mark dirty just in case we're a width change
    dirtyGeometry_ = true;
    updateMapItem();
}

void QDeclarativeRouteMapItem::setMap(QDeclarativeGeoMap* quickMap, Map *map)
{
    QDeclarativeGeoMapItemBase::setMap(quickMap,map);
    if (map) {
        QObject::connect(map, SIGNAL(cameraDataChanged(CameraData)), this, SLOT(handleCameraDataChanged(CameraData)));
        dirtyGeometry_ = true;
        updateMapItem();
    }
}

QDeclarativeGeoRoute* QDeclarativeRouteMapItem::route() const
{
    return route_;
}

void QDeclarativeRouteMapItem::setRoute(QDeclarativeGeoRoute *route)
{
    if (route_ == route)
        return;

    route_ = route;

    if (route_) {
        path_ = route_->routePath();
    } else {
        path_ = QList<QGeoCoordinate>();
    }

    dirtyGeometry_ = true;
    updateMapItem();
    emit routeChanged(route_);

}

QSGNode* QDeclarativeRouteMapItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);

    MapPolylineNode *node = static_cast<MapPolylineNode*>(oldNode);

    if (!node) {
        node = new MapPolylineNode();
    }

    //TODO: update only material
    if (dirtyGeometry_ || dirtyMaterial_) {
        node->update(line_.color(), polyline_, line_.width());
        dirtyGeometry_ = false;
        dirtyMaterial_ = false;
    }
    return node;
}

QDeclarativeMapLineProperties *QDeclarativeRouteMapItem::line()
{
    return &line_;
}

void QDeclarativeRouteMapItem::updateMapItem()
{
    if (!map() || path_.isEmpty())
        return;

    if (dirtyGeometry_) {
        qreal h = 0;
        qreal w = 0;
        polyline_.clear();
        updatePolyline(polyline_, *map(), path_, w, h);
        setWidth(w);
        setHeight(h);
    }
    setPositionOnMap(path_.at(0), polyline_.at(0));
}

void QDeclarativeRouteMapItem::handleCameraDataChanged(const CameraData& cameraData)
{
    if (cameraData.zoomFactor() != zoomLevel_) {
        zoomLevel_ = cameraData.zoomFactor();
        dirtyGeometry_ = true;
    }
    updateMapItem();
}

bool QDeclarativeRouteMapItem::contains(QPointF point)
{
    return polyline_.contains(point);
}

void QDeclarativeRouteMapItem::dragStarted()
{
    qmlInfo(this) << "warning: mouse dragging is not currently supported with this element.";
}