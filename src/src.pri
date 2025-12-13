# windows
CLASSES += galoshwindow profiledialog colorschemes
CLASSES += msspview exploredialog mapoptions
CLASSES += equipmentview itemsearchdialog

CLASSES += galoshsession galoshterm roomview
CLASSES += dialogtabbase servertab
CLASSES += triggertab appearancetab waypointstab
CLASSES += commandline multicommandline dropdowndelegate

# models
CLASSES += userprofile serverprofile
CLASSES += triggermanager infomodel itemdatabase

# networking
CLASSES += telnetsocket

HEADERS += $$PWD/algorithms.h $$PWD/refable.h $$PWD/settingsgroup.h
SOURCES += $$PWD/main.cpp

addClasses()
