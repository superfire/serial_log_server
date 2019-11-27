/****************************************************************************
**
** Copyright (C) 2013 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "serialportreader.h"

#include <QCoreApplication>

#include <QDebug>
#include <QDateTime>
#include <QMutex>

SerialPortReader::SerialPortReader(QSerialPort *serialPort, QObject *parent) :
    QObject(parent),
    m_serialPort(serialPort),
    m_standardOutput(stdout)
{
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortReader::handleReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialPortReader::handleError);
    connect(&m_timer, &QTimer::timeout, this, &SerialPortReader::handleTimeout);
    connect(this, &SerialPortReader::logUpdate, this, &SerialPortReader::logSave);

    if(!m_LogDir.exists("log"))
        m_LogDir.mkdir("log");

    m_timer.start(5000);

    m_writeTimer.setInterval(1000);
    m_writeTimer.setSingleShot(true);
    connect(&m_writeTimer, &QTimer::timeout, this, &SerialPortReader::LogTimerout);
}

void SerialPortReader::handleReadyRead()
{
    QByteArray data;
    data = m_serialPort->readAll();

    switch( m_curRxCache ) {
    case RX_CACHE_1:
        m_rxCache_1.append(data);
        if( m_rxCache_1.size() >= MAX_CACHE )
        {
            m_logCache = m_rxCache_1;
            m_rxCache_1.clear();
            emit logUpdate(m_logCache);

            m_curRxCache = RX_CACHE_2;
        }
        break;
    case RX_CACHE_2:
        m_rxCache_2.append(data);
        if( m_rxCache_2.size() >= MAX_CACHE )
        {
            m_logCache = m_rxCache_2;
            m_rxCache_2.clear();
            emit logUpdate(m_logCache);

            m_curRxCache = RX_CACHE_1;
        }
        break;
    default:
        qDebug() << "cache err";
        break;
    }

    m_writeTimer.start();  // reset

    qDebug() << "rec data:" << data;

    if (!m_timer.isActive())
        m_timer.start(5000);
    else
        m_timer.start();  // reset
}

void SerialPortReader::handleTimeout()
{
    m_standardOutput << QObject::tr("No data was currently available "
                                    "for reading from port %1")
                        .arg(m_serialPort->portName())
                     << endl;

    m_timer.start(5000);
}

void SerialPortReader::handleError(QSerialPort::SerialPortError serialPortError)
{
    if (serialPortError == QSerialPort::ReadError) {
        m_standardOutput << QObject::tr("An I/O error occurred while reading "
                                        "the data from port %1, error: %2")
                            .arg(m_serialPort->portName())
                            .arg(m_serialPort->errorString())
                         << endl;
        QCoreApplication::exit(1);
    }
}

void SerialPortReader::logSave(QByteArray &data)
{
    static QMutex mutex;
    QString mmsg = QString::fromLocal8Bit(data);

    mutex.lock();

//    QString time=QDateTime::currentDateTime().toString(QString("yyyy-MM-dd HH:mm:ss:zzz"));
    QString file_time=QDateTime::currentDateTime().toString(QString("yyyy-MM-dd"));
    QString msg_time=QDateTime::currentDateTime().toString(QString("[yyyy-MM-dd_HH:mm:ss:zzz] "));

    QString filepath;
            filepath.append("log/log_");
            filepath.append(file_time);
            filepath.append(".log");
    QFileInfo fi(filepath);
    if( !fi.exists())
        fi.created();

    QFile file(filepath);
    bool isOpen = file.open(QIODevice::ReadWrite | QIODevice::Append);
    if(! isOpen)
        qDebug() << "open failed";

    QTextStream stream(&file);
    stream << "\r\n\r\n" << msg_time << mmsg;
//    stream  << mmsg;
    file.flush();
    file.close();

    mutex.unlock();
}

void SerialPortReader::LogTimerout()
{
    qDebug() << "logTimerout";

    switch( m_curRxCache ) {
    case RX_CACHE_1:
        m_logCache = m_rxCache_1;
        m_rxCache_1.clear();
        emit logUpdate(m_logCache);

        m_curRxCache = RX_CACHE_2;
        break;
    case RX_CACHE_2:
        m_logCache = m_rxCache_2;
        m_rxCache_2.clear();
        emit logUpdate(m_logCache);

        m_curRxCache = RX_CACHE_1;
        break;
    default:
        qDebug() << "cache err";
        break;
    }
}

