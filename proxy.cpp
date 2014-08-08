#include "proxy.h"
#include "ui_proxy.h"

#include <QWebElement>
#include <QWebElementCollection>
#include <QWebFrame>
#include <QFileDialog>
#include <QFile>
#include <QThread>

quint32 Proxy::proxy_list_counter = 0;      // Just a counter keeping track of number of proxy list processed so far
quint32 Proxy::NUMBER_OF_PROXY_SITES = 8;   // Total numer of proxy list that are to be fetched from Net
/* The eight currently fetched proxy sites/pages are:
 *
 * http://www.freeproxylists.net/?pr=HTTPS&a[]=2&s=rs
 * http://www.samair.ru/proxy/proxy-01.htm
 * http://www.cool-proxy.net/proxies/http_proxy_list/sort:working_average/direction:desc/country_code:/port:/anonymous:1
 * http://free-proxy-list.net/
 * http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000#table
 * http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000&sort=reliability&desc=true&pnum=1#table
 * http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000&sort=reliability&desc=true&pnum=2#table
 * http://proxy-list.org/english/search.php?country=any&type=anonymous-and-elite&port=any&ssl=yes&p=1
 *
 * As you can see xroxy.com is fetched three times for each separate page while for other's it's just the first page.
 * This needs to be fixed: Try to make generic proxy fetcher that can handle multipages
*/
quint32 Proxy::NUMBER_OF_PROXIES = 0;       // Just a counter of individual proxies. Needed for progress bar




Proxy::Proxy(QWidget *parent) :
    QMainWindow(parent),worker(NULL),workerThread(NULL),
    ui(new Ui::Proxy)
{
    ui->setupUi(this);

    /* At the very beginning, hide the fetch & check progressbars and controls */
    this->ui->ProxyFetching_label->setVisible(false);
    this->ui->ProxyFetching_progressBar->setVisible(false);
    this->ui->ProxyFetching_progressBar->setMaximum(Proxy::NUMBER_OF_PROXY_SITES);

    this->ui->ProxyChecking_label->setVisible(false);
    this->ui->ProxyChecking_progressBar->setVisible(false);
    this->ui->stopChecking_pushButton->setVisible(false);


    /* Initialize the proxy fetchers.
     * FIXME: Is there a better way? */
    this->proxy_freeproxylists = new QWebPage(this);
    this->proxy_free_proxy_list = new QWebPage(this);
    this->proxy_xroxy = new QWebPage(this);
    this->proxy_xroxy1 = new QWebPage(this);
    this->proxy_xroxy2 = new QWebPage(this);
    this->proxy_samair = new QWebPage(this);
    this->proxy_coolproxy = new QWebPage(this);
    this->proxy_proxy_list = new QWebPage(this);


    /* Connect the signals & slots */
    connect(this->proxy_freeproxylists->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(freeproxylist(bool)));
    connect(this->proxy_samair->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(samair(bool)));
    connect(this->proxy_coolproxy->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(coolproxy(bool)));
    connect(this->proxy_free_proxy_list->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(free_proxy_list(bool)));
    connect(this->proxy_xroxy->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(xroxy(bool)));
    connect(this->proxy_xroxy1->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(xroxy1(bool)));
    connect(this->proxy_xroxy2->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(xroxy2(bool)));
    connect(this->proxy_proxy_list->mainFrame(),SIGNAL(loadFinished(bool)),this,SLOT(proxy_list(bool)));


    connect(this,SIGNAL(allProxiesFinished()),this,SLOT(slot_allProxiesFinished()));
    connect(this,SIGNAL(readyForProxyChecking()),this,SLOT(proxyListReady()));

}

Proxy::~Proxy()
{
    delete ui;
}

/* =================================================================== */
/* Proxy fetchers */
/* FIXME: There should be a better, general way */
/* =================================================================== */

void
Proxy::coolproxy(bool ok)
{
#ifndef NDEBUG
    qDebug() << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_coolproxy->mainFrame()->documentElement();
        QWebElementCollection rows = doc.findAll(QLatin1String("tr"));
        QString text;

        /* Skip the first row that contains header and start scraping */
        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));

            if(!cols.at(0).toPlainText().isEmpty()) {
                text = cols.at(0).toPlainText().trimmed() + QLatin1String(":") + cols.at(1).toPlainText().trimmed();

                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }
}


void
Proxy::samair(bool ok)
{
#ifndef NDEBUG
    qDebug() << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_samair->mainFrame()->documentElement();
        QWebElement table = doc.findFirst(QLatin1String("table#proxylist"));
        QWebElementCollection rows = table.findAll(QLatin1String("tr"));
        QString text;

        /* Skip the first row that contains header and start scraping */
        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));

            if(!cols.at(0).toPlainText().isEmpty() && !cols.at(1).toPlainText().isEmpty() && cols.at(1).toPlainText() != QLatin1String("transparent")) {
                text = cols.at(0).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {

        emit readyForProxyChecking();
    }
}

void
Proxy::proxy_list(bool ok)
{
#ifndef NDEBUG
    qDebug() << "Called " << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_proxy_list->mainFrame()->documentElement();
        QWebElement table = doc.findFirst(QLatin1String("div#proxy-table"));
        QWebElementCollection rows = table.findAll(QLatin1String("li.proxy"));
        QString text;

        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            if(!rows.at(i).toPlainText().isEmpty()) {
                text = rows.at(i).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }

}

/* www.freeproxylists.net uses alternating rows table (Even,Odd,Even,Odd, etc....) */
void
Proxy::freeproxylist(bool ok)
{
#ifndef NDEBUG
    qDebug() << "Called " << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_freeproxylists->mainFrame()->documentElement();
        QWebElementCollection rows = doc.findAll(QLatin1String("tr.Even"));
        QString text;

        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(0).toPlainText().isEmpty()) {
                text = cols.at(0).toPlainText().trimmed() + QLatin1String(":") + cols.at(1).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
        rows = doc.findAll(QLatin1String("tr.Odd"));
        register size_t n2 = rows.count();
        for(register unsigned int i = 0;i < n2;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(0).toPlainText().isEmpty()) {
                text = cols.at(0).toPlainText().trimmed() + QLatin1String(":") + cols.at(1).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);

            }
        }
    }


    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }

}

/* ========================================================= */
/* www.xroxy.com uses alternating rows table (row0,row1,row0,row1 etc...) */
/* ========================================================= */
void
Proxy::xroxy(bool ok)
{
#ifndef NDEBUG
    qDebug() << "Called " << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_xroxy->mainFrame()->documentElement();
        QWebElementCollection rows = doc.findAll(QLatin1String("tr.row0"));
        QString text;

        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
        rows = doc.findAll(QLatin1String("tr.row1"));
        register size_t n2 = rows.count();
        for(register unsigned int i = 0;i < n2;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }
}

/* ========================================================= */
/* Yes, another www.xroxy.com fetcher but this time for page two.
 * FIXME: Try to automatically handle multipage proxy sites
 * and their fetching */
/* ========================================================= */

void
Proxy::xroxy1(bool ok)
{
#ifndef NDEBUG
    qDebug() << "Called " << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_xroxy1->mainFrame()->documentElement();
        QWebElementCollection rows = doc.findAll(QLatin1String("tr.row0"));
        QString text;

        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
        rows = doc.findAll(QLatin1String("tr.row1"));
        register size_t n2 = rows.count();
        for(register unsigned int i = 0;i < n2;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif
    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }
}

/* Yet another xroxy.com fetcher for page three */
void
Proxy::xroxy2(bool ok)
{
#ifndef NDEBUG
    qDebug() << "Called " << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_xroxy2->mainFrame()->documentElement();
        QWebElementCollection rows = doc.findAll(QLatin1String("tr.row0"));
        QString text;

        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();

                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
        rows = doc.findAll(QLatin1String("tr.row1"));
        register size_t n2 = rows.count();
        for(register unsigned int i = 0;i < n2;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));
            if(!cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(1).toPlainText().trimmed() + QLatin1String(":") + cols.at(2).toPlainText().trimmed();

                if(!Proxy::proxyList.contains(text))
                    Proxy::proxyList.insert(text);
            }
        }
    }

    ++Proxy::proxy_list_counter;
    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif
    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }
}





/* http://free-proxy-list.net fetcher */
void
Proxy::free_proxy_list(bool ok)
{

#ifndef NDEBUG
    qDebug() << __FUNCTION__;
#endif
    /* Fetching was successfull, parse results */
    if(ok) {
        QWebElement doc = this->proxy_free_proxy_list->mainFrame()->documentElement();
        QWebElement table = doc.findFirst(QLatin1String("table"));
        QWebElementCollection rows = table.findAll(QLatin1String("tr"));
        QString text;

        /* Skip the first row that contains header and start scraping */
        register size_t n = rows.count();
        for(register unsigned int i = 1;i < n;++i) {
            QWebElementCollection   cols = rows.at(i).findAll(QLatin1String("td"));

            if(!cols.at(0).toPlainText().isEmpty() && !cols.at(1).toPlainText().isEmpty()) {
                text = cols.at(0).toPlainText().trimmed() + QLatin1String(":") + cols.at(1).toPlainText().trimmed();
                if(!Proxy::proxyList.contains(text)) {
                    Proxy::proxyList.insert(text);
                }
            }
        }
    }

    ++Proxy::proxy_list_counter;

    this->ui->ProxyFetching_progressBar->setValue(Proxy::proxy_list_counter);

#ifndef NDEBUG
    qDebug() << "Proxy::proxy_list_counter " << Proxy::proxy_list_counter << "/" << this->ui->ProxyFetching_progressBar->maximum();
#endif

    /* We start proxy checking when all fetchers have finished fetching (successfull or not) */
    if(Proxy::proxy_list_counter >= Proxy::NUMBER_OF_PROXY_SITES) {
        emit readyForProxyChecking();
    }
}

void
Proxy::dataReady(const QStringList& data,const QColor& color)
{
#ifndef NDEBUG
    qDebug() << "Called data ready ... " << data;
#endif

    this->ui->ProxyChecking_progressBar->setValue(++Proxy::NUMBER_OF_PROXIES);

    /* When this slot has finished the QStringList data will  contain the following: */
    /*
     * Host	Port	HTTP Status	Latency (in ms)		Country		SSL	Anonymous
    -------------------------------------------------------------------------------------------------------
    */
/* ============================================== */
    QString ip = data.at(0).trimmed();
    QString port = data.at(1).trimmed();
    QString httpStatus = data.at(2).trimmed();
    QString latency = data.at(3).trimmed();
    QString country = data.at(4).trimmed();
    QString ssl = data.at(5).trimmed();
    QString anonymous = data.at(6).trimmed();



    QTreeWidgetItem*    item = new QTreeWidgetItem(this->ui->ProxyFetcherChecker_treeWidget);
    item->setText(0,ip);
    item->setText(1,port);
    item->setText(2,httpStatus);
    item->setText(3,latency);
    item->setText(4,country);
    item->setText(5,ssl);
    item->setText(6,anonymous);

/* ============================================== */
    register size_t columns = this->ui->ProxyFetcherChecker_treeWidget->columnCount();
    for(register unsigned int i = 0;i < columns;++i) {
        item->setTextColor(i,color);
    }
}


/* Proxies have now been fetched. Show them */
void
Proxy::proxyListReady()
{
    /* Proxy list is now fetched, hide the proxy fetcher controls */
    this->ui->ProxyFetching_label->setVisible(false);
    this->ui->ProxyFetching_progressBar->setVisible(false);
    this->ui->ProxyFetching_progressBar->setValue(0);

    this->ui->ProxyFetcherChecker_treeWidget->clear();


    if(Proxy::proxyList.count() > 0) {
        QSet<QString>::const_iterator     i = Proxy::proxyList.constBegin();
        while (i != Proxy::proxyList.constEnd()) {
#ifndef NDEBUG
            qDebug() << *i;
#endif
            QStringList host = i->split(QLatin1String(":"),QString::SkipEmptyParts);
            QTreeWidgetItem*    item = new QTreeWidgetItem(this->ui->ProxyFetcherChecker_treeWidget,host);
            ++i;
        }
        /* Internal proxy structure used when doing proxy checking */
        this->proxies = Proxy::proxyList.toList();
    }
}



/* Start proxy fetcher */
void Proxy::on_GetProxies_pushButton_clicked()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif


    this->ui->ProxyFetching_label->setVisible(true);
    this->ui->ProxyFetching_progressBar->setVisible(true);
    this->ui->ProxyFetching_progressBar->setValue(0);


    Proxy::proxy_list_counter = 0;
#ifndef NDEBUG
    qDebug() << "==================================";
    qDebug() << Proxy::proxy_list_counter;
    qDebug() << "==================================";
#endif

    /* Fire the page requests to proxy sites away ... */
    this->proxy_freeproxylists->mainFrame()->load(QUrl(QLatin1String("http://www.freeproxylists.net/?pr=HTTPS&a[]=2&s=rs")));
    this->proxy_samair->mainFrame()->load(QUrl(QLatin1String("http://www.samair.ru/proxy/proxy-01.htm")));
    this->proxy_coolproxy->mainFrame()->load(QUrl(QLatin1String("http://www.cool-proxy.net/proxies/http_proxy_list/sort:working_average/direction:desc/country_code:/port:/anonymous:1")));
    this->proxy_free_proxy_list->mainFrame()->load(QUrl(QLatin1String("http://free-proxy-list.net/")));
    this->proxy_xroxy->mainFrame()->load(QUrl(QLatin1String("http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000#table")));
    this->proxy_xroxy1->mainFrame()->load(QUrl(QLatin1String("http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000&sort=reliability&desc=true&pnum=1#table")));
    this->proxy_xroxy2->mainFrame()->load(QUrl(QLatin1String("http://www.xroxy.com/proxylist.php?port=&type=Anonymous&ssl=ssl&country=&latency=&reliability=9000&sort=reliability&desc=true&pnum=2#table")));
    this->proxy_proxy_list->mainFrame()->load(QUrl(QLatin1String("http://proxy-list.org/english/search.php?country=any&type=anonymous-and-elite&port=any&ssl=yes&p=1")));

}

/* Just a place holder slot here for debugging purposes if needed.
 * */
void
Proxy::on_ProxyFetcherChecker_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif
    if(current) {
    }
}

/* Save proxies to file. Format of each line is IP:PORT */
void
Proxy::on_ExportProxies_pushButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save File"),
                                   QDir::currentPath(),
                                   tr("Any (*.*)"));

    if(!filename.isEmpty() && !filename.isNull()) {
        QFile   file(filename);
        if(file.open(QIODevice::WriteOnly)) {
            QList<QTreeWidgetItem*> list = this->ui->ProxyFetcherChecker_treeWidget->selectedItems();
            if(list.count() > 0 ) {
                register size_t n = list.count();
                QString  text;
                for(register unsigned int i = 0;i < n;++i) {
                    text = list.at(i)->text(0) + QLatin1String(":") + list.at(i)->text(1) + QLatin1String("\r\n");
                    file.write(text.toLatin1());
                }
            } else {
                register unsigned int i = 0;

                register size_t n = this->ui->ProxyFetcherChecker_treeWidget->topLevelItemCount();
                for(;i < n;++i) {
                    QString tmp = this->ui->ProxyFetcherChecker_treeWidget->topLevelItem(i)->text(0) + QLatin1String(":") + this->ui->ProxyFetcherChecker_treeWidget->topLevelItem(i)->text(1) + QLatin1String("\r\n");
                    file.write(tmp.toLatin1());
                }
            }

        }
    }

}

/* Load proxies from file. Format of each line is IP:PORT */
void Proxy::on_ImportProxies_pushButton_clicked()
{
    int originalSize = 0;
    QString filename;


        filename = QFileDialog::getOpenFileName(this, tr("Open File"),
                                       QDir::currentPath(),
                                       tr("Any (*.*)"));

    if(!filename.isEmpty() && !filename.isNull()) {
        QFile   file(filename);
        if(file.open(QIODevice::ReadOnly)) {
            originalSize = Proxy::proxyList.count();

            int n = 0;
            while(!file.atEnd()) {
                QString line = file.readLine().trimmed();
                if(!line.isEmpty()) {
                        Proxy::proxyList.insert(line);
                        n++;
                }
            }


            file.close();
#ifndef NDEBUG
            qDebug() << n << " proxies importet";
#endif

            this->ui->ProxyFetcherChecker_treeWidget->clear();

            if(Proxy::proxyList.size() > 0) {

                this->ui->ExportProxies_pushButton->setEnabled(true);
                this->ui->CheckProxies_pushButton->setEnabled(true);

                if(this->ui->ProxyFetcherChecker_treeWidget->topLevelItemCount() > 0) {
                    /* Enable export/import proxy button */
                    this->ui->ExportProxies_pushButton->setEnabled(true);
                    this->ui->ImportProxies_pushButton->setEnabled(true);
                }
            }

        }
#ifndef NDEBUG
        qDebug() << "ALL READY!";
#endif       
    }
    emit readyForProxyChecking();
}


/* ============================================================ */
/*
 * Host	Port	HTTP Status	Latency (in ms)		Country		SSL	Anonymous
-------------------------------------------------------------------------------------------------------
*/
/*
 * Begin proxy checking.
 * FIXME: Need multithread expert here */
void Proxy::on_CheckProxies_pushButton_clicked()
{
#ifndef NDEBUG
    qDebug() << __PRETTY_FUNCTION__ << " called ...";
#endif

    this->ui->ProxyChecking_label->setVisible(true);
    this->ui->ProxyChecking_progressBar->setVisible(true);
    this->ui->stopChecking_pushButton->setVisible(true);

    Proxy::NUMBER_OF_PROXIES = 0;


    this->ui->ProxyChecking_progressBar->setMaximum(this->proxies.count());
    this->ui->ProxyChecking_progressBar->setValue(0);

/* ===================================================================== */

    this->ui->ProxyFetcherChecker_treeWidget->clear();

    worker = new Worker(proxies);
    workerThread = new QThread(this);
    worker->moveToThread(workerThread);

    connect(workerThread, SIGNAL(started()), worker, SLOT(doWork()));

    connect(worker,SIGNAL(finished()),workerThread,SLOT(quit()));
    connect(worker,SIGNAL(allProxiesFinished()),workerThread,SLOT(quit()));

    connect(worker,SIGNAL(finished()),worker,SLOT(deleteLater()));
    connect(workerThread,SIGNAL(finished()),workerThread,SLOT(deleteLater()));

    connect(this,SIGNAL(abortConnections()),worker,SLOT(slot_abortConnections()));

    connect(worker,SIGNAL(dataReady(QStringList,QColor)),this,SLOT(dataReady(QStringList,QColor)));
    workerThread->start(QThread::TimeCriticalPriority);
}

void
Proxy::on_stopChecking_pushButton_clicked()
{
    disconnect(worker,SIGNAL(dataReady(QStringList,QColor)),this,SLOT(dataReady(QStringList,QColor)));
    emit abortConnections();
    emit allProxiesFinished();
    workerThread->quit();
}

void
Proxy::slot_allProxiesFinished()
{
    this->ui->ProxyChecking_progressBar->setValue(this->ui->ProxyChecking_progressBar->maximum());

#ifndef NDEBUG
    qDebug() << "=========================================";
    qDebug() << "ALL " << Proxy::NUMBER_OF_PROXIES << "PROXIES FINISHED!";
    qDebug() << "=========================================";
#endif

    this->ui->ProxyChecking_label->setVisible(false);
    this->ui->ProxyChecking_progressBar->setVisible(false);
    this->ui->stopChecking_pushButton->setVisible(false);

}
