#ifndef WEMODEVICE_H
#define WEMODEVICE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QUuid>
#include <QAbstractSocket>

//*****************************************************************************
//*****************************************************************************
/**
 * @brief The WemoDevice class
 */
//*****************************************************************************
class WemoDevice : public QObject
{
    Q_OBJECT

public:

    //*** constructor ***
    explicit WemoDevice( QString name, quint16 port, QObject *parent = nullptr);

    //*** destructor ***
    ~WemoDevice();

    //*** sets the current state of the device ***
    void setCurrentState( bool state ) { state_ = state; }

    //*** return device info ***
    quint16 getPort() { return port_; }
    QString getName() { return deviceName_; }
    QString getUuid() { return uuid_; }


signals:

    //*** error messages ***
    void error( QString errStr );

    //*** general messages ***
    void msgOut( QString msgStr );

    //*** announce device state set by Alexa ***
    void setDeviceState( QString devName, bool state );


private slots:

    //*** called when there is a new TCP connection ***
    void newTcpConnection();

    //*** TCP socket error ***
    void clientError( QAbstractSocket::SocketError socketError );

    //*** TCP message received ***
    void clientDataAvailable();


private:

    //*** returns current time/date in correct format ***
    QString timeStr() { return QDateTime::currentDateTime().toString( Qt::RFC2822Date ); }

    //*** handlers for different TCP messages ***
    QString handleSetup();
    QString handleEvent();
    QString handleMetaInfo();
    QString handleAction( QString msgIn );

    //*** adds http header to body to create full message ***
    QByteArray createMsg( QString body );


    //*** name of this device ***
    QString deviceName_;

    //*** unique port for this device ***
    quint16 port_;

    //*** unique id for this device ***
    QString uuid_;

    //*** current state for this device ***
    bool state_;

    //*** holds address info for connected peer ***
    QHostAddress peerAddr_;
    quint16      peerPort_;

    //*** TCP server for the device ***
    QTcpServer *tcpServer_;
};

#endif // WEMODEVICE_H
