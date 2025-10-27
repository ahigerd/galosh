#include <QApplication>
#include <QDir>
#include "galoshwindow.h"
#include "galoshterm.h"
#include "mudletimport.h"

const char* TRANSLATIONS_DIR = nullptr;

const char* findDir(const QString& path, QByteArray* buffer)
{
  QDir dir(qApp->applicationDirPath());
  if (dir.exists(path)) {
    *buffer = dir.filePath(path).toUtf8();
  } else if (dir.exists("qtermwidget/lib/" + path)) {
    *buffer = dir.filePath("qtermwidget/lib/" + path).toUtf8();
  } else if (QDir("/usr/share/qtermwidget6/").exists(path)) {
    *buffer = QDir("/usr/share/qtermwidget6/").filePath(path).toUtf8();
  }
  return buffer->constData();
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  QByteArray tsDir, kbDir, csDir;
  TRANSLATIONS_DIR = findDir("translations", &tsDir);

  GaloshWindow win;
  win.show();
  win.openConnectDialog();

  return app.exec();
}
