/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QNMEAPOSITIONINFOSOURCE_P_H
#define QNMEAPOSITIONINFOSOURCE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qnmeapositioninfosource.h"
#include "qgeopositioninfo.h"

#include <QObject>
#include <QQueue>
#include <QPointer>
#include <QtCore/qtimer.h>
#include <QtCore/private/qglobal_p.h>

QT_BEGIN_NAMESPACE

class QBasicTimer;
class QTimerEvent;
class QTimer;

class QNmeaReader;
struct QPendingGeoPositionInfo
{
    QGeoPositionInfo info;
    bool hasFix;
};


class QNmeaPositionInfoSourcePrivate : public QObject
{
    Q_OBJECT
public:
    QNmeaPositionInfoSourcePrivate(QNmeaPositionInfoSource *parent, QNmeaPositionInfoSource::UpdateMode updateMode);
    ~QNmeaPositionInfoSourcePrivate();

    void startUpdates();
    void stopUpdates();
    void requestUpdate(int msec);

    bool parsePosInfoFromNmeaData(const char *data,
                                  int size,
                                  QGeoPositionInfo *posInfo,
                                  bool *hasFix);

    void notifyNewUpdate(QGeoPositionInfo *update, bool fixStatus);

    QNmeaPositionInfoSource::UpdateMode m_updateMode;
    QPointer<QIODevice> m_device;
    QGeoPositionInfo m_lastUpdate;
    bool m_invokedStart;
    QGeoPositionInfoSource::Error m_positionError;
    double m_userEquivalentRangeError;

public Q_SLOTS:
    void readyRead();

protected:
    void timerEvent(QTimerEvent *event) override;

private Q_SLOTS:
    void emitPendingUpdate();
    void sourceDataClosed();
    void updateRequestTimeout();

private:
    bool openSourceDevice();
    bool initialize();
    void prepareSourceDevice();
    void emitUpdated(const QGeoPositionInfo &update);

    QNmeaPositionInfoSource *m_source;
    QNmeaReader *m_nmeaReader;
    QGeoPositionInfo m_pendingUpdate;
    QDate m_currentDate;
    QBasicTimer *m_updateTimer; // the timer used in startUpdates()
    QTimer *m_requestTimer; // the timer used in requestUpdate()
    qreal m_horizontalAccuracy;
    qreal m_verticalAccuracy;
    bool m_noUpdateLastInterval;
    bool m_updateTimeoutSent;
    bool m_connectedReadyRead;
};


class QNmeaReader
{
public:
    explicit QNmeaReader(QNmeaPositionInfoSourcePrivate *sourcePrivate)
            : m_proxy(sourcePrivate) {}
    virtual ~QNmeaReader() {}

    virtual void readAvailableData() = 0;

protected:
    QNmeaPositionInfoSourcePrivate *m_proxy;
};


class QNmeaRealTimeReader : public QNmeaReader
{
public:
    explicit QNmeaRealTimeReader(QNmeaPositionInfoSourcePrivate *sourcePrivate);
    void readAvailableData() override;
    void notifyNewUpdate();

    // Data members
    QGeoPositionInfo m_update;
    QDateTime m_lastPushedTS;
    bool m_updateParsed = false;
    bool m_hasFix = false;
    QTimer m_timer;
    int m_pushDelay = -1;
};


class QNmeaSimulatedReader : public QObject, public QNmeaReader
{
    Q_OBJECT
public:
    explicit QNmeaSimulatedReader(QNmeaPositionInfoSourcePrivate *sourcePrivate);
    ~QNmeaSimulatedReader();
    void readAvailableData() override;

protected:
    void timerEvent(QTimerEvent *event) override;

private Q_SLOTS:
    void simulatePendingUpdate();

private:
    bool setFirstDateTime();
    void processNextSentence();

    QQueue<QPendingGeoPositionInfo> m_pendingUpdates;
    QByteArray m_nextLine;
    int m_currTimerId;
    bool m_hasValidDateTime;
};

QT_END_NAMESPACE

#endif
