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
}

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
HEADERS += src/telnetsocket.h
SOURCES += src/telnetsocket.cpp

SOURCES += src/main.cpp

QTW = src/qtermwidget
HEADERS += $$QTW/TerminalDisplay.h   $$QTW/Filter.h   $$QTW/ScreenWindow.h
SOURCES += $$QTW/TerminalDisplay.cpp $$QTW/Filter.cpp $$QTW/ScreenWindow.cpp

HEADERS += $$QTW/Screen.h   $$QTW/Emulation.h   $$QTW/KeyboardTranslator.h
SOURCES += $$QTW/Screen.cpp $$QTW/Emulation.cpp $$QTW/KeyboardTranslator.cpp

HEADERS += $$QTW/tools.h   $$QTW/konsole_wcwidth.h   $$QTW/History.h
SOURCES += $$QTW/tools.cpp $$QTW/konsole_wcwidth.cpp $$QTW/History.cpp

HEADERS += $$QTW/TerminalCharacterDecoder.h   $$QTW/BlockArray.h
SOURCES += $$QTW/TerminalCharacterDecoder.cpp $$QTW/BlockArray.cpp

HEADERS += $$QTW/Vt102Emulation.h
SOURCES += $$QTW/Vt102Emulation.cpp

HEADERS += $$QTW/Character.h $$QTW/CharacterColor.h
