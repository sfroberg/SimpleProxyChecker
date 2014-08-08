#include "proxy.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Proxy w;
    w.show();

    return a.exec();
}
