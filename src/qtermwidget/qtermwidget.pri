HEADERS += $$PWD/TerminalDisplay.h   $$PWD/Filter.h   $$PWD/ScreenWindow.h
SOURCES += $$PWD/TerminalDisplay.cpp $$PWD/Filter.cpp $$PWD/ScreenWindow.cpp

HEADERS += $$PWD/Screen.h   $$PWD/Emulation.h   $$PWD/KeyboardTranslator.h
SOURCES += $$PWD/Screen.cpp $$PWD/Emulation.cpp $$PWD/KeyboardTranslator.cpp

HEADERS += $$PWD/konsole_wcwidth.h   $$PWD/History.h
SOURCES += $$PWD/konsole_wcwidth.cpp $$PWD/History.cpp

HEADERS += $$PWD/TerminalCharacterDecoder.h   $$PWD/BlockArray.h
SOURCES += $$PWD/TerminalCharacterDecoder.cpp $$PWD/BlockArray.cpp

HEADERS += $$PWD/Vt102Emulation.h $$PWD/Character.h $$PWD/CharacterColor.h
SOURCES += $$PWD/Vt102Emulation.cpp

unix {
  macx {
    DEFINES += HAVE_UTMPX UTMPX_COMPAT
  }
  else {
    DEFINES += HAVE_OPENPTY HAVE_PTY_H HAVE_UPDWTMPX
  }
  HEADERS += $$PWD/kpty.h   $$PWD/kptydevice.h   $$PWD/kptyprocess.h   $$PWD/kprocess.h   $$PWD/kpty_p.h
  SOURCES += $$PWD/kpty.cpp $$PWD/kptydevice.cpp $$PWD/kptyprocess.cpp $$PWD/kprocess.cpp
}
