#include "mapviewer.h"
#include "mapmanager.h"
#include "explorehistory.h"
#include "userprofile.h"
#include "galoshsession.h"
#include <QSettings>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <QScrollBar>
#include <QToolTip>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QtDebug>

class MapWidget : public QWidget
{
public:
  MapWidget(MapViewer* parent)
  : QWidget(parent), mapViewer(parent), mapLayout(nullptr), zoomLevel(5), currentRoomId(-1)
  {
    setMouseTracking(true);
  }

  MapViewer* mapViewer;
  MapLayout* mapLayout;
  double zoomLevel;
  int currentRoomId;

  QSize sizeHint() const
  {
    if (!mapLayout) {
      return QSize();
    }
    return mapLayout->displaySize() * zoomLevel;
  }

  void setMap(MapLayout* map)
  {
    mapLayout = map;
    update();
  }

protected:
  void mouseMoveEvent(QMouseEvent* event)
  {
    if (!mapLayout) {
      QToolTip::hideText();
      return;
    }

    QPointF pt = event->pos();
    pt /= zoomLevel;
    const MapRoom* room = mapLayout->roomAt(pt);
    if (room) {
      QString label = QStringLiteral("%2 [%1]").arg(room->id).arg(room->name);
      if (room->zone != mapLayout->currentZone) {
        label = QStringLiteral("%1: %2").arg(room->zone, label);
      }
      QToolTip::showText(event->globalPos(), label, this);
    } else {
      QToolTip::hideText();
    }
  }

  void mouseDoubleClickEvent(QMouseEvent* event)
  {
    if (!mapLayout) {
      return;
    }

    QPointF pt = event->pos();
    pt /= zoomLevel;
    const MapRoom* room = mapLayout->roomAt(pt);
    if (room) {
      if (room->zone != mapLayout->currentZone) {
        mapViewer->loadZone(room->zone);
      } else {
        emit mapViewer->exploreMap(room->id);
      }
    }
  }

  void paintEvent(QPaintEvent* event)
  {
    if (!mapLayout) {
      return;
    }

    QPainter p(this);
    p.scale(zoomLevel, zoomLevel);
    QRect vp = event->rect();
    vp = QRect(vp.left() / zoomLevel, vp.top() / zoomLevel, vp.width() / zoomLevel, vp.height() / zoomLevel);
    mapLayout->render(&p, vp, mapViewer->mapType != MapViewer::MiniMap);

    QRectF highlight = mapLayout->roomPos(currentRoomId);
    if (!highlight.isNull()) {
      highlight.adjust(-1.5, -1.5, 2.5, 2.5);
      p.setPen(QPen(QColor(128, 255, 255), 1));
      p.setBrush(Qt::transparent);
      p.drawRect(highlight);
    }
  }
};

MapViewer::MapViewer(MapViewer::MapType mapType, QWidget* parent)
: QWidget(parent), session(nullptr), map(nullptr), mapLayout(nullptr), mapType(mapType)
{
  if (mapType == StandaloneMap) {
    setAttribute(Qt::WA_WindowPropagation, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
  }

  setBackgroundRole(QPalette::Window);
  QPalette p(palette());
  while (p.color(QPalette::Window).value() > 128) {
    p.setColor(QPalette::Window, p.color(QPalette::Window).darker());
  }
  setPalette(p);

  view = new MapWidget(this);

  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  header = new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout(header);
  layout->setSpacing(0);
  layout->setContentsMargins(2, 2, 2, 2);

  if (mapType == MiniMap) {
    layout->addStretch(1);
  }

  zone = new QComboBox(header);
  zone->setInsertPolicy(QComboBox::InsertAlphabetically);
  layout->addWidget(zone, 1);

  QToolButton* bIn = new QToolButton(header);
  bIn->setText("+");
  QObject::connect(bIn, SIGNAL(clicked()), this, SLOT(zoomIn()));
  layout->addWidget(bIn);

  QToolButton* bOut = new QToolButton(header);
  bOut->setText("\u2212");
  QObject::connect(bOut, SIGNAL(clicked()), this, SLOT(zoomOut()));
  layout->addWidget(bOut);

  if (mapType == MiniMap) {
    QFont small = font();
    small.setPointSize(small.pointSize() * .75);
    bIn->setFont(small);
    bOut->setFont(small);
    bIn->setFixedSize(bIn->minimumSizeHint());
    bOut->setFixedSize(bOut->minimumSizeHint());
    zone->setVisible(false);
    header->setParent(view);
  } else {
    mainLayout->addWidget(header, 0);
  }

  scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(false);
  scrollArea->setWidget(view);
  mainLayout->addWidget(scrollArea, 1);

  QSettings settings;
  if (mapType == MiniMap) {
    setZoom(settings.value("miniMapZoom", 1.5).toDouble());
  } else if (mapType == EmbedMap) {
    setZoom(settings.value("embedMapZoom", 5).toDouble());
  } else {
    restoreGeometry(settings.value("map").toByteArray());
    setZoom(settings.value("mapZoom", 5).toDouble());
  }

  QObject::connect(zone, SIGNAL(currentTextChanged(QString)), this, SLOT(loadZone(QString)));
  QObject::connect(scrollArea->horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(repositionHeader()));
  QObject::connect(scrollArea->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(repositionHeader()));

  reload();
}

void MapViewer::reload()
{
  zone->blockSignals(true);
  zone->clear();
  if (map) {
    for (const QString& name : map->zoneNames()) {
      zone->addItem(name);
    }
  }
  zone->blockSignals(false);
}

void MapViewer::setZoom(double level)
{
  QPointF center(scrollArea->horizontalScrollBar()->value(), scrollArea->verticalScrollBar()->value());
  center /= view->zoomLevel;
  view->zoomLevel = level;
  if (mapLayout) {
    view->resize(mapLayout->displaySize() * view->zoomLevel);
  }
  center *= view->zoomLevel;
  scrollArea->horizontalScrollBar()->setValue(center.x());
  scrollArea->verticalScrollBar()->setValue(center.y());
  resizeEvent(nullptr);

  QSettings settings;
  if (mapType == MiniMap) {
    settings.setValue("miniMapZoom", level);
  } else if (mapType == EmbedMap) {
    settings.setValue("embedMapZoom", level);
  } else {
    settings.setValue("mapZoom", level);
  }
}

void MapViewer::zoomIn()
{
  setZoom(view->zoomLevel * 5/4);
}

void MapViewer::zoomOut()
{
  setZoom(view->zoomLevel * 4/5);
}

void MapViewer::setCurrentRoom(int roomId)
{
  if (!mapLayout) {
    return;
  }
  const MapRoom* room;
  if (roomId < 0 && session) {
    room = session->currentRoom();
    if (room) {
      roomId = room->id;
    }
  } else {
    room = map->room(roomId);
  }
  if (!room) {
    qDebug() << "Unknown room" << roomId;
    return;
  }
  if (view->currentRoomId != roomId) {
    loadZone(room->zone);
    view->currentRoomId = roomId;
    QPointF pos = mapLayout->roomPos(roomId).center() * view->zoomLevel;
    scrollArea->ensureVisible(pos.x(), pos.y(), width() / 3, height() / 3);
  }
  // TODO: recalc map if necessary (will this need a toggle?)
}

void MapViewer::loadZone(const QString& name, bool force)
{
  if (!mapLayout) {
    return;
  }
  if (zone->currentText() != name) {
    zone->blockSignals(true);
    if (zone->findText(name) < 0) {
      if (map->zoneNames().contains(name)) {
        zone->addItem(name);
      } else {
        qDebug() << "Unknown zone" << name;
        zone->blockSignals(false);
        return;
      }
    }
    zone->setCurrentText(name);
    zone->blockSignals(false);
  }
  mapLayout->loadZone(map->zone(name), force);
  view->resize(mapLayout->displaySize() * view->zoomLevel);
  resizeEvent(nullptr);
  view->update();
}

void MapViewer::repositionHeader()
{
  if (mapType == MiniMap) {
    int w = scrollArea->width();
    int sw = scrollArea->verticalScrollBar()->sizeHint().width() + 2;
    if (view->width() + sw > w) {
      w -= sw;
    }
    header->setGeometry(scrollArea->horizontalScrollBar()->value(), scrollArea->verticalScrollBar()->value(), w, header->sizeHint().height());
  }
}

void MapViewer::resizeEvent(QResizeEvent* event)
{
  view->resize(view->sizeHint());

  if (event) {
    QWidget::resizeEvent(event);

    if (mapType == StandaloneMap) {
      QSettings settings;
      settings.setValue("map", saveGeometry());
    }
  }

  repositionHeader();
}

void MapViewer::showEvent(QShowEvent*)
{
  if (mapLayout) {
    QPointF pos = mapLayout->roomPos(view->currentRoomId).center() * view->zoomLevel;
    scrollArea->ensureVisible(pos.x(), pos.y(), width() / 3, height() / 3);
    repositionHeader();
  }
}

void MapViewer::moveEvent(QMoveEvent*)
{
  if (mapType == StandaloneMap) {
    QSettings settings;
    settings.setValue("map", saveGeometry());
  }
}

void MapViewer::setSession(GaloshSession* sess)
{
  if (sess == session) {
    return;
  }
  if (session) {
    QObject::disconnect(session, 0, this, 0);
  }
  session = sess;
  if (session) {
    if (mapType == MiniMap) {
      QObject::connect(session, SIGNAL(currentRoomUpdated()), this, SLOT(setCurrentRoom()));
    }

    map = session->map();
    mapLayout = map->layout();
    view->setMap(mapLayout);
    setCurrentRoom();
  } else {
    map = nullptr;
    mapLayout = nullptr;
    view->setMap(nullptr);
  }
  reload();
}
