#include <QApplication>
#include <QDir>
#include <QIcon>
#include "galoshwindow.h"
#include "galoshterm.h"
#include "mudletimport.h"

int main(int argc, char** argv)
{
  QApplication::setApplicationName("Galosh");
  QApplication::setApplicationDisplayName("Galosh");
  QApplication::setOrganizationName("Alkahest");
  QApplication::setOrganizationDomain("com.alkahest");
  QApplication app(argc, argv);

  QIcon icon;
  icon.addFile(":/icon16.png");
  icon.addFile(":/icon24.png");
  icon.addFile(":/icon32.png");
  icon.addFile(":/icon48.png");
  icon.addFile(":/icon64.png");
  app.setWindowIcon(icon);

  GaloshWindow win;
  win.show();
  win.openConnectDialog();

  return app.exec();
}
