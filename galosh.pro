TEMPLATE = app
QT = core gui widgets network
CONFIG += debug
INCLUDEPATH += src/ qtermwidget/lib
OBJECTS_DIR = .build
MOC_DIR = .build
RCC_DIR = .build
UI_DIR = .build

DEFINES += HAVE_UPDWTMPX HAVE_POSIX_OPENPT HAVE_SYS_TIME_H
DEFINES += KB_LAYOUT_DIR='\\"/usr/share/qtermwidget6/kb-layouts\\"'
DEFINES += COLORSCHEMES_DIR='\\"/usr/share/qtermwidget6/color-schemes\\"'

HEADERS += src/galoshwindow.h   src/galoshterm.h   src/infomodel.h
SOURCES += src/galoshwindow.cpp src/galoshterm.cpp src/infomodel.cpp

HEADERS += src/triggermanager.h   src/mapmanager.h   src/roomview.h
SOURCES += src/triggermanager.cpp src/mapmanager.cpp src/roomview.cpp

HEADERS += src/termsocket.h   src/telnetsocket.h   src/commandline.h
SOURCES += src/termsocket.cpp src/telnetsocket.cpp src/commandline.cpp

SOURCES += src/main.cpp

HEADERS += $$files(qtermwidget/lib/*.h)
SOURCES += $$files(qtermwidget/lib/*.cpp)
FORMS += $$files(qtermwidget/lib/*.ui)
