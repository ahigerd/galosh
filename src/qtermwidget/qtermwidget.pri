CLASSES += TerminalDisplay Filter ScreenWindow
CLASSES += Screen Emulation KeyboardTranslator
CLASSES += konsole_wcwidth History
CLASSES += TerminalCharacterDecoder BlockArray
CLASSES += Vt102Emulation

HEADERS += $$PWD/Character.h $$PWD/CharacterColor.h

unix {
  macx {
    DEFINES += HAVE_UTMPX UTMPX_COMPAT
  }
  else {
    DEFINES += HAVE_OPENPTY HAVE_PTY_H HAVE_UPDWTMPX
  }

  CLASSES += kpty kptydevice kptyprocess kprocess

  HEADERS += $$PWD/kpty_p.h
}

addClasses()
