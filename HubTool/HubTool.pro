#-------------------------------------------------
#
# Project created by QtCreator 2015-01-23T15:36:25
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = HubTool
TEMPLATE = app

INCLUDEPATH += $$PWD/lib/
INCLUDEPATH += $$PWD/lib/BrainStem2/
INCLUDEPATH += /usr/lib/gcc/x86_64-linux-gnu/9/include
message ($$INCLUDEPATH)

SOURCES += main.cpp\
           clickablelabel.cpp \
           clicktoeditlabel.cpp \
           hubtool.cpp \
           qcustomplot.cpp \
           stemworker.cpp \
           plotwindow.cpp

HEADERS  += hubtool.h \
            clickablelabel.h \
            clicktoeditlabel.h \
            qcustomplot.h \
            stemworker.h \
            plotwindow.h \
            appnap.h

FORMS    += hubtool.ui \
            clicktoeditlabel.ui \
            plotwindow.ui

# Change to new .icns file.
mac: ICON = HubTool.icns
win32: RC_FILE = HubTool.rc

RESOURCES += \
    resources.qrc

CONFIG += c++11

#unix:!mac:CONFIG(release, debug|release): CONFIG += static

#win32:CONFIG(release, debug|release): CONFIG += static
static { # Everything below takes effect with CONFIG += static
    CONFIG += static
    DEFINES += STATIC
    message("Static build.")
}

LIBS += -L$$PWD/../lib/ -lBrainStem2 -ludev
DEPENDPATH += $$PWD/../lib
PRE_TARGETDEPS += $$PWD/../lib/libBrainStem2.a

DISTFILES += \
    HubTool.rc

OBJECTIVE_SOURCES += \
    appnap.mm
