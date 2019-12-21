#ifndef FAUXMOQT_H
#define FAUXMOQT_H

#pragma once

#include "FauxMoLib_global.h"

#include <QtCore>
#include <QtNetwork>
#include <QUdpSocket>
#include <QHash>
#include <QList>
#include <QNetworkInterface>
#include <QHostAddress>

#include "WemoDevice.h"

#include "FauxMo_Templates.h"

const QString FAUXMO_UDP_MULTICAST_IP   = "239.255.255.250";
const quint16 FAUXMO_UDP_MULTICAST_PORT = 1900;

const quint16 BASE_TCP_PORT             = 19125;


class FAUXMOLIB_EXPORT FauxMoQt : public QObject
{

    Q_OBJECT

public:

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief FauxMo - constructor
     */
    //*****************************************************************************
    FauxMoQt();

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief FauxMo - destructor
     */
    //*****************************************************************************
    ~FauxMoQt();

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief initialize called to initialize the object - call after connecting
     */
    //*****************************************************************************
    void initialize();

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief addDevice
     * @param devName
     * @return
     */
    //*****************************************************************************
    void addDevice( QString devName );

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief enableDiscovery
     * @param en
     */
    //*****************************************************************************
    void enableDiscovery( bool en ) { discoveryEnabled_ = en; }

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief setState
     * @param devName
     * @param state
     * @return
     */
    //*****************************************************************************
    bool setState( QString devName, bool state );


signals:

    void error( QString errStr );
    void msgOut( QString msgStr );

    void setDeviceState( QString devName, bool state );


private slots:

    //*****************************************************************************
    //*****************************************************************************
    /**
     * @brief readPendingDatagrams
     */
    //*****************************************************************************
    void readPendingDatagrams();

private:

    bool discoveryEnabled_;

    //*** TCP server port ***
    quint16 nextTcpPort_;

     //*** maps ***
    QHash<QString,WemoDevice*> nameToDevice_;

    QStringList patterns_;

    bool haveInterface_;
    QNetworkInterface netIF_;
    QHostAddress localAddress_;
    QString macStr_;

    QUdpSocket *udp_;

    bool setupNetworkInterface();

    void setupUDP();

    void sendUDPResponse( QHostAddress &addr, quint16 portIn, QString devName, QString pattern );

    QString timeStr() { return QDateTime::currentDateTime().toString( Qt::RFC2822Date ); }


};

#endif // FAUXMOQT_H
