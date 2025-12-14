#ifndef GALOSH_ITEMSETDIALOG_H
#define GALOSH_ITEMSETDIALOG_H

#include <QDialog>
#include "itemdatabase.h"
class EquipmentView;
class UserProfile;
class QGroupBox;
class QComboBox;

class ItemSetDialog : public QDialog
{
Q_OBJECT
public:
  ItemSetDialog(UserProfile* profile, bool newSet, QWidget* parent = nullptr);

  void loadNewEquipment(const QList<ItemDatabase::EquipSlot>& equipment);
  void loadNamedSet(const QString& setName);

  void done(int r) override;

private slots:
  void reset();

private:
  bool save();

  UserProfile* profile;
  ItemDatabase* db;
  EquipmentView* equipView;
  QComboBox* setName;
  bool newSetMode;
  QList<ItemDatabase::EquipSlot> loadedEquipment;
};

#endif
