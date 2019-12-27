#include "FauxMoQt.h"
#include "FauxMo_Templates.h"

#include <QDebug>

//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::FauxMoQt - Constructor
 */
//*****************************************************************************
FauxMoQt::FauxMoQt() : QObject()
{
    //*** initialize vars ***
    haveInterface_ = false;
    discoveryEnabled_ = false;

    //*** set up TCP port ***
    nextTcpPort_   = BASE_TCP_PORT;

    //*** message patterns to respond to ***
    patterns_ << "ST: urn:Belkin:device:controllee:1";
    patterns_ << "ST: upnp:rootdevice";
    patterns_ << "ST: ssdp:all";
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::~FauxMoQt - destructor
 */
//*****************************************************************************
FauxMoQt::~FauxMoQt()
{
    if ( udp_ )
    {
        qDebug() << "[UDP] leaving multicast group";
        udp_->leaveMulticastGroup( QHostAddress( FAUXMO_UDP_MULTICAST_IP ) );
        delete udp_;
    }

    //*** deletes all devices ***
    qDeleteAll( nameToDevice_ );
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief initialize called to initialize the object - call after connecting
 */
//*****************************************************************************
void FauxMoQt::initialize()
{
    setupNetworkInterface();

    setupUDP();
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::setupNetworkInterface
 * @return
 */
//*****************************************************************************
bool FauxMoQt::setupNetworkInterface()
{
QNetworkInterface::InterfaceFlags flags;
bool isUp       = false;
bool isRunning  = false;
bool isLoopback = false;

    if ( haveInterface_ ) return true;

    emit msgOut( "Setting up network interface" );
    emit msgOut( "" );

    //*** get list of interfaces ***
    QList<QNetworkInterface> ifs = QNetworkInterface::allInterfaces();

    //*** check each interface ***
    foreach( auto ni, ifs )
    {
        //*** get the flags for the interface ***
        flags = ni.flags();

        isUp       = flags & QNetworkInterface::IsUp;
        isRunning  = flags & QNetworkInterface::IsRunning;
        isLoopback = flags & QNetworkInterface::IsLoopBack;

        //*** look for one in use ***
        if ( isUp && isRunning && !isLoopback )
        {
            haveInterface_ = true;      // we have a winner
            netIF_ = ni;                // save the interface

            //*** get IPV4 address ***
            QList<QNetworkAddressEntry> entries = ni.addressEntries();
            foreach( auto entry, entries )
            {
                //*** we only want the IPV4 address ***
                if ( entry.ip().protocol() == QAbstractSocket::IPv4Protocol )
                {
                    //*** save the IPV4 address ***
                    localAddress_ = entry.ip();
                }
            }

            //*** mac address ***
            macStr_ = netIF_.hardwareAddress().toLower().remove( ":" );

            //*** create uuid using mac ***
            QString msgTxt = QString("IF: %1  localIP: %2  mac: %3\n")
                          .arg(netIF_.humanReadableName())
                          .arg(localAddress_.toString())
                          .arg( macStr_ );
            emit msgOut( msgTxt );
            emit msgOut( "" );

            break;
        }
    }

    if ( !haveInterface_ )
        emit error( "No valid interface found" );

    return haveInterface_;
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::setupUDP
 */
//*****************************************************************************
void FauxMoQt::setupUDP()
{
bool ok = true;

    //*** must have found a valid interface ***
    if ( haveInterface_ )
    {
        //*** create the UDP socket ***
        udp_ = new QUdpSocket( this );

        //*** multicast ***
        QHostAddress multicastAddr( FAUXMO_UDP_MULTICAST_IP );

        //*** bind to a port ***
        if ( !udp_->bind( QHostAddress::AnyIPv4, FAUXMO_UDP_MULTICAST_PORT, QUdpSocket::ShareAddress ) )
        {
            ok = false;
            emit error( "[UDP] Error binding to port" );
        }
        else
        {
            qDebug() << "[UDP] Bound to port";
        }

        //*** set the interface to use ***
        udp_->setMulticastInterface( netIF_ );

        //*** join the multicast group ***
        if ( ok && !udp_->joinMulticastGroup( multicastAddr, netIF_ ) )
        {
            emit error( "[UDP] Error joining multicast group" );
            ok = false;
        }
        else
        {
            qDebug() << "[UDP] Joined multicast group : " << udp_->multicastInterface();
        }

        //*** connect to data packets received ***
        if ( ok )
        {
            connect( udp_, SIGNAL(readyRead()), SLOT(readPendingDatagrams()) );
        }
    }
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::readPendingDatagrams
 */
//*****************************************************************************
void FauxMoQt::readPendingDatagrams()
{
QHostAddress sender;
quint16 senderPort = 0;
QString pattern;

    //*** process all datagrams ***
    while( udp_->hasPendingDatagrams() )
    {
        //*** allocate buffer to hold the message ***
        QByteArray datagram;
        datagram.resize( udp_->pendingDatagramSize() );

        //*** read the packet ***
        udp_->readDatagram( datagram.data(), datagram.size(), &sender, &senderPort );

        //*** convert to a QString ***
        QString data = datagram.data();

        //*** determine if it's a message we want to respond to ***
        if ( discoveryEnabled_ && data.contains( "M-SEARCH" ) )
        {
            //*** check for any valid pattern ***
            foreach( auto p, patterns_ )
            {
                if ( data.contains( p ) )
                {
                    pattern = p;
                    break;
                }
            }

            //*** if pattern was found ***
            if ( !pattern.isNull() )
            {
                //*** send response for each device ***
                QStringList devices = nameToDevice_.keys();
                foreach( auto d, devices )
                {
                    sendUDPResponse( sender, senderPort, d, pattern );
                }
            }
        }
    }
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::sendUDPResponse
 */
//*****************************************************************************
void FauxMoQt::sendUDPResponse( QHostAddress &addr, quint16 portIn, QString devName, QString pattern )
{
QString response;

    //*** must have ethernet info ***
    if ( !haveInterface_ ) return;

    //*** get pointer to device ***
    WemoDevice* device = nameToDevice_[devName];

    //*** get port for this device ***
    quint16 myPort = device->getPort();

    QString uuid = device->getUuid();

    QString pattern2 = pattern.remove( "ST: " );

    //*** location of setup file ***
    QString point = localAddress_.toString() + ":" + QString::number( myPort );

    //*** create the response ***
    response = QString(UDP_RESPONSE_TEMPLATE)
            .arg(timeStr()).arg(point).arg(uuid).arg(pattern).arg(pattern2);

    //*** convert to utf8 ***
    QByteArray inUtf8 = response.toUtf8();
    const char *responseUtf8 = inUtf8.constData();

    //*** send the response ***
    udp_->writeDatagram( responseUtf8, strlen(responseUtf8), addr, portIn );
}



//*****************************************************************************
//*****************************************************************************
/**
 * @brief FauxMoQt::addDevice
 * @param devName
 */
//*****************************************************************************
void FauxMoQt::addDevice( QString devName )
{
    //*** check if already exists ***
    if ( nameToDevice_.contains( devName ) ) return;

    //*** create a new object ***
    WemoDevice* newDev = new WemoDevice( devName, nextTcpPort_++, this );
    nameToDevice_[devName] = newDev;

    //*** propagate signals ***
    connect( newDev, SIGNAL(setDeviceState(QString,bool)), SIGNAL(setDeviceState(QString,bool)) );

    connect( newDev, SIGNAL(error(QString)),  SIGNAL(error(QString))  );
    connect( newDev, SIGNAL(msgOut(QString)), SIGNAL(msgOut(QString)) );
}


//*****************************************************************************
//*****************************************************************************
/**
 * @brief setState
 * @param devName
 * @param state
 * @return
 */
//*****************************************************************************
bool FauxMoQt::setState( QString devName, bool state )
{
    if ( nameToDevice_.contains( devName ) )
    {
        nameToDevice_[devName]->setCurrentState( state );
        return true;
    }

    return false;
}

