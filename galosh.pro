TEMPLATE = app
QT = core gui widgets network
CONFIG += debug
CONFIG -= debug_and_release release
INCLUDEPATH += src/ src/qtermwidget
OBJECTS_DIR = .build
MOC_DIR = .build
RCC_DIR = .build
UI_DIR = .build

win32 {
  SOURCES += src/mman-win32/mman.c
  DEFINES += HAVE_ICU
  LIBS += -licu
  RESOURCES += res/res-win.qrc
}
else {
  RESOURCES += res/res-unix.qrc
}

RESOURCES += res/res.qrc

# windows
HEADERS += src/galoshwindow.h   src/profiledialog.h   src/triggertab.h
SOURCES += src/galoshwindow.cpp src/profiledialog.cpp src/triggertab.cpp

# widgets
HEADERS += src/galoshterm.h   src/roomview.h   src/commandline.h
SOURCES += src/galoshterm.cpp src/roomview.cpp src/commandline.cpp

# models
HEADERS += src/triggermanager.h   src/mapmanager.h   src/infomodel.h
SOURCES += src/triggermanager.cpp src/mapmanager.cpp src/infomodel.cpp

HEADERS += src/mudletimport.h
SOURCES += src/mudletimport.cpp

# networking
HEADERS += src/telnetsocket.h
SOURCES += src/telnetsocket.cpp

SOURCES += src/main.cpp

include(src/qtermwidget/qtermwidget.pri)
