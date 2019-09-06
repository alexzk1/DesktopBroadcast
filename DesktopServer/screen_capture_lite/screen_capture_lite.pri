#qmake include for https://github.com/smasherprog/screen_capture_lite

#common part, assumes C++17 standard to compile
INCLUDEPATH += $$PWD/include

HEADERS += $$PWD/include/ScreenCapture.h
HEADERS += $$PWD/include/internal/SCCommon.h
HEADERS += $$PWD/include/internal/ThreadManager.h

SOURCES += $$PWD/src/ScreenCapture.cpp
SOURCES += $$PWD/src/SCCommon.cpp
SOURCES += $$PWD/src/ThreadManager.cpp


#OS specifiec things, only linux currently
unix:!macx {
       HEADERS += $$PWD/include/linux/X11MouseProcessor.h
       HEADERS += $$PWD/include/linux/X11FrameProcessor.h

       SOURCES += $$PWD/src/linux/X11MouseProcessor.cpp
       SOURCES += $$PWD/src/linux/X11FrameProcessor.cpp
       SOURCES += $$PWD/src/linux/GetMonitors.cpp
       SOURCES += $$PWD/src/linux/GetWindows.cpp
       SOURCES += $$PWD/src/linux/ThreadRunner.cpp
       LIBS += -lX11 -lXext -lXfixes -lXtst -lXinerama

       #WARNING! set platform dependent include for each platform
       INCLUDEPATH += $$PWD/include/linux
}
else {
 error("Not implemented. Read original CMakeList.txt and extended.")
}
