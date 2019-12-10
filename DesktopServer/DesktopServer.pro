#-------------------------------------------------
#
# Project created by QtCreator 2019-09-06T14:22:32
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DesktopServer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

include($$PWD/screen_capture_lite/screen_capture_lite.pri)

CONFIG += c++17

#gona use own pooled allocators
DEFINES += LODEPNG_NO_COMPILE_ALLOCATORS

SOURCES += \
        brcconnection.cpp \
        brcserver.cpp \
        lodepng.cpp \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        brcconnection.h \
        brcserver.h \
        lodepng.h \
        mainwindow.h

FORMS += \
        mainwindow.ui


LIBS += -lpthread

QMAKE_CXXFLAGS +=  -pipe -std=c++17 -Wall -frtti -fexceptions -Werror=return-type -Werror=overloaded-virtual
QMAKE_CXXFLAGS +=  -Wctor-dtor-privacy -Werror=delete-non-virtual-dtor -fno-strict-aliasing
QMAKE_CXXFLAGS +=  -Werror=strict-aliasing -Wstrict-aliasing=2
INCLUDEPATH += $$PWD/../utils

unix:!macosx: DEFINES += OS_LINUX

include($$PWD/../NetProto/bproto.pri)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
