TEMPLATE = app
QT = core gui widgets network
CONFIG += debug
CONFIG -= debug_and_release release
INCLUDEPATH += src/ qtermwidget/lib
OBJECTS_DIR = .build
MOC_DIR = .build
RCC_DIR = .build
UI_DIR = .build

DEFINES += HAVE_UPDWTMPX HAVE_POSIX_OPENPT HAVE_SYS_TIME_H
DEFINES += KB_LAYOUT_DIR='\\"/usr/share/qtermwidget6/kb-layouts\\"'
DEFINES += COLORSCHEMES_DIR='\\"/usr/share/qtermwidget6/color-schemes\\"'

# windows
HEADERS += src/galoshwindow.h   src/profiledialog.h
SOURCES += src/galoshwindow.cpp src/profiledialog.cpp

# widges
HEADERS += src/galoshterm.h   src/roomview.h   src/commandline.h
SOURCES += src/galoshterm.cpp src/roomview.cpp src/commandline.cpp

# models
HEADERS += src/triggermanager.h   src/mapmanager.h   src/infomodel.h
SOURCES += src/triggermanager.cpp src/mapmanager.cpp src/infomodel.cpp

# networking
HEADERS += src/termsocket.h   src/telnetsocket.h
SOURCES += src/termsocket.cpp src/telnetsocket.cpp

SOURCES += src/main.cpp

HEADERS += $$files(qtermwidget/lib/*.h)
SOURCES += $$files(qtermwidget/lib/*.cpp)
FORMS += $$files(qtermwidget/lib/*.ui)
