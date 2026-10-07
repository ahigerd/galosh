#ifndef GALOSH_INFOMODEL_H
#define GALOSH_INFOMODEL_H

#include <QStandardItemModel>
#include <QVariant>

class InfoModel : public QStandardItemModel
{
Q_OBJECT
public:
  InfoModel(QObject* parent = nullptr);

  void loadLegacyTree(const QVariant& data);
  void gmcpChar(const QString& key, const QVariantMap& data);

private:
  QStandardItem* ensureItem(const QString& parentPath, const QString& key);
  void recurseLegacyTree(const QString& path, const QString& key, QVariant value);
  QVariant preprocess(const QString& path, const QString& key, const QVariant& value);
  void removeChildren(QStandardItem* parent, int row);

  QMap<QString, QStandardItem*> itemsByPath;
  QMap<QString, QString> labelMap;
};

#endif
