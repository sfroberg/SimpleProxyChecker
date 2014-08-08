#include "worker.h"

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QDebug>
#include <QColor>

#include <QRegularExpression>
#include <QRegularExpressionMatch>

int Worker::NUMBER_OF_PROXIES = 0;

/* Just for pretty coloring of the proxy list entries.
 * Ok proxies marked with green and not ok with red.
 * Orange reserved for "warning" status (not currently in use)
*/

static const    QColor  DarkOrange(0xf8, 0x80, 0x17);
static const    QColor  MediumSpringGreen(0x34,0x80,0x17);
static const    QColor  ChilliPepper(0xc1,0x1b,0x17);


Worker::Worker(const QStringList& __proxies):gi(NULL)
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif


    /* GeoIP initialization */
    this->gi = GeoIP_open("./GeoIP.dat",GEOIP_INDEX_CACHE | GEOIP_CHECK_CACHE);

    /*****************************************/
    /* GeoIP sanity checking                 */
    /*****************************************/
#ifndef NDEBUG
    if(gi == NULL) {
        qDebug() << "Error opening database!!!";
    }
    /* make sure GeoIP deals with invalid query gracefully */
    if (GeoIP_country_code_by_addr(this->gi, NULL) != NULL) {
        qDebug() <<  "Invalid Query test failed, got non NULL, expected NULL";
    }
    if (GeoIP_country_code_by_name(this->gi, NULL) != NULL) {
        qDebug() << "Invalid Query test failed, got non NULL, expected NULL";
    }

    /* If got this far just spit out the path of GeoIP.dat and everything
     * is ready to go */
    if(this->gi) {
        qDebug() << gi->file_path;
    }
#endif
    /* Read the proxies from list and setup corresponding QNetworkAccessManagers */
    register size_t  n = __proxies.count();
    for(register unsigned int i = 0;i < n;++i) {
        QStringList host = __proxies.at(i).split(QLatin1String(":"),QString::SkipEmptyParts);
        QNetworkAccessManager*  m = new QNetworkAccessManager(this);
        m->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy,host.at(0),host.at(1).toUInt()));
        this->proxies.append(m);
    }
}




void
Worker::doWork()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif

    Worker::NUMBER_OF_PROXIES = 0;

    connect(this,SIGNAL(allProxiesFinished()),this,SLOT(slot_allProxiesFinished()));

    /* Using QEventLoop to synchronize requests because QNetworkAccessManager does not
     * provide synchronize API. There really should be some better way .... */

    QEventLoop  loop;
    connect(this,SIGNAL(allProxiesFinished()),&loop,SLOT(quit()));

    /* Here we loop throught all the proxies that have been stored into
     * this->proxies list. Each entry in that list is an QNetworkAccessManager object
     * with it's proxy already set with setProxy() method.
     *
     * For each iteration, the following request are sent in this loop:
     * - Check if proxy works (without ssl or anonymous stuff) by trying to
     *   get a response to http method HEAD (we don't need the page content so HEAD should be enough)
     *   request to wikipedia.org.
     * - Check for SSL support of proxy by doing http method HEAD request to wikipedia.org.
     * - Check for anonymous support of proxy by doing http method HEAD request to goldenpirates.org
     *   and then parsing the results.
     * Note: This loop just send the requests away, the actual handling of results is done in
     * each corresponding slot: slot_finished(), slot_encrypted() and slot_anonymous().
     * Note 2: Note that proxy owners might be blocking access to wikipedia or just it's SSL version.
     * So the target URL must be changed in those cases. */

    register size_t  n = this->proxies.count();
    for(register unsigned int i = 0;i < n;++i) {

        /* Check for normal, ordinary non-ssl connection */
        QNetworkRequest req(QUrl(QLatin1String("http://en.wikipedia.org/wiki/Main_Page")));
        req.setPriority(QNetworkRequest::HighPriority);
        QNetworkReply*  r =  this->proxies.at(i)->head(req);
        connect(r,SIGNAL(finished()),this,SLOT(slot_finished()));
        connect(this,SIGNAL(abortConnections()),r,SLOT(abort()));

        QNetworkRequest req2(QUrl(QLatin1String("https://en.wikipedia.org/wiki/Main_Page")));
        req2.setPriority(QNetworkRequest::HighPriority);
        QNetworkReply*  ssl = this->proxies.at(i)->head(req2);
        connect(ssl,SIGNAL(finished()),this,SLOT(slot_encrypted()));
        connect(this,SIGNAL(abortConnections()),ssl,SLOT(abort()));

        QNetworkRequest req3(QUrl(QLatin1String("http://www.goldenpirates.org/proxy/azenv.php")));
        req3.setPriority(QNetworkRequest::HighPriority);
        QNetworkReply*  anonymous = this->proxies.at(i)->get(req3);
        connect(anonymous,SIGNAL(finished()),this,SLOT(slot_anonymous()));
        connect(this,SIGNAL(abortConnections()),anonymous,SLOT(abort()));

       if(Worker::NUMBER_OF_PROXIES >= this->proxies.count()) {
            break;
        }
    }

    loop.exec();
    emit finished();

}


void
Worker::slot_finished()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif

    QNetworkReply *r = qobject_cast<QNetworkReply*>(QObject::sender());
    QSharedPointer<QNetworkReply>  reply(r,&QObject::deleteLater);

    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString    proxy = reply->manager()->proxy().hostName() + QLatin1String(":") + QString::number(reply->manager()->proxy().port());

    QString    proxyHostname = reply->manager()->proxy().hostName();
    quint32    proxyPort = reply->manager()->proxy().port();

    QStringList    tmp;
    QStringList    data;

    /* This is just used to indicate if the connection was successful (1) or not (0) */
    if(reply->error() == QNetworkReply::NoError && httpStatusCode == 200) {
        tmp << "1";
    } else {
        tmp << "0";
    }

    if(httpStatusCode >= 100) {
        tmp <<  (QString::number(httpStatusCode) + QLatin1String(" ") + reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    } else {
       tmp <<  reply->errorString();
    }

    this->connected.insert(proxy,tmp);


    if(this->encrypted.contains(proxy) && this->anonymous.contains(proxy)) {
        // QStringList data order is following:
        // 0 = host
        // 1 = port
        // 2 = http status code
        // 3 = ping
        // 4 = country
        // 5 = ssl
        // 6 = anonymous

        data << proxyHostname << QString::number(proxyPort) << this->connected.value(proxy).at(1);

        QString    p;
#ifdef  Q_OS_WIN32
        PICMP_ECHO_REPLY  status = ping(proxyHostname);
        if(status != 0 && status->Status == IP_SUCCESS) {
            p = QString::number(status->RoundTripTime);
        }
#endif
#ifdef  Q_OS_LINUX
        int  status = ping(proxyHostname.toUtf8().constData());
        if(status > 0) {
            p = QString::number(status);
        }
#endif
        data << p;

        const char* returnedCountry = NULL;
        if(this->gi) {
            returnedCountry =  GeoIP_country_name_by_addr(this->gi,proxyHostname.toLatin1().constData());
        }
        data << returnedCountry;
        data << this->encrypted.value(proxy).at(1) << this->anonymous.value(proxy).at(1);

        QColor  color;
        if(this->connected.value(proxy).at(1).startsWith(QLatin1String("200 OK")) && this->encrypted.value(proxy).at(1) == QLatin1String("Yes") && (this->anonymous.value(proxy).at(1) == QLatin1String("High-anonymous") || this->anonymous.value(proxy).at(1) == QLatin1String("Anonymous")) ) {
            color = MediumSpringGreen;
        } else {
            color = ChilliPepper;
        }

        emit dataReady(data,color);

        Worker::NUMBER_OF_PROXIES++;

#ifndef NDEBUG
        qDebug() << "# OF PROXIES  " << Worker::NUMBER_OF_PROXIES;
#endif
        if(Worker::NUMBER_OF_PROXIES >= this->proxies.count()) {
            emit allProxiesFinished();
        }
    }

}

void
Worker::slot_encrypted()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif

    QNetworkReply *r = qobject_cast<QNetworkReply*>(QObject::sender());
    QSharedPointer<QNetworkReply>  reply(r,&QObject::deleteLater);

    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString    proxy = reply->manager()->proxy().hostName() + QLatin1String(":") + QString::number(reply->manager()->proxy().port());

    QString    proxyHostname = reply->manager()->proxy().hostName();
    quint32    proxyPort = reply->manager()->proxy().port();

    QStringList tmp;
    QStringList data;

    /* This is just used to indicate if the connection was successful (1) or not (0) */
    if(reply->error() == QNetworkReply::NoError && httpStatusCode == 200) {
        tmp << "1";
    } else {
        tmp << "0";
    }

    if(httpStatusCode >= 100) {
        if(httpStatusCode == 200) {
            tmp << "Yes";
        } else {
            tmp <<  (QString::number(httpStatusCode) + QLatin1String(" ") + reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        }
    } else {
               tmp <<  reply->errorString();
    }

    this->encrypted.insert(proxy,tmp);

    if(this->connected.contains(proxy) && this->anonymous.contains(proxy)) {
        // QStringList data order is following:
        // 0 = host
        // 1 = port
        // 2 = http status code
        // 3 = ping
        // 4 = country
        // 5 = ssl
        // 6 = anonymous
        data << proxyHostname << QString::number(proxyPort) << this->connected.value(proxy).at(1);

        QString    p;
#ifdef  Q_OS_WIN32
        PICMP_ECHO_REPLY  status = ping(proxyHostname);
        if(status != 0 && status->Status == IP_SUCCESS) {
            p = QString::number(status->RoundTripTime);
        }
#endif
#ifdef  Q_OS_LINUX
        int  status = ping(proxyHostname.toUtf8().constData());
        if(status > 0) {
            p = QString::number(status);
        }
#endif
        data << p;



        const char* returnedCountry = NULL;
        if(this->gi) {            
            returnedCountry =  GeoIP_country_name_by_addr(this->gi,proxyHostname.toLatin1().constData());
        }
        data << returnedCountry;
        data << this->encrypted.value(proxy).at(1) << this->anonymous.value(proxy).at(1);


        QColor  color;
        if(this->connected.value(proxy).at(1).startsWith(QLatin1String("200 OK")) && this->encrypted.value(proxy).at(1) == QLatin1String("Yes") && (this->anonymous.value(proxy).at(1) == QLatin1String("High-anonymous") || this->anonymous.value(proxy).at(1) == QLatin1String("Anonymous")) ) {
            color = MediumSpringGreen;
        } else {
            color = ChilliPepper;
        }

        emit dataReady(data,color);

        Worker::NUMBER_OF_PROXIES++;

#ifndef NDEBUG
        qDebug() << "# OF PROXIES  " << Worker::NUMBER_OF_PROXIES;
#endif
        if(Worker::NUMBER_OF_PROXIES >= this->proxies.count()) {
            emit allProxiesFinished();
        }

    }

}

/* This slot will try to determine the anonymity level of the proxy
 * by parsing the raw headers returned by http://www.goldenpirates.org/proxy/azenv.php
 * */
void
Worker::slot_anonymous()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif

    QNetworkReply *r = qobject_cast<QNetworkReply*>(QObject::sender());
    QSharedPointer<QNetworkReply>  reply(r,&QObject::deleteLater);

    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString    proxy = reply->manager()->proxy().hostName() + QLatin1String(":") + QString::number(reply->manager()->proxy().port());

    QString    proxyHostname = reply->manager()->proxy().hostName();
    quint32    proxyPort = reply->manager()->proxy().port();


    QStringList tmp;
    QStringList data;

    QString  rawData;

    /* This is just used to indicate if the connection was successful (1) or not (0) */
    if(reply->error() == QNetworkReply::NoError && httpStatusCode == 200) {
        tmp << "1";
        rawData = reply->readAll();
#ifndef NDEBUG
        qDebug() << rawData;
#endif
    } else {
        tmp << "0";
    }

    if(httpStatusCode >= 100) {

/* =====================================================*/
        QString anon = "No";

        QRegularExpression re(QLatin1String("REMOTE_ADDR = (.*)"));
        QRegularExpressionMatch match = re.match(rawData);
        if (match.hasMatch()) {
#ifndef NDEBUG
            qDebug() << "WHOLE MATCH " << match.captured(0);
            qDebug() << "EXACT MATCH " << match.captured(1);
#endif
            if(proxyHostname == match.captured(1)) {
                anon = "High-anonymous";
            }
        }

        QRegularExpression dangerous_proxy_headers(QLatin1String("X-Forwarded-For|HTTP_FORWARDED|HTTP_X_FORWARDED_FOR|HTTP_X_PROXY_ID|HTTP_VIA|CLIENT_IP|HTTP_FROM"));
        QRegularExpressionMatch match2 = dangerous_proxy_headers.match(rawData);
        if(match2.hasMatch()) {
            anon = "Anonymous";
        }
        tmp << anon;
    } else {
        tmp <<  reply->errorString();
    }



    this->anonymous.insert(proxy,tmp);


    if(this->connected.contains(proxy) && this->encrypted.contains(proxy)) {
        // QStringList data order is following:
        // 0 = host
        // 1 = port
        // 2 = http status code
        // 3 = ping
        // 4 = country
        // 5 = ssl
        // 6 = anonymous

        data << proxyHostname << QString::number(proxyPort);
        data << this->connected.value(proxy).at(1);

        QString    p;
    #ifdef  Q_OS_WIN32
        PICMP_ECHO_REPLY  status = ping(proxyHostname);
        if(status != 0 && status->Status == IP_SUCCESS) {
            p = QString::number(status->RoundTripTime);
        }
    #endif
    #ifdef  Q_OS_LINUX
        int  status = ping(proxyHostname.toUtf8().constData());
        if(status > 0) {
            p = QString::number(status);
        }
    #endif
        data << p;



        const char* returnedCountry = NULL;
        if(this->gi) {
            returnedCountry =  GeoIP_country_name_by_addr(this->gi,proxyHostname.toLatin1().constData());
        }
        data << returnedCountry;
        data << this->encrypted.value(proxy).at(1);

        if(this->anonymous.value(proxy).at(0) == QLatin1String("1")) {

            QString anon = "No";

            QRegularExpression re(QLatin1String("REMOTE_ADDR = (.*)"));
            QRegularExpressionMatch match = re.match(rawData);
            if (match.hasMatch()) {
#ifndef NDEBUG
                qDebug() << "WHOLE MATCH " << match.captured(0);
                qDebug() << "EXACT MATCH " << match.captured(1);
#endif
                if(proxyHostname == match.captured(1)) {
                    anon = "High-anonymous";
                }
            }

            QRegularExpression dangerous_proxy_headers(QLatin1String("X-Forwarded-For|HTTP_FORWARDED|HTTP_X_FORWARDED_FOR|HTTP_X_PROXY_ID|HTTP_VIA|CLIENT_IP|HTTP_FROM"));
            QRegularExpressionMatch match2 = dangerous_proxy_headers.match(rawData);
            if(match2.hasMatch()) {
                anon = "Anonymous";
            }

            data << anon;
        } else {
            data <<  this->anonymous.value(proxy).at(1);
        }


        QColor  color;

        if(this->connected.value(proxy).at(1).startsWith(QLatin1String("200 OK")) && this->encrypted.value(proxy).at(1) == QLatin1String("Yes") && (this->anonymous.value(proxy).at(1) == QLatin1String("High-anonymous") || this->anonymous.value(proxy).at(1) == QLatin1String("Anonymous")) ) {
            color = MediumSpringGreen;
        } else {
            color = ChilliPepper;
        }

        emit dataReady(data,color);


        Worker::NUMBER_OF_PROXIES++;

#ifndef NDEBUG
        qDebug() << "# OF PROXIES  " << Worker::NUMBER_OF_PROXIES;
#endif
        if(Worker::NUMBER_OF_PROXIES >= this->proxies.count()) {
            emit allProxiesFinished();
        }

    } else {

    }


}



void
Worker::slot_allProxiesFinished()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif
#ifndef NDEBUG
    qDebug() << "ALL " << Worker::NUMBER_OF_PROXIES << " PROXIES FINISHED";
#endif
}



void
Worker::slot_abortConnections()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif
    // Stop all
    Worker::NUMBER_OF_PROXIES = this->proxies.count();
    emit abortConnections();
    emit allProxiesFinished();
}
