TEMPLATE = app
QT = core gui widgets network
CONFIG += c++2a
CONFIG += debug
CONFIG -= debug_and_release release
INCLUDEPATH += src/ src/qtermwidget src/mapping
OBJECTS_DIR = .build
MOC_DIR = .build
RCC_DIR = .build
UI_DIR = .build

win32 {
  SOURCES += src/mman-win32/mman.c
  # DEFINES += HAVE_ICU
  # LIBS += -licu
  RESOURCES += res/res-win.qrc
}
else {
  RESOURCES += res/res-unix.qrc
}

RESOURCES += res/res.qrc

# windows
HEADERS += src/galoshwindow.h   src/profiledialog.h   src/colorschemes.h
SOURCES += src/galoshwindow.cpp src/profiledialog.cpp src/colorschemes.cpp

HEADERS += src/msspview.h   src/exploredialog.h   src/mapoptions.h
SOURCES += src/msspview.cpp src/exploredialog.cpp src/mapoptions.cpp

# widgets
HEADERS += src/galoshsession.h   src/galoshterm.h   src/roomview.h
SOURCES += src/galoshsession.cpp src/galoshterm.cpp src/roomview.cpp

HEADERS += src/triggertab.h   src/appearancetab.h   src/waypointstab.h
SOURCES += src/triggertab.cpp src/appearancetab.cpp src/waypointstab.cpp

HEADERS += src/commandline.h   src/multicommandline.h   src/dropdowndelegate.h
SOURCES += src/commandline.cpp src/multicommandline.cpp src/dropdowndelegate.cpp

# models
HEADERS += src/userprofile.h   src/serverprofile.h
SOURCES += src/userprofile.cpp src/serverprofile.cpp

HEADERS += src/triggermanager.h   src/infomodel.h   src/itemdatabase.h
SOURCES += src/triggermanager.cpp src/infomodel.cpp src/itemdatabase.cpp

# networking
HEADERS += src/telnetsocket.h
SOURCES += src/telnetsocket.cpp

HEADERS += src/algorithms.h src/refable.h src/settingsgroup.h
SOURCES += src/main.cpp

include(src/mapping/mapping.pri)
include(src/commands/commands.pri)
include(src/qtermwidget/qtermwidget.pri)

VERSION = 0.0.4

# If git commands can be run without errors, grab the commit hash
system(git log -1 --pretty=format:) {
  BUILD_HASH = -$$system(git log -1 --pretty=format:%h)
}
else {
  BUILD_HASH =
}

DEFINES += GALOSH_VERSION=$${VERSION}$${BUILD_HASH}
