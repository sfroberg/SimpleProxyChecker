#ifndef PROXY_H
#define PROXY_H

#include <QMainWindow>

#include <QTreeWidgetItem>
#include <QWebPage>
#include <QNetworkProxy>

#ifdef  Q_OS_WIN32
#include <winsock2.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <icmpapi.h>
#endif

#include "ping.h"
#include "worker.h"

#include <zlib.h>
#include <GeoIP.h>


namespace Ui {
class Proxy;
}

class Proxy : public QMainWindow
{
    Q_OBJECT

public:
    static quint32 proxy_list_counter;      // Just a counter keeping track of number of proxy list processed so far
    static quint32 NUMBER_OF_PROXY_SITES;   // Total numer of proxy list that are to be fetched from Net
    static quint32 NUMBER_OF_PROXIES;       // Just a counter of individual proxies. Needed for progress bar

    explicit Proxy(QWidget *parent = 0);
    ~Proxy();
signals:
    void    readyForProxyChecking();
    void    allProxiesFinished();
    void    abortConnections();
public slots:

    /* Proxy fetchers */
    void                    coolproxy(bool);
    void                    samair(bool);
    void                    freeproxylist(bool);
    void                    free_proxy_list(bool);
    void                    xroxy(bool);
    void                    xroxy1(bool);
    void                    xroxy2(bool);
    void                    proxy_list(bool);

    void                    slot_allProxiesFinished();
    void                    dataReady(const QStringList& data,const QColor& color);
    void                    proxyListReady();
private slots:
    void on_GetProxies_pushButton_clicked();
    void on_ProxyFetcherChecker_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void on_ExportProxies_pushButton_clicked();
    void on_ImportProxies_pushButton_clicked();

    void on_CheckProxies_pushButton_clicked();
    void on_stopChecking_pushButton_clicked();

private:

    QSet<QString>                   proxyList;
    QList<QString>                  proxies;

    QWebPage*                    proxy_freeproxylists;
    QWebPage*                    proxy_free_proxy_list;
    QWebPage*                    proxy_xroxy;
    QWebPage*                    proxy_xroxy1;
    QWebPage*                    proxy_xroxy2;
    QWebPage*                    proxy_samair;
    QWebPage*                    proxy_coolproxy;
    QWebPage*                    proxy_proxy_list;


    Worker *                    worker;
    QThread*                    workerThread;

    Ui::Proxy *ui;
};

#endif // PROXY_H
