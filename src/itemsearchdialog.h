#ifndef GALOSH_ITEMSEARCHDIALOG_H
#define GALOSH_ITEMSEARCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
class ItemDatabase;
class QCheckBox;
class QGroupBox;
class QComboBox;
class QSpinBox;
class QAbstractButton;
class QToolButton;
class QPushButton;
class QButtonGroup;
class QTreeView;
class QStandardItemModel;
class QGridLayout;

class ItemSearchDialog : public QDialog
{
Q_OBJECT
public:
  ItemSearchDialog(ItemDatabase* db, bool forSelection, QWidget* parent = nullptr);

  void setRequiredSlot(const QString& keyword);
  void requireFlag(const QString& flag, bool require = true);
  inline void prohibitFlag(const QString& flag) { requireFlag(flag, false); }

  QString selectedItemName() const;

private slots:
  void applyRowUpdated();
  void removeApplyRow(QAbstractButton*);
  void addApplyRow();
  void search();
  void updateEnabled();
  void selectItem(const QModelIndex& index);
  void tryAccept();
  void openItemContextMenu(const QPoint& pos);
  void showDetails(const QModelIndex& index);

private:
  QGroupBox* makeCheckGroup(const QString& label, const QStringList& options, bool tristate, QList<QCheckBox*>& list);

  QModelIndex keyIndex(const QModelIndex& index) const;
  QModelIndex currentIndex() const;

  QList<QCheckBox*> typeChecks, slotChecks, flagChecks;
  QMap<QCheckBox*, QString> checkValues;

  QGridLayout* lApply;
  struct ApplyRow {
    QComboBox* mod;
    QComboBox* compare;
    QSpinBox* value;
    QToolButton* remove;
  };
  QList<ApplyRow> applyRows;
  QButtonGroup* applyRemove;
  QTreeView* matchView;
  QStandardItemModel* matchModel;
  QPushButton* searchButton;
  QPushButton* selectButton;

  ItemDatabase* db;
};

#endif
