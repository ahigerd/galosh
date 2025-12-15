#ifndef GALOSH_ITEMSETDIALOG_H
#define GALOSH_ITEMSETDIALOG_H

#include <QDialog>
#include "itemdatabase.h"
class EquipmentView;
class UserProfile;
class QGroupBox;
class QComboBox;
class QPushButton;

class ItemSetDialog : public QDialog
{
Q_OBJECT
public:
  ItemSetDialog(UserProfile* profile, bool newSet, QWidget* parent = nullptr);

  void loadNewEquipment(const ItemDatabase::EquipmentSet& equipment);

  void done(int r) override;

signals:
  void equipNamedSet(const QString& setName);

public slots:
  void setConnected(bool isConnected);
  void loadNamedSet(const QString& setName);

private slots:
  void reset();
  void equipCurrentSet();
  void setMenu();
  void containerMenu();
  bool save();

private:
  bool validateSetName(const QString& name);

  UserProfile* profile;
  ItemDatabase* db;
  EquipmentView* equipView;
  QComboBox* cSetName;
  QComboBox* cContainer;
  QPushButton* equipButton;
  bool newSetMode;
  ItemDatabase::EquipmentSet loadedEquipment;
  QString loadedContainer;
};

#endif
