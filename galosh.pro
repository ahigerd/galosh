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

defineTest(addClasses) {
  for (base, CLASSES) {
    HEADERS += $$PWD/$${base}.h
    SOURCES += $$PWD/$${base}.cpp
  }
  export(HEADERS)
  export(SOURCES)
  CLASSES =
  export(CLASSES)
}

include(src/src.pri)
include(src/mapping/mapping.pri)
include(src/commands/commands.pri)
include(src/qtermwidget/qtermwidget.pri)

VERSION = 0.1.2

# If git commands can be run without errors, grab the commit hash
system(git log -1 --pretty=format:) {
  BUILD_HASH = -$$system(git log -1 --pretty=format:%h)
}
else {
  BUILD_HASH =
}

DEFINES += GALOSH_VERSION=$${VERSION}$${BUILD_HASH}
