#ifndef GALOSH_INFOMODEL_H
#define GALOSH_INFOMODEL_H

#include <QStandardItemModel>
#include <QVariant>

class InfoModel : public QStandardItemModel
{
Q_OBJECT
public:
  InfoModel(QObject* parent = nullptr);

  void loadTree(const QVariant& data);

private:
  void recurseTree(const QString& path, const QString& key, QVariant value);
  QVariant preprocess(const QString& path, const QString& key, const QVariant& value);
  void removeChildren(QStandardItem* parent, int row);

  QMap<QString, QStandardItem*> itemsByPath;
};

#endif
