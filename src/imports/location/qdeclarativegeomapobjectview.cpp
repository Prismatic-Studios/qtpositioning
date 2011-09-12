/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Mobility Components.
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

#include "qdeclarativegeomapobjectview_p.h"
#include "qdeclarativegeomapmousearea_p.h"
#include "qdeclarative3dgraphicsgeomap_p.h"
#include "qdeclarativegeomapitem_p.h"

#include <QDebug>
#include <QDeclarativeParserStatus>
#include <QAbstractItemModel>
#include <QDeclarativeContext>

QT_BEGIN_NAMESPACE

/*!
    \qmlclass MapObjectView

    \brief The MapObjectView is used to populate Map from a model.
    \inherits QSGItem

    \ingroup qml-location-maps

    The MapObjectView is used to populate Map with MapObjects from a model.
    The MapObjectView element only makes sense when contained in a Map object,
    meaning that it has no standalone presentation.

    The MapObjectView element is part of the \bold{Qt.Location 5.0} module.
*/

QDeclarativeGeoMapObjectView::QDeclarativeGeoMapObjectView(QSGItem *parent)
    : QObject(parent), visible_(true), componentCompleted_(false), delegate_(0),
      model_(0), map_(0)
{
}

QDeclarativeGeoMapObjectView::~QDeclarativeGeoMapObjectView()
{
    if (map_)
        removeInstantiatedItems();
    // Remove group from map, and items from the group. This is to
    // prevent their deletion. The objects are owned by the
    // declarative objects and are to be deleted by them.
    // TODO the concept of group objects need to be analyzed - is it needed anymore?
    /*
    if (map_ && map_->mapData_) {
        map_->mapData_->removeMapObject(&group_);
        removeInstantiatedItems();
    }
    */
}

void QDeclarativeGeoMapObjectView::componentComplete()
{
    componentCompleted_ = true;
}

QVariant QDeclarativeGeoMapObjectView::model() const
{
    return modelVariant_;
}

/*!
    \qmlproperty model MapObjectView::model

    This property holds the model that provides data for
    populating data with delegates.
*/

void QDeclarativeGeoMapObjectView::setModel(const QVariant &model)
{
    if (!model.isValid() || model == modelVariant_)
        return;
    QObject *object = qvariant_cast<QObject*>(model);
    QAbstractItemModel* itemModel;
    if (!object || !(itemModel = qobject_cast<QAbstractItemModel*>(object))) {
        return;
    }
    modelVariant_ = model;
    model_ = itemModel;
    // At the moment maps only works with selected models. Because of this tight
    // restriction, we are not listening to all change signals.
    QObject::connect(model_, SIGNAL(modelReset()), this, SLOT(modelReset()));
    QObject::connect(model_, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(modelRowsRemoved(QModelIndex, int, int)));
    QObject::connect(model_, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(modelRowsInserted(QModelIndex, int, int)));
    repopulate();
    emit modelChanged();
}

void QDeclarativeGeoMapObjectView::modelReset()
{
    repopulate();
}

void QDeclarativeGeoMapObjectView::modelRowsInserted(QModelIndex, int start, int end)
{
    if (!componentCompleted_ || !map_ || !delegate_ || !model_) {
        QDeclarativeGeoMapItem* mapItem;
        for (int i = start; i <= end; ++i) {
            mapItem = createItem(i);
            if (!mapItem) {
                break;
            }
            mapItemList_.append(mapItem);
            // TODO visibility factors need to be solved
            //mapItem->setVisible(visible_);
            map_->addMapItem(mapItem);
            // TODO mouse areas are omitted atm
        }
    }
    /*
    if (!componentCompleted_ || !map_ || !map_->mapData_ || !delegate_ || !model_)
        return;
    Q_ASSERT(declarativeObjectList_.count() == group_.childObjects().count());
    QDeclarativeGeoMapObject* mapObject;
    for (int i = start; i <= end; ++i) {
        mapObject = createItem(i);
        if (!mapObject) {
            break;
        }
        declarativeObjectList_.append(mapObject);
        mapObject->setVisible(visible_);
        mapObject->setMap(map_);
        group_.addChildObject(mapObject->mapObject());
        // Needed in order for mouse areas to work.
        map_->objectMap_.insert(mapObject->mapObject(), mapObject);
    }
    Q_ASSERT(declarativeObjectList_.count() == group_.childObjects().count());
    */
}

void QDeclarativeGeoMapObjectView::modelRowsRemoved(QModelIndex, int start, int end)
{
    if (!componentCompleted_ || !map_ || !delegate_ || !model_)
        return;
    for (int i = end; i >= start; --i) {
        QDeclarativeGeoMapItem *mapItem = mapItemList_.takeAt(i);
        Q_ASSERT(mapItem);
        if (!mapItem) // bad
            break;
        map_->removeMapItem(mapItem);
        delete mapItem;
    }

    /*
    if (!componentCompleted_ || !map_ || !map_->mapData_ || !delegate_ || !model_)
        return;
    Q_ASSERT(declarativeObjectList_.count() == group_.childObjects().count());
    for (int i = end; i >= start; --i) {
        QDeclarativeGeoMapObject *object = declarativeObjectList_.takeAt(i);
        if (!object) // bad
            break;
        group_.removeChildObject(object->mapObject());
        map_->objectMap_.remove(object->mapObject());
        delete object;
    }
    Q_ASSERT(declarativeObjectList_.count() == group_.childObjects().count());
    */
}

QDeclarativeComponent* QDeclarativeGeoMapObjectView::delegate() const
{
    return delegate_;
}

/*!
    \qmlproperty Component MapObjectView::delegate

    This property holds the delegate which defines how each item in the
    model should be displayed. The Component must contain exactly one
    MapObject -derived element as the root element.

*/

void QDeclarativeGeoMapObjectView::setDelegate(QDeclarativeComponent *delegate)
{
    if (!delegate)
        return;
    delegate_ = delegate;

    repopulate();
    emit delegateChanged();
}

//void QDeclarativeGeoMapObjectView::setMapData(QDeclarativeGraphicsGeoMap* map)
void QDeclarativeGeoMapObjectView::setMapData(QDeclarative3DGraphicsGeoMap* map)
{
    if (!map || map_) // changing map on the fly not supported
        return;
    map_ = map;
    /*
    if (!map || !map->mapData_ || map_) // changing map on the fly not supported
        return;
    map_ = map;
    map_->mapData_->addMapObject(&group_);
    */
}

void QDeclarativeGeoMapObjectView::removeInstantiatedItems()
{
    if (!map_)
        return;
    for (int i = 0; i < mapItemList_.count(); ++ i) {
        map_->removeMapItem(mapItemList_.at(i));
    }
    mapItemList_.clear();
    // Delete the declarative components we have instantiated.
    // They will also delete the actual qgeomapobjects
    /*
    QList<QGeoMapObject*> mapObjects = group_.childObjects();
    if (!mapObjects.isEmpty()) {
        for (int i = 0; i < mapObjects.size(); i++) {
            group_.removeChildObject(mapObjects.at(i));

            QDeclarativeGeoMapObject *mapObject = map_->objectMap_.value(mapObjects.at(i));
            map_->recursiveRemoveFromObjectMap(mapObjects.at(i));
            delete mapObject;
        }
    }
    declarativeObjectList_.clear();
    */
}

// Removes and repopulates all items.
void QDeclarativeGeoMapObjectView::repopulate()
{
    if (!componentCompleted_ || !map_ || !delegate_ || !model_)
        return;
    // Free any earlier instances
    removeInstantiatedItems();

    // Iterate model data and instantiate delegates.
    // We could use more specialized landmark model calls here too,
    // but hopefully the support will be leveraged to a general model
    // level.
    QDeclarativeGeoMapItem* mapItem;
    for (int i = 0; i < model_->rowCount(); ++i) {
        mapItem = createItem(i);
        Q_ASSERT(mapItem);
        if (!mapItem) // bad
            break;
        mapItemList_.append(mapItem);
        // TODO what to do with visibility
        //mapObject->setVisible(visible_);
        map_->addMapItem(mapItem);
    }

    /*
    if (!componentCompleted_ || !map_ || !map_->mapData_ || !delegate_ || !model_)
        return;
    // Free any earlier instances
    removeInstantiatedItems();

    // Iterate model data and instantiate delegates.
    QDeclarativeGeoMapObject* mapObject;
    for (int i = 0; i < model_->rowCount(); ++i) {
        mapObject = createItem(i);
        if (!mapObject)
            break;
        declarativeObjectList_.append(mapObject);
        mapObject->setVisible(visible_);
        mapObject->setMap(map_);
        group_.addChildObject(mapObject->mapObject());
        // Needed in order for mouse areas to work.
        map_->recursiveAddToObjectMap(mapObject);
    }
    */
}

// Currently item creation is tightly bound to models providing
// QObject* as data. Some day this may be leveraged to any user defined
// model or e.g. XML model.
//QDeclarativeGeoMapObject* QDeclarativeGeoMapObjectView::createItem(int modelRow)
QDeclarativeGeoMapItem* QDeclarativeGeoMapObjectView::createItem(int modelRow)
{
    if (!delegate_ || !model_)
        return NULL;
    QModelIndex index = model_->index(modelRow, 0); // column 0
    if (!index.isValid()) {
        qWarning() << "QDeclarativeGeoMapObject Index is not valid: " << modelRow;
        return NULL;
    }
    QHashIterator<int, QByteArray> iterator(model_->roleNames());
    QDeclarativeContext *itemContext = new QDeclarativeContext(qmlContext(this));
    while (iterator.hasNext()) {
        iterator.next();
        QVariant modelData = model_->data(index, iterator.key());
        if (!modelData.isValid())
            continue;

        // Currently we only support QObject* type data.
        QObject* data_ptr = modelData.value<QObject*>();

        if (!data_ptr)
            continue;
        itemContext->setContextProperty(QLatin1String(iterator.value().data()), data_ptr);
        itemContext->setContextProperty(QLatin1String("model"), data_ptr);
        // To avoid name collisions (delegate has same named attribute as model's role)
        // one can add here that the data is accessible also e.g. via 'model'.
        // However this requires instantiating a dynamic qobject and assigning it the
        // dynamic property as property. Dynamic meta object code from declarative
        // code should be reused. In mobility, e.g. contacts should have an example.
    }
    QObject* obj = delegate_->create(itemContext);

    if (!obj) {
        qWarning() << "QDeclarativeGeoMapObject map object creation failed.";
        delete itemContext;
        return 0;
    }
    QDeclarativeGeoMapItem *declMapObj =  qobject_cast<QDeclarativeGeoMapItem*>(obj);
    //QDeclarativeGeoMapObject *declMapObj =  qobject_cast<QDeclarativeGeoMapObject*>(obj);
    if (!declMapObj) {
        qWarning() << "QDeclarativeGeoMapObject map object delegate is of unsupported type.";
        delete itemContext;
        return 0;
    }
    itemContext->setParent(declMapObj);
    return declMapObj;
}

/*!
    \qmlproperty bool MapObjectView::visible

    This property holds whether the delegate objects created from the
    model are visible or not. Default value is true.

*/

void QDeclarativeGeoMapObjectView::setVisible(bool visible)
{
    // TODO!
    Q_UNUSED(visible);
    // TODO visibility
    /*
    if (visible_ == visible)
        return;
    visible_ = visible;

    QList<QGeoMapObject*> mapObjects = group_.childObjects();
    if (!mapObjects.isEmpty()) {
        for (int i = 0; i < mapObjects.count(); ++i) {
            mapObjects.at(i)->setVisible(visible_);
        }
    }
    emit visibleChanged();
    */
}

bool QDeclarativeGeoMapObjectView::isVisible() const
{
    return visible_;
}

/*!
    \qmlproperty int MapObjectView::z

    This property holds the z-value of the MapObjectView.
    It determines the z-value of the instantiated delegates.

    As with other Map objects, objects with same z-value are
    drawn in insertion order.

*/

void QDeclarativeGeoMapObjectView::setZValue(qreal zValue)
{
    // TODO z values
    Q_UNUSED(zValue);
    // group_.setZValue(zValue);
    // emit zChanged();
}

qreal QDeclarativeGeoMapObjectView::zValue()
{
    // TODO z values what to do
    return 1.0;
    //return group_.zValue();
}

#include "moc_qdeclarativegeomapobjectview_p.cpp"

QT_END_NAMESPACE