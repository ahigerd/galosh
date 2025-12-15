#include "itemsearchdialog.h"
#include "itemdatabase.h"
#include "algorithms.h"
#include <QMessageBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QMenu>
#include <QTimer>
#include <QtDebug>

ItemSearchDialog::ItemSearchDialog(ItemDatabase* db, bool forSelection, QWidget* parent)
: QDialog(parent), db(db)
{
  setWindowTitle("Item Search");

  QGridLayout* layout = new QGridLayout(this);

  QStringList types = db->knownTypes();
  std::sort(types.begin(), types.end());
  layout->addWidget(makeCheckGroup("Type (match any)", types, false, typeChecks), 0, 0, 1, 2);

  QStringList eqSlots;
  for (const QString& slot : db->equipmentSlotOrder()) {
    QString keyword = db->equipmentSlotType(slot).keyword;
    if (!keyword.isEmpty() && !eqSlots.contains(keyword)) {
      eqSlots << keyword;
    }
  }
  layout->addWidget(makeCheckGroup("Slot (match any)", eqSlots, false, slotChecks), 1, 0, 1, 2);

  QStringList flags = db->knownFlags();
  std::sort(flags.begin(), flags.end());
  layout->addWidget(makeCheckGroup("Flags (match all)", flags, true, flagChecks), 2, 0, 1, 2);

  QGroupBox* bApply = new QGroupBox("Stats (match all)", this);
  lApply = new QGridLayout(bApply);
  lApply->setColumnStretch(0, 1);
  lApply->setColumnStretch(1, 0);
  lApply->setColumnStretch(2, 1);
  lApply->setColumnStretch(3, 0);
  applyRemove = new QButtonGroup(this);
  QObject::connect(applyRemove, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(removeApplyRow(QAbstractButton*)));
  addApplyRow();
  layout->addWidget(bApply, 3, 0);

  QGroupBox* bMatches = new QGroupBox("Matching items", this);
  QGridLayout* lMatches = new QGridLayout(bMatches);
  matchView = new QTreeView(bMatches);
  matchView->setRootIsDecorated(false);
  matchView->setIndentation(0);
  matchView->setSortingEnabled(true);
  matchView->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(matchView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(openItemContextMenu(QPoint)));
  QObject::connect(matchView, SIGNAL(activated(QModelIndex)), this, SLOT(selectItem(QModelIndex)));
  lMatches->addWidget(matchView);
  layout->addWidget(bMatches, 4, 0, 1, 2);

  QHBoxLayout* lButtons = new QHBoxLayout;
  layout->addLayout(lButtons, 5, 0, 1, 2);

  if (!forSelection) {
    QPushButton* setButton = new QPushButton("Item &Sets...");
    QObject::connect(setButton, SIGNAL(clicked()), this, SIGNAL(openItemSets()));
    lButtons->addWidget(setButton, 0);
  }

  QDialogButtonBox* bResults = new QDialogButtonBox(QDialogButtonBox::Close, this);
  QObject::connect(bResults, SIGNAL(rejected()), this, SLOT(reject()));
  lButtons->addWidget(bResults, 1);

  searchButton = bResults->addButton("Search", QDialogButtonBox::ActionRole);
  searchButton->setIcon(QIcon::fromTheme("edit-find"));
  searchButton->setDefault(true);
  QObject::connect(searchButton, SIGNAL(clicked()), this, SLOT(search()));

  if (forSelection) {
    selectButton = bResults->addButton("Select", QDialogButtonBox::AcceptRole);
    selectButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    selectButton->setDefault(false);
    selectButton->setEnabled(false);
    QObject::connect(selectButton, SIGNAL(clicked()), this, SLOT(tryAccept()));
  } else {
    selectButton = nullptr;
  }

  layout->setRowStretch(4, 1);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 1);

  matchModel = new QStandardItemModel(0, 5, this);
  matchModel->setHorizontalHeaderLabels({ "Name", "Level", "Type", "Slot", "Stats" });
  matchView->setModel(matchModel);
  QObject::connect(matchView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(updateEnabled()));
}

QGroupBox* ItemSearchDialog::makeCheckGroup(const QString& label, const QStringList& options, bool tristate, QList<QCheckBox*>& list)
{
  QGroupBox* box = new QGroupBox(label, this);
  QGridLayout* layout = new QGridLayout(box);

  int x = 0;
  int y = 0;
  for (const QString& opt : options) {
    QCheckBox* check = new QCheckBox(opt, box);
    if (tristate) {
      check->setTristate(true);
      check->setCheckState(Qt::PartiallyChecked);
    }
    layout->addWidget(check, y, x);
    list << check;
    checkValues[check] = opt;
    x++;
    if (x == 8) {
      y++;
      x = 0;
    }
  }

  return box;
}

void ItemSearchDialog::addApplyRow()
{
  ApplyRow row;
  row.mod = new QComboBox;
  row.mod->addItem("");
  row.mod->addItem("armor");
  row.mod->addItem("damage");
  row.mod->addItem("level");
  row.mod->addItem("value");
  row.mod->addItem("weight");
  QStringList apply = db->knownApply();
  std::sort(apply.begin(), apply.end());
  for (const QString& mod : apply) {
    row.mod->addItem(mod);
  }
  QObject::connect(row.mod, SIGNAL(currentIndexChanged(int)), this, SLOT(applyRowUpdated()));

  row.compare = new QComboBox;
  row.compare->addItem("=");
  row.compare->addItem("!=");
  row.compare->addItem("<=");
  row.compare->addItem("<");
  row.compare->addItem(">=");
  row.compare->addItem(">");
  row.compare->setCurrentText(">");

  row.value = new QSpinBox;
  row.value->setRange(-999, 999);
  row.value->setValue(0);

  row.remove = new QToolButton;
  row.remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
  applyRemove->addButton(row.remove);

  int y = applyRows.count();
  lApply->addWidget(row.mod, y, 0);
  lApply->addWidget(row.compare, y, 1);
  lApply->addWidget(row.value, y, 2);
  lApply->addWidget(row.remove, y, 3);

  applyRows << row;
}

void ItemSearchDialog::applyRowUpdated()
{
  bool hasBlank = false;
  for (const ApplyRow& row : applyRows) {
    if (row.mod->currentText().isEmpty()) {
      hasBlank = true;
      break;
    }
  }
  if (hasBlank) {
    return;
  }
  addApplyRow();
}

void ItemSearchDialog::setRequiredSlot(const QString& keyword)
{
  for (QCheckBox* check : slotChecks) {
    check->setChecked(checkValues.value(check) == keyword);
    check->setEnabled(keyword.isEmpty());
  }
}

void ItemSearchDialog::requireFlag(const QString& flag, bool require)
{
  for (QCheckBox* check : flagChecks) {
    if (checkValues.value(check) == flag) {
      check->setChecked(require);
      break;
    }
  }
}

QModelIndex ItemSearchDialog::keyIndex(const QModelIndex& index) const
{
  QModelIndex key = index.siblingAtColumn(0);
  if (key.isValid() && (key.flags() & Qt::ItemIsEnabled)) {
    return key;
  }
  return QModelIndex();
}

QModelIndex ItemSearchDialog::currentIndex() const
{
  return keyIndex(matchView->currentIndex());
}

QString ItemSearchDialog::selectedItemName() const
{
  return currentIndex().data(Qt::DisplayRole).toString();
}

void ItemSearchDialog::removeApplyRow(QAbstractButton* button)
{
  for (int i = 0; i < applyRows.length(); i++) {
    ApplyRow row = applyRows[i];
    if (row.remove == button) {
      delete row.mod;
      delete row.compare;
      delete row.value;
      delete row.remove;
      applyRows.removeAt(i);
      break;
    }
  }
  applyRowUpdated();
  QTimer::singleShot(1, [this]{ resize(minimumSizeHint()); });
}

void ItemSearchDialog::search()
{
  QList<ItemQuery> queries;

  QStringList types;
  for (QCheckBox* check : typeChecks) {
    if (check->isChecked()) {
      types << checkValues.value(check);
    }
  }
  if (!types.isEmpty()) {
    queries << (ItemQuery){ "type", types, ItemQuery::Any };
  }

  QStringList eqSlots;
  for (QCheckBox* check : slotChecks) {
    if (check->isChecked()) {
      eqSlots << checkValues.value(check);
    }
  }
  if (!eqSlots.isEmpty()) {
    queries << (ItemQuery){ "worn", eqSlots, ItemQuery::Any };
  }

  QStringList flagsOn, flagsOff;
  for (QCheckBox* check : flagChecks) {
    if (check->checkState() == Qt::Checked) {
      flagsOn << checkValues.value(check);
    } else if (check->checkState() == Qt::Unchecked) {
      flagsOff << checkValues.value(check);
    }
  }
  if (!flagsOn.isEmpty()) {
    queries << (ItemQuery){ "flags", flagsOn, ItemQuery::All };
  }
  if (!flagsOff.isEmpty()) {
    queries << (ItemQuery){ "flags", flagsOff, ItemQuery::None };
  }

  for (const ApplyRow& row : applyRows) {
    QString mod = row.mod->currentText();
    if (mod.isEmpty()) {
      break;
    }
    ItemQuery query;
    query.stat = mod;
    switch (row.compare->currentIndex()) {
      case 0: query.compare = ItemQuery::Equal; break;
      case 1: query.compare = ItemQuery::NotEqual; break;
      case 2: query.compare = ItemQuery::LessEqual; break;
      case 3: query.compare = ItemQuery::Less; break;
      case 4: query.compare = ItemQuery::GreaterEqual; break;
      case 5: query.compare = ItemQuery::Greater; break;
    }
    query.value = row.value->value();
    queries << query;
  }

  matchModel->setRowCount(0);
  QList<ItemStats> results = db->searchForItem(queries);
  if (results.isEmpty()) {
    QStandardItem* noResults = new QStandardItem("No results.");
    noResults->setEnabled(false);
    matchModel->appendRow(noResults);
    updateEnabled();
    return;
  }
  for (const ItemStats& item : results) {
    QStringList apply;
    if (item.averageDamage > 0) {
      apply << QStringLiteral("%1 average damage").arg(item.averageDamage, 0, 'f', 1);
    }
    if (item.armor != 0) {
      apply << QStringLiteral("%1 armor").arg(item.armor);
    }
    for (auto [mod, value] : cpairs(item.apply)) {
      if (value < 0) {
        apply << QStringLiteral("%1%2").arg(mod).arg(value);
      } else {
        apply << QStringLiteral("%1+%2").arg(mod).arg(value);
      }
    }
    QList<QStandardItem*> cells = {
      new QStandardItem(item.uniqueName),
      new QStandardItem(),
      new QStandardItem(item.type),
      new QStandardItem(item.worn.join(" ")),
      new QStandardItem(apply.join(", ")),
    };
    cells[1]->setData(item.level, Qt::DisplayRole);
    for (auto cell : cells) {
      cell->setEditable(false);
    }
    matchModel->appendRow(cells);
  }
  matchView->header()->resizeSections(QHeaderView::ResizeToContents);
  updateEnabled();
}

void ItemSearchDialog::updateEnabled()
{
  if (!selectButton) {
    return;
  }
  bool hasSelection = currentIndex().isValid();
  selectButton->setEnabled(hasSelection);
  selectButton->setDefault(hasSelection);
  searchButton->setDefault(!hasSelection);
}

void ItemSearchDialog::selectItem(const QModelIndex& index)
{
  if (matchView->currentIndex() != index) {
    return;
  }
  if (!selectedItemName().isEmpty()) {
    if (selectButton) {
      accept();
    } else {
      showDetails(currentIndex());
    }
  }
}

void ItemSearchDialog::tryAccept()
{
  selectItem(matchView->currentIndex());
}

void ItemSearchDialog::openItemContextMenu(const QPoint& pos)
{
  QModelIndex index = keyIndex(matchView->indexAt(pos));
  if (!index.isValid()) {
    return;
  }
  QMenu menu;
  QAction* selectAction = nullptr;
  if (selectButton) {
    selectAction = menu.addAction("&Select");
  }
  QAction* detailsAction = menu.addAction("Sh&ow Details...");
  QAction* action = menu.exec(matchView->mapToGlobal(pos));
  if (!action) {
    return;
  } else if (action == selectAction) {
    matchView->setCurrentIndex(index);
    tryAccept();
  } else if (action == detailsAction) {
    showDetails(index);
  }
}

void ItemSearchDialog::showDetails(const QModelIndex& index)
{
  QString itemName = index.data(Qt::DisplayRole).toString();
  QString text = db->itemStats(itemName);
  QString keyword = db->itemKeyword(itemName);
  if (!keyword.isEmpty()) {
    text += "\n\n" + QStringLiteral("Assigned keyword: %1").arg(keyword);
  }
  QMessageBox mb(QMessageBox::NoIcon, "Item Details", text, QMessageBox::Ok, this);
  mb.exec();
}
