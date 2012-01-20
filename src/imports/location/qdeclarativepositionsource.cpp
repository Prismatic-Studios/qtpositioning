/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include <QtDeclarative/qdeclarativeinfo.h>
#include "qdeclarativepositionsource_p.h"
#include "qdeclarativeposition_p.h"
#include "qdeclarative.h"
#include <qnmeapositioninfosource.h>
#include <QFile>
#include <QTimer>

QT_BEGIN_NAMESPACE

/*!
    \qmlclass PositionSource QDeclarativePositionSource
    \inqmlmodule QtLocation 5
    \ingroup qml-QtLocation5-positioning
    \since QtLocation 5.0

    \brief The PositionSource element provides the device's current position.

    The PositionSource element provides information about the user device's
    current position. The position is available as a \l{Position} element, which
    contains all the standard parameters typically available from GPS and other
    similar sytems, including longitude, latitude, speed and accuracy details.

    As different position sources are available on different platforms and
    devices, these are categorized by their basic type (Satellite, NonSatellite,
    and AllPositioningMethods). The available methods for the current platform
    can be enumerated in the \l{supportedPositioningMethods} property.

    To indicate which methods are suitable for your application, set the
    \l{preferredPositioningMethods} property. If the preferred methods are not
    available, the default source of location data for the platform will be
    chosen instead.

    The \l updateInterval property can then be used to indicate how often your
    application wishes to receive position updates. Once set, the \l{start}(),
    \l{stop}() and \l{update}() methods can be used to control the operation
    of the PositionSource, as well as the \l{active} property, which when set
    is equivalent to calling \l{start}() or \l{stop}().

    When the PositionSource is active, position updates can be retrieved
    either by simply using the \l{position} property in a binding (as the
    value of another item's property), or by providing a handler for the
    \l{onPositionChanged} signal.

    \section2 Example Usage

    The following example shows a simple PositionSource used to receive
    updates every second and print the longitude and latitude out to
    the console.

    \code
    PositionSource {
        id: src
        updateInterval: 1000
        active: true

        onPositionChanged: {
            var coord = src.position.coordinate;
            console.log("Coordinate:", coord.longitude, coord.latitude);
        }
    }
    \endcode

    The \l{declarative/flickr}{Flickr} example application shows how to use
    a PositionSource in your application to retrieve local data for users
    from a REST web service.

    \sa {QtLocation5::Position}, {QGeoPositionInfoSource}

*/

QDeclarativePositionSource::QDeclarativePositionSource()
    : m_positionSource(0), m_positioningMethod(QDeclarativePositionSource::NoPositioningMethod),
      m_nmeaFile(0), m_active(false), m_singleUpdate(false), m_updateInterval(0)
{
    m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_positionSource) {
        connect(m_positionSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(positionUpdateReceived(QGeoPositionInfo)));
        m_positioningMethod = supportedPositioningMethods();
#if defined(Q_OS_SYMBIAN)
    } else {
        RProcess thisProcess;
        if (!thisProcess.HasCapability(ECapabilityLocation)) {
            qmlInfo(this) << tr("PositionSource requires the Symbian Location capability to succeed on the Symbian platform.");
        }
#endif
    }
#ifdef QDECLARATIVE_POSITION_DEBUG
    if (m_positionSource)
        qDebug("QDeclarativePositionSource QGeoPositionInfoSource::createDefaultSource SUCCESS");
    else
        qDebug("QDeclarativePositionSource QGeoPositionInfoSource::createDefaultSource FAILURE");
#endif
}

QDeclarativePositionSource::~QDeclarativePositionSource()
{
    delete m_nmeaFile;
    delete m_positionSource;
}

void QDeclarativePositionSource::setNmeaSource(const QUrl& nmeaSource)
{
    // Strip the filename. This is clumsy but the file may be prefixed in several
    // ways: "file:///", "qrc:///", "/", "" in platform dependant manner.
    QString localFileName = nmeaSource.toString();
    if (!QFile::exists(localFileName)) {
        if (localFileName.startsWith("qrc:///")) {
            localFileName.remove(0, 7);
        } else if (localFileName.startsWith("file:///")) {
            localFileName.remove(0, 7);
        }
        if (!QFile::exists(localFileName) && localFileName.startsWith("/")) {
            localFileName.remove(0,1);
        }
    }
    if (m_nmeaFileName == localFileName)
        return;
    m_nmeaFileName = localFileName;
    m_nmeaSource = nmeaSource;
    // The current position source needs to be deleted
    // because QNmeaPositionInfoSource can be bound only to a one file.
    if (m_positionSource) {
        delete m_positionSource;
        m_positionSource = 0;
    }
    // Create the NMEA source based on the given data. QML has automatically set QUrl
    // type to point to correct path. If the file is not found, check if the file actually
    // was an embedded resource file.
    delete m_nmeaFile;
    m_nmeaFile = new QFile(localFileName);
    if (!m_nmeaFile->exists()) {
        localFileName.prepend(":");
        m_nmeaFile->setFileName(localFileName);
    }
    if (m_nmeaFile->exists()) {
#ifdef QDECLARATIVE_POSITION_DEBUG
        qDebug() << "QDeclarativePositionSource NMEA File was found: " << localFileName;
#endif
        m_positionSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::SimulationMode);
        (qobject_cast<QNmeaPositionInfoSource*>(m_positionSource))->setDevice(m_nmeaFile);
        connect(m_positionSource, SIGNAL(positionUpdated(QGeoPositionInfo)),
                this, SLOT(positionUpdateReceived(QGeoPositionInfo)));
        if (m_active && !m_singleUpdate) {
            // Keep on updating even though source changed
            QTimer::singleShot(0, this, SLOT(start()));
        }
    } else {
        qmlInfo(this) << tr("Nmea file not found.") << localFileName;
#ifdef QDECLARATIVE_POSITION_DEBUG
        qDebug() << "QDeclarativePositionSource NMEA File was not found: " << localFileName;
#endif
        if (m_active) {
            m_active = false;
            m_singleUpdate = false;
            emit activeChanged();
        }
    }
    if (m_positioningMethod != supportedPositioningMethods()) {
        m_positioningMethod = supportedPositioningMethods();
        emit supportedPositioningMethodsChanged();
    }
    emit this->nmeaSourceChanged();
}

void QDeclarativePositionSource::setUpdateInterval(int updateInterval)
{
    if (m_updateInterval == updateInterval)
        return;

    m_updateInterval = updateInterval;
    if (m_positionSource) {
        m_positionSource->setUpdateInterval(updateInterval);
    }
    emit updateIntervalChanged();
}

/*!
    \qmlproperty url PositionSource::nmeaSource

    This property holds the source for NMEA data (file). One purpose of this
    property is to be of development convenience.

    Setting this property will override any other position source. Currently only
    files local to the .qml -file are supported. Nmea source is created in simulation mode,
    meaning that the data and time information in the NMEA source data is used to provide
    positional updates at the rate at which the data was originally recorded.

    If nmeaSource has been set for a PositionSource element, there is no way to revert
    back to non-file sources.
*/

QUrl QDeclarativePositionSource::nmeaSource() const
{
    return m_nmeaSource;
}

/*!
    \qmlproperty bool PositionSource::updateInterval

    This property holds the desired interval between updates (milliseconds).

    \sa {QGeoPositionInfoSource::updateInterval()}

*/

int QDeclarativePositionSource::updateInterval() const
{
    return m_updateInterval;
}

void QDeclarativePositionSource::setPreferredPositioningMethods(QGeoPositionInfoSource::PositioningMethods methods)
{
    m_positionSource->setPreferredPositioningMethods(methods);
    emit preferredPositioningMethodsChanged();
}

/*!
    \qmlproperty enumeration PositionSource::supportedPositioningMethods

    This property holds the supported positioning methods of the
    current source.

    \list
    \o NoPositioningMethod - No positioning methods supported (no source).
    \o SatellitePositioningMethod - Satellite-based positioning methods such as GPS is supported.
    \o NonSatellitePositioningMethod - Non satellite methods are supported.
    \o AllPositioningMethods - Combination of methods are supported.
    \endlist

*/

QDeclarativePositionSource::PositioningMethods QDeclarativePositionSource::supportedPositioningMethods() const
{
    if (m_positionSource) {
        QGeoPositionInfoSource::PositioningMethods methods = m_positionSource->supportedPositioningMethods();
        if ( (methods & QGeoPositionInfoSource::AllPositioningMethods) == methods ) {
            return QDeclarativePositionSource::AllPositioningMethods;
        } else if (methods & QGeoPositionInfoSource::SatellitePositioningMethods) {
            return QDeclarativePositionSource::SatellitePositioningMethod;
        } else if (methods & QGeoPositionInfoSource::NonSatellitePositioningMethods) {
            return QDeclarativePositionSource::NonSatellitePositioningMethod;
        }
    }
    return QDeclarativePositionSource::NoPositioningMethod;
}

/*!
    \qmlproperty enumeration PositionSource::preferredPositioningMethods

    This property holds the supported positioning methods of the
    current source.

    \list
    \o SatellitePositioningMethod - Satellite-based positioning methods such as GPS is supported.
    \o NonSatellitePositioningMethod - Non satellite methods are supported.
    \o AllPositioningMethods - Combination of methods are supported.
    \endlist

*/

QDeclarativePositionSource::PositioningMethods QDeclarativePositionSource::preferredPositioningMethods() const
{
    if (m_positionSource) {
        QGeoPositionInfoSource::PositioningMethods methods = m_positionSource->preferredPositioningMethods();
        if ( (methods & QGeoPositionInfoSource::AllPositioningMethods) == methods) {
            return QDeclarativePositionSource::AllPositioningMethods;
        } else if (methods & QGeoPositionInfoSource::SatellitePositioningMethods) {
            return QDeclarativePositionSource::SatellitePositioningMethod;
        } else if (methods & QGeoPositionInfoSource::NonSatellitePositioningMethods) {
            return QDeclarativePositionSource::NonSatellitePositioningMethod;
        }
    }
    return QDeclarativePositionSource::NoPositioningMethod;
}

/*!
    \qmlmethod PositionSource::start()

    Requests updates from the location source.
    Uses \l updateInterval if set, default interval otherwise.
    If there is no source available, this method has no effect.

    \sa stop, update, active
*/

void QDeclarativePositionSource::start()
{
    if (m_positionSource) {
        // Safe to set, setting zero means using default value
        m_positionSource->setUpdateInterval(m_updateInterval);
        m_positionSource->startUpdates();
        if (!m_active) {
            m_active = true;
            emit activeChanged();
        }
    }
}

/*!
    \qmlmethod PositionSource::update()

    A convenience method to request single update from the location source.
    If there is no source available, this method has no effect.

    \sa start, stop, active
*/

void QDeclarativePositionSource::update()
{
    if (m_positionSource) {
        if (!m_active) {
            m_active = true;
            m_singleUpdate = true;
            emit activeChanged();
        }
        // Use default timeout value. Set active before calling the
        // update request because on some platforms there may
        // be results immediately.
        m_positionSource->requestUpdate();
    }
}

/*!
    \qmlmethod PositionSource::stop()

    Stops updates from the location source.
    If there is no source available or it is not active,
    this method has no effect.

    \sa start, update, active
*/

void QDeclarativePositionSource::stop()
{
    if (m_positionSource) {
        m_positionSource->stopUpdates();
        if (m_active) {
            m_active = false;
            emit activeChanged();
        }
    }
}

/*!
    \qmlsignal PositionSource::positionChanged()

    This signal is sent when a position update has been received
    from the location source. Upon receiving this signal, at least
    position::latitude, position::longitude, and position::timestamp
    members of the \l position have been updated.

    \sa updateInterval

*/

/*!
    \qmlproperty bool PositionSource::active

    This property indicates whether the position source is (/should be)
    active. Setting this property to false equals calling \l stop, and
    setting this property true equals calling \l start.

    \sa start, stop, update

*/

void QDeclarativePositionSource::setActive(bool active)
{
    if (active == m_active)
        return;

    if (active)
        QTimer::singleShot(0, this, SLOT(start())); // delay ensures all properties have been set
    else
        stop();
}

bool QDeclarativePositionSource::isActive() const
{
    return m_active;
}

/*!
    \qmlproperty Position PositionSource::position

    This property holds the last known positional data.

    The Position element has different positional member variables,
    whose validity can be checked with appropriate validity functions
    (e.g. sometimes an update does not have speed or altitude data).

    However, whenever a \l positionChanged signal has been received, at least
    position::coordinate::latitude, position::coordinate::longitude, and position::timestamp can
    be assumed to be valid.

    \sa start, stop, update

*/

QDeclarativePosition* QDeclarativePositionSource::position()
{
    return &m_position;
}

void QDeclarativePositionSource::positionUpdateReceived(const QGeoPositionInfo& update)
{
    if (update.isValid()) {
        m_position.setTimestamp(update.timestamp());
        m_position.setCoordinate(update.coordinate());
        if (update.hasAttribute(QGeoPositionInfo::GroundSpeed)) {
            m_position.setSpeed(update.attribute(QGeoPositionInfo::GroundSpeed));
        }
        if (update.hasAttribute(QGeoPositionInfo::HorizontalAccuracy)) {
            m_position.setHorizontalAccuracy(update.attribute(QGeoPositionInfo::HorizontalAccuracy));
        }
        if (update.hasAttribute(QGeoPositionInfo::VerticalAccuracy)) {
            m_position.setVerticalAccuracy(update.attribute(QGeoPositionInfo::VerticalAccuracy));
        }
        emit positionChanged();
    } else {
        m_position.invalidate();
    }
    if (m_singleUpdate && m_active) {
        m_active = false;
        m_singleUpdate = false;
        emit activeChanged();
    }
}

#include "moc_qdeclarativepositionsource_p.cpp"

QT_END_NAMESPACE
