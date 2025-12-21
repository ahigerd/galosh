#ifndef GALOSH_GALOSHSESSION_H
#define GALOSH_GALOSHSESSION_H

#include <QPointer>
#include <QStringList>
#include <memory>
#include "galoshterm.h"
#include "profiledialog.h"
#include "explorehistory.h"
#include "automapper.h"
#include "userprofile.h"
#include "itemdatabase.h"
#include "commands/textcommandprocessor.h"
class MapManager;
class MapRoom;
class TriggerManager;
class InfoModel;
class ExploreDialog;
class ItemSearchDialog;
class ItemSetDialog;

class GaloshSession : public QObject, public TextCommandProcessor
{
Q_OBJECT
public:
  GaloshSession(UserProfile* profile, QWidget* parent = nullptr);
  ~GaloshSession();

  std::unique_ptr<UserProfile> profile;
  QPointer<GaloshTerm> term;
  QString name() const;
  MapManager* map() const;
  ItemDatabase* itemDB() const;
  TriggerManager* triggers() const;
  QHash<QString, QString> msspData() const;
  const MapRoom* currentRoom() const;
  inline bool isConnected() const { return !connectedHost().isEmpty(); }
  QString connectedHost() const;
  QString statusBarText() const;

  void connect();
  void startOffline();
  void reload();

  bool hasUnread() const;

  InfoModel* infoModel;

  bool eventFilter(QObject* obj, QEvent* event);

  void equipmentReceived(const ItemDatabase::EquipmentSet& equipment);

signals:
  void msspReceived();
  void statusUpdated();
  void currentRoomUpdated();
  void openProfileDialog(ProfileDialog::Tab);
  void unreadUpdated();

public slots:
  void exploreMap(int roomId = -1, const QString& movement = QString());
  CommandResult switchEquipment(const QString& set, const QString& container = QString());
  void openItemDatabase();
  void openItemSets();

#ifdef Q_MOC_RUN
  void help();
#endif

private slots:
  void showCommandMessage(TextCommand* command, const QString& message, MessageType msgType) override;
  void speedwalk(const QStringList& steps);
  void abortSpeedwalk();
  void setLastRoom(int roomId);
  void gmcpEvent(const QString& key, const QVariant& value);
  void onLineReceived(const QString& line);
  void setUnread();
  void serverCertificate(const QMap<QString, QString>& info, bool selfSigned, bool nameMismatch);
  void connectionChanged();
  void processSlashCommand(const QString& command, const QStringList& args);
  void processCommandQueue(const CommandResult* result = nullptr);
  void stepTimeout();

private:
  void speedwalkStep(const QString& step, CommandResult& res, bool fast);
  void changeEquipment(const ItemDatabase::EquipmentSet& current, const QString& setName, const QString& container);
  bool customCommandFilter(const QString& command);

  AutoMapper autoMap;
  ExploreHistory exploreHistory;
  QStringList speedPath;
  QStringList commandQueue;
  QString statusBar;
  QPointer<ExploreDialog> explore;
  QPointer<ItemSearchDialog> itemSearch;
  QPointer<ItemSetDialog> itemSets;
  bool unread;
  CommandResult equipResult, stepResult, customResult;
  QTimer stepTimer;
};

#endif
