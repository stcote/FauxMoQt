#include "WemoDevice.h"
#include "FauxMo_Templates.h"

#include <QTcpSocket>

//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::WemoDevice
 * @param name
 * @param port
 * @param parent
 */
//*****************************************************************************
WemoDevice::WemoDevice( QString name, quint16 port, QObject *parent )
    : QObject(parent),
      deviceName_(name),
      port_(port)
{
    //*** initialize state ***
    state_ = false;

    //*** create a new TCP server ***
    tcpServer_ = new QTcpServer( this );

    //*** create unique ID ***
    uuid_ = QUuid::createUuid().toString().remove("{").remove("}");

    //*** start listening ***
    if ( !tcpServer_->listen( QHostAddress::Any, port_ ) )
    {
        emit error( "Error listening on TCP port " + QString::number(port_) );
        return;
    }

    //*** connect to 'new client handler' ***
    connect( tcpServer_, SIGNAL(newConnection()), SLOT(newTcpConnection()) );
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::~WemoDevice
 */
//*****************************************************************************
WemoDevice::~WemoDevice()
{
    //*** close down TCP server ***
    delete tcpServer_;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::newTcpConnection
 */
//*****************************************************************************
void WemoDevice::newTcpConnection()
{
    //*** get socket for new connection ***
    QTcpSocket *clientSock = tcpServer_->nextPendingConnection();

    //*** delete socket on disconnect ***
    connect( clientSock, &QAbstractSocket::disconnected, clientSock, &QObject::deleteLater );

    //*** read socket data ***
    connect( clientSock, SIGNAL(readyRead()), SLOT(clientDataAvailable()) );

    //*** monitor errors ***
    connect( clientSock, SIGNAL(error(QAbstractSocket::SocketError)),
                         SLOT(clientError(QAbstractSocket::SocketError)) );
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::clientError
 * @param socketError
 */
//*****************************************************************************
void WemoDevice::clientError( QAbstractSocket::SocketError socketError )
{
    //*** don't worry about disconnects - they are expected ***
    if ( socketError != QAbstractSocket::RemoteHostClosedError )
    {
        //*** get client socket for this signal ***
        QTcpSocket *client = dynamic_cast<QTcpSocket*>(sender());

        //*** expose error ***
        QString errStr = "[" + deviceName_ + "] Client socket error: " + client->errorString();
        emit error( errStr );
    }
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::clientDataAvailable
 */
//*****************************************************************************
void WemoDevice::clientDataAvailable()
{
QString body;
const QString SetupStr    = "GET /setup.xml HTTP/1.1";
const QString EventStr    = "/eventservice.xml";
const QString MetaInfoStr = "/metainfoservice.xml";
const QString ActionStr   = "POST /upnp/control/basicevent1 HTTP/1.1";

    //*** get client socket ***
    QTcpSocket* sock = dynamic_cast<QTcpSocket*>( sender() );

    if ( !sock ) return;

    //*** save address and port of sender ***
    peerAddr_ = sock->peerAddress();
    peerPort_ = sock->peerPort();

    //*** make sure there's data ***
    qint64 bytesAvailable = sock->bytesAvailable();
    if ( bytesAvailable < 1 ) return;

    //*** read in all the data ***
    QByteArray allData = sock->readAll();

    //*** make sure we got all that was expected ***
    if ( bytesAvailable != allData.size() )
    {
        emit error( "[" + deviceName_ + "] Mismatch in client data read" );
        return;
    }

    //*** convert to a QString ***
    QString data = allData.data();

    //*** determine how to handle this message ***
    if ( data.contains( SetupStr ) )
        body = handleSetup();
    else if ( data.contains( EventStr ) )
        body = handleEvent();
    else if ( data.contains( MetaInfoStr ) )
        body = handleMetaInfo();
    else if ( data.contains( ActionStr ) )
        body = handleAction( data );
    else
    {
        emit msgOut( "[" + deviceName_ + "] Unknown TCP message received");
        return;
    }

    //*** if no body, then no response expected ***
    if ( body.isEmpty() ) return;

    //*** add the http header and create a full message ***
    QByteArray msgOut = createMsg( body );

    //*** send the message ***
    sock->write( msgOut );
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::handleSetup
 * @return - body of setup message
 */
//*****************************************************************************
QString WemoDevice::handleSetup()
{
QString body;

    //*** create from template ***
    body = QString( SETUP_XML ).arg( deviceName_ ).arg( uuid_ );

    return body;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::handleEvent
 * @return - body of event message
 */
//*****************************************************************************
QString WemoDevice::handleEvent()
{
QString body;

    //*** create from template ***
    body = EVENT_SERVICE_XML;

    return body;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::handleMetaInfo
 * @return
 */
//*****************************************************************************
QString WemoDevice::handleMetaInfo()
{
QString body;

    //*** create from template ***
    body = METAINFO_XML;

    return body;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::handleAction
 * @param msgIn
 * @return
 */
//*****************************************************************************
QString WemoDevice::handleAction( QString msgIn )
{
QString body;

    //*** handle 'get state' action ***
    if ( msgIn.contains( "GetBinaryState" ) )
    {
        //*** return current state as a 'soap' response ***
        body = QString( SOAP_RESPONSE ).arg("Get").arg("BinaryState").arg(state_ ? "1" : "0");
    }

    //*** handle 'set state' action ***
    else if ( msgIn.contains( "SetBinaryState" ) )
    {
        //*** display who is controlling us ***
        QString peer = peerAddr_.toString() + ":" + QString::number( peerPort_ );
        emit msgOut( "[" + deviceName_ + "] " + peer + " - SetBinaryState" );

        //*** off ***
        if ( msgIn.contains( "<BinaryState>0</BinaryState>") )
        {
            //*** set our current state ***
            state_ = false;

            //*** let parent program handle new state ***
            emit setDeviceState( deviceName_, false );
        }

        //*** on ***
        else if ( msgIn.contains( "<BinaryState>1</BinaryState>") )
        {
            //*** set our current state ***
            state_ = true;

            //*** let parent program handle new state ***
            emit setDeviceState( deviceName_, true );
        }

        //*** unknown ***
        else
        {
            emit error( "[" + deviceName_ + "] Invalid SetBinaryState msg" );
        }

        //*** create response from template ***
        body = QString( SOAP_RESPONSE ).arg("Set").arg("BinaryState").arg(state_ ? "1" : "0");
    }

    //*** handle 'get friendly name' action ***
    else if ( msgIn.contains( "GetFriendlyName") )
    {
        //*** create from template ***
        body = QString( SOAP_RESPONSE ).arg("Get").arg("FriendlyName").arg(deviceName_);
    }

    return body;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief WemoDevice::createMsg
 * @param body
 * @return
 */
//*****************************************************************************
QByteArray WemoDevice::createMsg( QString body )
{
    //*** convert body to utf8 ***
    QByteArray bodyUtf8 = body.toUtf8();

    //*** create header from template ***
    QString hdr = QString( HTTP_HEADER ).arg( bodyUtf8.size() ).arg( timeStr() );

    //*** combine header and body ***
    return hdr.toUtf8() + body.toUtf8();
}
