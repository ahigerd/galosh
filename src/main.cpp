#include <QApplication>
#include <QIcon>
#include <QSettings>
#include "galoshwindow.h"

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define GALOSH_VERSION_STRING STRINGIFY(GALOSH_VERSION)

int main(int argc, char** argv)
{
  QApplication::setApplicationName("Galosh");
  QApplication::setApplicationVersion(GALOSH_VERSION_STRING);
  QApplication::setApplicationDisplayName("Galosh");
  QApplication::setOrganizationName("Alkahest");
  QApplication::setOrganizationDomain("com.alkahest");
  QApplication app(argc, argv);
  app.setAttribute(Qt::AA_UseHighDpiPixmaps);

  QIcon icon;
  icon.addFile(":/icon16.png");
  icon.addFile(":/icon24.png");
  icon.addFile(":/icon32.png");
  icon.addFile(":/icon48.png");
  icon.addFile(":/icon64.png");
  app.setWindowIcon(icon);

  GaloshWindow win;
  {
    QSettings settings;
    if (settings.value("maximized").toBool()) {
      win.showMaximized();
    } else {
      win.show();
    }
  }
  win.openConnectDialog();

  return app.exec();
}
