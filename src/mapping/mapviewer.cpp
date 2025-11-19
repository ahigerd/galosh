#include "mapviewer.h"
#include "mapmanager.h"
#include <QSettings>
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
  MapWidget(MapLayout* mapLayout, QWidget* parent = nullptr)
  : QWidget(parent), mapLayout(mapLayout), zoomLevel(5)
  {
    setMouseTracking(true);
  }

  MapLayout* mapLayout;
  double zoomLevel;

  QSize sizeHint() const
  {
    return mapLayout->displaySize() * zoomLevel;
  }

protected:
  void mouseMoveEvent(QMouseEvent* event)
  {
    QPointF pt = event->pos();
    pt /= zoomLevel;
    const MapRoom* room = mapLayout->roomAt(pt);
    if (room) {
      QToolTip::showText(event->globalPos(), QStringLiteral("[%1] %2").arg(room->id).arg(room->name), this);
    } else {
      QToolTip::hideText();
    }
  }

  void paintEvent(QPaintEvent* event)
  {
    QPainter p(this);
    p.scale(zoomLevel, zoomLevel);
    QRect vp = event->rect();
    vp = QRect(vp.left() / zoomLevel, vp.top() / zoomLevel, vp.width() / zoomLevel, vp.height() / zoomLevel);
    mapLayout->render(&p, vp);
  }
};

MapViewer::MapViewer(MapManager* map, QWidget* parent)
: QScrollArea(parent), map(map)
{
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setBackgroundRole(QPalette::Window);
  QPalette p(palette());
  while (p.color(QPalette::Window).value() > 128) {
    p.setColor(QPalette::Window, p.color(QPalette::Window).darker());
  }
  setPalette(p);

  mapLayout.reset(new MapLayout(map, map->search()));
  view = new MapWidget(mapLayout.get(), this);

  header = new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout(header);
  layout->setContentsMargins(2, 2, 2, 2);

  zone = new QComboBox(header);
  QObject::connect(zone, SIGNAL(currentTextChanged(QString)), this, SLOT(loadZone(QString)));
  layout->addWidget(zone, 1);

  QToolButton* bIn = new QToolButton(header);
  bIn->setText("+");
  QObject::connect(bIn, SIGNAL(clicked()), this, SLOT(zoomIn()));
  layout->addWidget(bIn);

  QToolButton* bOut = new QToolButton(header);
  bOut->setText("\u2212");
  QObject::connect(bOut, SIGNAL(clicked()), this, SLOT(zoomOut()));
  layout->addWidget(bOut);

  for (const QString& name : map->zoneNames()) {
    const MapZone* mapZone = map->zone(name);
    if (mapZone->roomIds.size() < 3) {
      continue;
    }
    zone->addItem(name);
  }

  setViewportMargins(0, header->sizeHint().height(), 0, 0);
  setWidgetResizable(false);
  setWidget(view);

  QSettings settings;
  restoreGeometry(settings.value("map").toByteArray());
}

void MapViewer::setZoom(double level)
{
  QPointF center(horizontalScrollBar()->value(), verticalScrollBar()->value());
  center /= view->zoomLevel;
  view->zoomLevel = level;
  view->resize(mapLayout->displaySize() * view->zoomLevel);
  center *= view->zoomLevel;
  horizontalScrollBar()->setValue(center.x());
  verticalScrollBar()->setValue(center.y());
}

void MapViewer::zoomIn()
{
  setZoom(view->zoomLevel * 5/4);
}

void MapViewer::zoomOut()
{
  setZoom(view->zoomLevel * 4/5);
}

void MapViewer::loadZone(const QString& name)
{
  if (zone->currentText() != name) {
    zone->blockSignals(true);
    zone->setCurrentText(name);
    zone->blockSignals(false);
  }
  mapLayout->loadZone(map->zone(name));
  view->resize(mapLayout->displaySize() * view->zoomLevel);
  view->update();
}

void MapViewer::resizeEvent(QResizeEvent* event)
{
  int w = width();
  if (verticalScrollBar()->isVisible()) {
    w -= verticalScrollBar()->width();
  }
  header->setGeometry(0, 0, w, header->sizeHint().height());
  view->resize(view->sizeHint());

  QScrollArea::resizeEvent(event);

  QSettings settings;
  settings.setValue("map", saveGeometry());
}

void MapViewer::moveEvent(QMoveEvent*)
{
  QSettings settings;
  settings.setValue("map", saveGeometry());
}
