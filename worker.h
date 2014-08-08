#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QTimer>

#ifdef  Q_OS_WIN32
#include <winsock2.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <icmpapi.h>
#endif

#include <zlib.h>
#include <GeoIP.h>

#include "ping.h"


class Worker : public QObject
{
    Q_OBJECT
public:
    static  int NUMBER_OF_PROXIES;

    Worker(const QStringList& __proxies);
signals:
    void    abortConnections();
    void    allProxiesFinished();
    void    dataReady(const QStringList&,const QColor&);
    void    finished();
public slots:
    void    doWork();

    void    slot_finished();
    void    slot_encrypted();
    void    slot_anonymous();

    void    slot_allProxiesFinished();
    void    slot_abortConnections();

private:
    QList<QNetworkAccessManager*>  proxies;

    /* Using QHash instead of simple QList because preventing duplicate inserting is easier */
    QHash<QString,QStringList>      connected;
    QHash<QString,QStringList>      encrypted;
    QHash<QString,QStringList>      anonymous;

    GeoIP*                      gi;
};

#endif // WORKER_H
