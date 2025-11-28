#include "colorschemes.h"
#include <QButtonGroup>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QSettings>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QtDebug>

static const char* previewLabels[20] = {
  "Foreground",
  "Background",
  "&0",
  "&1",
  "&2",
  "&3",
  "&4",
  "&5",
  "&6",
  "&7",
  "Bright FG",
  "Bright BG",
  "&L",
  "&R",
  "&G",
  "&Y",
  "&B",
  "&M",
  "&C",
  "&W",
};

static const char* colorLabels[10] = {
  "Foreground",
  "Background",
  "Black",
  "Red",
  "Green",
  "Yellow",
  "Blue",
  "Magenta",
  "Cyan",
  "White",
};

static ColorScheme generateScheme(const QString& name, quint8 l0, quint8 h0, quint8 l1, quint8 h1, int yellow = -1)
{
  if (yellow < 0) {
    yellow = h0;
  }
  return {
    name,
    {
      {{ h0, h0, h0 }},
      {{ l0, l0, l0 }},

      {{ l0, l0, l0 }},
      {{ h0, l0, l0 }},
      {{ l0, h0, l0 }},
      {{ h0, h0, l0 }},
      {{ l0, l0, h0 }},
      {{ h0, l0, h0 }},
      {{ l0, h0, h0 }},
      {{ h0, h0, h0 }},

      {{ l1, h1, l1 }},
      {{ l1, l1, l1 }},

      {{ l1, l1, l1 }},
      {{ h1, l1, l1 }},
      {{ l1, h1, l1 }},
      {{ h1, h1, l1 }},
      {{ l1, l1, h1 }},
      {{ h1, l1, h1 }},
      {{ l1, h1, h1 }},
      {{ h1, h1, h1 }},
    },
    true,
  };
}

static ColorScheme toggleDark(ColorScheme scheme)
{
  QColor fg = scheme[ColorScheme::Foreground];
  QColor bg = scheme[ColorScheme::Background];
  QColor fg2 = scheme[!ColorScheme::Foreground];
  QColor bg2 = scheme[!ColorScheme::Background];
  scheme[ColorScheme::Foreground] = bg;
  scheme[ColorScheme::Background] = fg2;
  scheme[!ColorScheme::Foreground] = bg2;
  scheme[!ColorScheme::Background] = fg;
  scheme.isDarkBackground = !scheme.isDarkBackground;
  for (int i = 2; i < 10; i++) {
    ColorScheme::Role role = ColorScheme::Role(i);
    QColor t = scheme[role];
    scheme[role] = scheme[!role];
    scheme[!role] = t;
  }
  return scheme;
}

static ColorScheme makeCGA(const QString& name, ColorScheme::Role fg)
{
  ColorScheme scheme = generateScheme(name, 0, 196, 78, 243);
  scheme[ColorScheme::Yellow] = { 196, 126, 0 };
  scheme[!ColorScheme::Blue] = { 78, 78, 220 };
  scheme[!ColorScheme::Green] = { 78, 220, 78 };
  scheme[!ColorScheme::Red] = { 220, 78, 78 };
  scheme[ColorScheme::Foreground] = scheme[fg];
  scheme[!ColorScheme::Foreground] = scheme[!fg];
  return scheme;
}

static ColorScheme makeMono(const QString& name, ColorScheme::Role fg)
{
  ColorScheme scheme = makeCGA(name, fg);
  QColor base = scheme[fg];
  for (int i = 2; i < 20; i++) {
    ColorScheme::Role role = ColorScheme::Role(i);
    QColor c = scheme[role];
    if (fg == !ColorScheme::White) {
      c = c.darker();
    }
    if (role == ColorScheme::White || role == !ColorScheme::White || role == !ColorScheme::Black) {
      scheme[role] = QColor::fromHsv(base.hue(), base.saturation(), c.value());
    } else if (role < ColorScheme::Intense) {
      scheme[role] = QColor::fromHsv(base.hue(), base.saturation(), c.value() * 0.75);
    } else if (role >= ColorScheme::RedIntense) {
      scheme[role] = QColor::fromHsv(base.hue(), base.saturation(), c.value() - 40);
    }
  }
  if (fg == !ColorScheme::White) {
    scheme[ColorScheme::Foreground] = QColor(32, 32, 32);
    scheme[ColorScheme::Background] = QColor(255, 255, 255);
    scheme[!ColorScheme::Foreground] = QColor(0, 0, 0);
  } else {
    scheme[ColorScheme::Background] = QColor::fromHsv(base.hue(), base.saturation(), 32);
    scheme[!ColorScheme::Background] = scheme[!ColorScheme::Black];
    scheme[!ColorScheme::Foreground] = QColor::fromHsv(base.hue(), qMin(255, int(base.saturation() * 1.25)), 255);
  }
  return scheme;
}

static ColorScheme makeEGA(const QString& name, ColorScheme::Role fg)
{
  ColorScheme scheme = generateScheme(name, 0, 170, 85, 255, 85);
  scheme[ColorScheme::Foreground] = scheme[fg];
  scheme[!ColorScheme::Foreground] = scheme[!fg];
  return scheme;
}

static ColorScheme makeLinuxBase(const QString& name)
{
  ColorScheme scheme = generateScheme(name, 24, 178, 84, 255, 104);
  scheme[!ColorScheme::Background] = QColor(104, 104, 104);
  scheme[ColorScheme::Black] = QColor(0, 0, 0);
  scheme[!ColorScheme::Black] = QColor(104, 104, 104);
  return scheme;
}

static ColorScheme makeLinux(const QString& name, ColorScheme::Role fg)
{
  ColorScheme scheme = makeLinuxBase(name);
  scheme[ColorScheme::Foreground] = scheme[fg];
  scheme[!ColorScheme::Foreground] = scheme[!fg];
  scheme[ColorScheme::Background] = QColor(0, 0, 0);
  return scheme;
}

static ColorScheme makeGalosh(const QString& name, const QColor& fg, const QColor& fgBright)
{
  ColorScheme scheme = makeLinuxBase(name);
  scheme[ColorScheme::Foreground] = fg;
  scheme[!ColorScheme::Foreground] = fgBright;
  scheme[ColorScheme::Background] = QColor(32, 34, 32);
  scheme[!ColorScheme::Background] = QColor(192, 255, 224);
  return scheme;
}

static ColorScheme makeBlue(ColorScheme scheme)
{
  QColor blue = scheme[ColorScheme::Blue];
  QColor blue2 = scheme[!ColorScheme::Blue];
  scheme[ColorScheme::Blue] = blue.lighter(150);
  scheme[!ColorScheme::Blue] = blue2.lighter(150);
  scheme[ColorScheme::Background] = blue.darker(150);
  scheme[!ColorScheme::Background] = blue2.darker(150);
  return scheme;
}

static ColorScheme defaultSchemes[] = {
  makeGalosh("Galosh Green", { 24, 240, 24 }, { 128, 255, 192 }),
  makeCGA("CGA Green", ColorScheme::Green),
  makeEGA("EGA Green", ColorScheme::Green),
  makeMono("Mono Green", !ColorScheme::Green),
  makeLinux("Linux Green", ColorScheme::Green),
  makeCGA("CGA Amber", ColorScheme::Yellow),
  makeEGA("EGA Amber", ColorScheme::Yellow),
  makeLinux("Linux Amber", ColorScheme::Yellow),
  makeMono("Mono Amber", ColorScheme::Yellow),
  makeCGA("CGA White", ColorScheme::White),
  makeEGA("EGA White", ColorScheme::White),
  makeLinux("Linux White", ColorScheme::White),
  makeMono("Mono White", ColorScheme::White),
  makeBlue(makeCGA("CGA Blueprint", ColorScheme::White)),
  makeBlue(makeEGA("EGA Blueprint", ColorScheme::White)),
  makeBlue(makeLinux("Linux Blueprint", ColorScheme::White)),
  toggleDark(makeCGA("CGA Light Mode", ColorScheme::White)),
  toggleDark(makeEGA("EGA Light Mode", ColorScheme::White)),
  toggleDark(makeLinux("Linux Light Mode", ColorScheme::White)),
  makeMono("Mono Light Mode", !ColorScheme::White),
};

ColorSchemes::ColorSchemes(const QString& selected, QWidget* parent)
: ColorSchemes(parent)
{
  schemes->setCurrentIndex(schemes->findText(selected));
  selectScheme(selected);
}

ColorSchemes::ColorSchemes(QWidget* parent)
: QDialog(parent)
{
  QSettings settings;
  if (settings.contains("Defaults/font")) {
    defaultFont = settings.value("Defaults/font").value<QFont>();
  } else {
    defaultFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  }

  QGridLayout* layout = new QGridLayout(this);

  QHBoxLayout* top = new QHBoxLayout;
  QLabel* lSchemes = new QLabel("Color &scheme:", this);
  schemes = new QComboBox(this);
  lSchemes->setBuddy(schemes);
  for (const QString& name : availableSchemes()) {
    schemes->addItem(name);
  }
  schemes->setCurrentIndex(schemes->findText(defaultScheme()));
  QObject::connect(schemes, SIGNAL(currentTextChanged(QString)), this, SLOT(selectScheme(QString)));
  top->addStretch(1);
  top->addWidget(lSchemes, 0);
  top->addWidget(schemes, 3);
  top->addStretch(1);
  layout->addLayout(top, 0, 0, 1, 4);

  QHBoxLayout* lDefault = new QHBoxLayout;
  isDefault = new QCheckBox("Use this as the &default color scheme", this);
  QObject::connect(isDefault, SIGNAL(toggled(bool)), this, SLOT(setAsDefault(bool)));
  lDefault->addStretch(1);
  lDefault->addWidget(isDefault, 0);
  lDefault->addStretch(1);
  layout->addLayout(lDefault, 1, 1, 1, 2);

  QGroupBox* gNormal = new QGroupBox("Normal", this);
  QGridLayout* lNormal = new QGridLayout(gNormal);
  lNormal->setColumnMinimumWidth(2, 20);
  layout->addWidget(gNormal, 2, 1);

  QGroupBox* gIntense = new QGroupBox("Intense", this);
  QGridLayout* lIntense = new QGridLayout(gIntense);
  lIntense->setColumnMinimumWidth(2, 20);
  layout->addWidget(gIntense, 2, 2);

  QButtonGroup* bg = new QButtonGroup(this);
  QSize swatchSize(32, 24);
  int x = 0, y = 0;
  for (int i = 0; i < 10; i++) {
    QToolButton* nButton = new QToolButton(gNormal);
    nButton->setIconSize(swatchSize);
    colorButtons[i] = nButton;
    bg->addButton(nButton, i);
    lNormal->addWidget(new QLabel(colorLabels[i], gNormal), y, x);
    lNormal->addWidget(nButton, y, x + 1);

    QToolButton* iButton = new QToolButton(gIntense);
    iButton->setIconSize(swatchSize);
    colorButtons[i + 10] = iButton;
    bg->addButton(iButton, i + 10);
    lIntense->addWidget(new QLabel(colorLabels[i], gNormal), y, x);
    lIntense->addWidget(iButton, y, x + 1);

    x += 3;
    if (x >= 4) {
      x = 0;
      y++;
    }
  }
  QObject::connect(bg, SIGNAL(idClicked(int)), this, SLOT(selectColor(int)));

  QHBoxLayout* bottom = new QHBoxLayout;
  QFrame* fPreviews = new QFrame(this);
  fPreviews->setFrameStyle(QFrame::Sunken | QFrame::StyledPanel);
  QGridLayout* lPreviews = new QGridLayout(fPreviews);
  lPreviews->setSpacing(0);
  lPreviews->setContentsMargins(0, 0, 0, 0);
  x = 0;
  y = 0;
  QFont boldFont = defaultFont;
  boldFont.setBold(true);

  QFontMetrics fm(defaultFont);
  QFontMetrics fmb(boldFont);
  if (fm.boundingRect("mmm").width() != fmb.boundingRect("mmm").width()) {
    boldFont = defaultFont;
  }

  for (int c = 0; c < 20; c++) {
    QLabel* p = new QLabel(previewLabels[c], fPreviews);
    p->setMargin(3);
    p->setFont(c >= ColorScheme::Intense ? boldFont : defaultFont);
    p->setAutoFillBackground(true);
    p->setAlignment(Qt::AlignCenter);
    previews[c] = p;
    int w = (c == 0 || c == 1 || c == 10 || c == 11) ? 2 : 1;
    lPreviews->addWidget(p, y, x, 1, w);
    x += w;
    if (x >= 4) {
      x = 0;
      y++;
    }
  }
  fPreviews->setMinimumWidth(200);
  bottom->addStretch(1);
  bottom->addWidget(fPreviews, 2);
  bottom->addStretch(1);
  layout->addLayout(bottom, 3, 0, 1, 4);

  QHBoxLayout* lButtons = new QHBoxLayout;
  QDialogButtonBox* controls = new QDialogButtonBox(this);
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  lButtons->addWidget(controls, 0);
  lButtons->addStretch(1);
  lButtons->addWidget(buttons, 0);
  layout->addLayout(lButtons, 4, 0, 1, 4);

  QPushButton* addButton = new QPushButton("&New Scheme", this);
  QObject::connect(addButton, SIGNAL(clicked()), this, SLOT(newScheme()));
  controls->addButton(addButton, QDialogButtonBox::ActionRole);
  deleteButton = new QPushButton("&Delete Scheme", this);
  controls->addButton(deleteButton, QDialogButtonBox::ActionRole);
  resetButton = new QPushButton("&Revert Scheme", this);
  controls->addButton(resetButton, QDialogButtonBox::ActionRole);

  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);
  layout->setColumnStretch(2, 0);
  layout->setColumnStretch(3, 1);

  selectedDefault = defaultScheme();
  selectScheme(selectedDefault);
}

QString ColorSchemes::defaultScheme()
{
  QSettings settings;
  return settings.value("Defaults/colors", "Galosh Green").toString();
}

void ColorSchemes::setDefaultScheme(const QString& scheme)
{
  QSettings settings;
  settings.setValue("Defaults/colors", scheme);
}

QStringList ColorSchemes::availableSchemes()
{
  QSettings settings;
  QStringList names;
  for (const ColorScheme& scheme : defaultSchemes) {
    names << scheme.name;
  }
  for (const QString& key : settings.childGroups()) {
    if (!key.startsWith("ColorScheme-")) {
      continue;
    }
    QString name = settings.value(key + "/name").toString();
    if (!name.isEmpty()) {
      names << name;
    }
  }
  return names;
}

ColorScheme ColorSchemes::scheme(const QString& name, bool defaultOnly)
{
  if (!defaultOnly) {
    QString key = "ColorScheme-" + name;
    QSettings settings;
    if (settings.contains(key)) {
      settings.beginGroup(key);
      ColorScheme scheme;
      scheme.name = settings.value("name").toString();
      for (int i = 0; i < 20; i++) {
        scheme[ColorScheme::Role(i)] = QColor(settings.value(QString::number(i)).toString());
      }
      scheme.isDarkBackground = settings.value("dark", true).toBool();
      return scheme;
    }
  }
  for (const ColorScheme& scheme : defaultSchemes) {
    if (scheme.name == name) {
      return scheme;
    }
  }
  return defaultSchemes[0];
}

static QIcon makeSwatch(const QColor& color)
{
  QImage img(32, 24, QImage::Format_RGB32);
  img.fill(color.rgb());
  return QIcon(QPixmap::fromImage(img));
}

void ColorSchemes::selectScheme(const QString& name, bool forceDefault)
{
  if (!forceDefault && pending.contains(name)) {
    current = pending[name];
  } else {
    current = scheme(name, forceDefault);
  }
  currentIsDefault = false;
  for (const ColorScheme& scheme : defaultSchemes) {
    if (scheme.name == name) {
      currentIsDefault = true;
      break;
    }
  }
  resetButton->setVisible(currentIsDefault);
  deleteButton->setVisible(!currentIsDefault);
  isDefault->blockSignals(true);
  isDefault->setChecked(name == selectedDefault);
  isDefault->blockSignals(false);
  refreshColors();
}

void ColorSchemes::refreshColors()
{
  QPalette p = palette();
  for (int i = 0; i < 20; i++) {
    QColor c = current[ColorScheme::Role(i)];
    if (i == ColorScheme::Background) {
      p.setColor(QPalette::WindowText, c);
      p.setColor(QPalette::Window, current[ColorScheme::Foreground]);
    } else if (i == !ColorScheme::Background) {
      p.setColor(QPalette::WindowText, current[ColorScheme::Background]);
      p.setColor(QPalette::Window, c);
    } else {
      p.setColor(QPalette::WindowText, c);
      p.setColor(QPalette::Window, current[ColorScheme::Background]);
    }
    previews[i]->setPalette(p);
    colorButtons[i]->setIcon(makeSwatch(c));
  }
}

void ColorSchemes::selectColor(int index)
{
  ColorScheme::Role role = ColorScheme::Role(index);
  QColorDialog dlg(current[role], this);
  for (int i = 0; i < 8; i++) {
    dlg.setCustomColor(i * 2, current[ColorScheme::Role(i + 2)]);
    dlg.setCustomColor(i * 2 + 1, current[ColorScheme::Role(i + 12)]);
  }
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  current[role] = dlg.selectedColor();
  pending[schemes->currentText()] = current;
  refreshColors();
}

void ColorSchemes::newScheme()
{
  QInputDialog dlg(this);
  dlg.setLabelText("Color scheme name:");
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  QString name = dlg.textValue();
  if (name.isEmpty()) {
    QMessageBox::critical(this, "Galosh", "Color scheme name is required.");
    return;
  }
  if (schemes->findText(name) >= 0) {
    QMessageBox::critical(this, "Galosh", "Color scheme name already exists.");
    return;
  }
  schemes->addItem(name);
  schemes->blockSignals(true);
  schemes->setCurrentIndex(schemes->count() - 1);
  schemes->blockSignals(false);
  pending[name] = current;
  currentIsDefault = false;
}

void ColorSchemes::resetScheme()
{
  selectScheme(schemes->currentText(), true);
}

void ColorSchemes::deleteScheme()
{
  int index = schemes->currentIndex();
  toDelete << schemes->currentText();
  pending.remove(schemes->currentText());
  schemes->removeItem(index);
  schemes->setCurrentIndex(index - 1);
}

void ColorSchemes::setAsDefault(bool on)
{
  if (!on) {
    isDefault->setChecked(true);
  }
  selectedDefault = schemes->currentText();
}

void ColorSchemes::done(int r)
{
  if (r == QDialog::Accepted) {
    QSettings settings;
    for (const QString& name : toDelete) {
      settings.remove(name);
    }
    for (const QString& name : pending.keys()) {
      const ColorScheme& scheme = pending[name];
      QString key = "ColorScheme-" + name;
      settings.beginGroup(key);
      settings.setValue("name", name);
      settings.setValue("dark", scheme[ColorScheme::Background].value() < 127);
      for (int i = 0; i < 20; i++) {
        settings.setValue(QString::number(i), scheme[ColorScheme::Role(i)].name());
      }
      settings.endGroup();
    }
    settings.setValue("Defaults/colors", selectedDefault);
  }
  QDialog::done(r);
}
