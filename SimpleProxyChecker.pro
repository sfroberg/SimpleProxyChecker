#-------------------------------------------------
#
# Project created by QtCreator 2014-08-05T19:14:34
#
#-------------------------------------------------

QT       += core gui network webkitwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SimpleProxyChecker5
TEMPLATE = app


SOURCES += main.cpp\
        proxy.cpp \
    worker.cpp

HEADERS  += proxy.h \
    ping.h \
    worker.h

FORMS    += proxy.ui

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include

win32:LIBS += -L$$PWD/lib/ -lIphlpapi -lws2_32 -lz -lGeoIP
unix:LIBS += -lz -lGeoIP


win32 {
DESTDIR = $$OUT_PWD/SimpleProxyChecker

lib.path    =  $${DESTDIR}
lib.files   += data/*

INSTALLS       += lib

}

