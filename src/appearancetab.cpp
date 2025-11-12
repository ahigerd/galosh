#include "appearancetab.h"
#include "profiledialog.h"
#include "colorschemes.h"
#include <QSettings>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFontDialog>
#include <QFontDatabase>

AppearanceTab::AppearanceTab(QWidget* parent)
: QWidget(parent)
{
  QFormLayout* layout = new QFormLayout(this);

  layout->addRow("&Color scheme:", colorScheme = new QComboBox(this));
  colorScheme->addItem("Default");
  for (const QString& name : ColorSchemes::availableSchemes()) {
    colorScheme->addItem(name);
  }
  QPushButton* editSchemes = new QPushButton("&Edit Schemes...", this);
  layout->addRow("", editSchemes);

  layout->addRow(ProfileDialog::horizontalLine(this));

  layout->addRow("Font:", fontPreview = new QLabel(this));
  fontPreview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  fontPreview->setBackgroundRole(QPalette::Base);
  fontPreview->setAutoFillBackground(true);
  fontPreview->setMargin(4);

  layout->addRow("", pickFont = new QPushButton("Select &Font...", this));
  layout->addRow("", useFontForAll = new QCheckBox("&Use this font for all profiles", this));

  QObject::connect(useFontForAll, SIGNAL(toggled(bool)), this, SIGNAL(markDirty()));
  QObject::connect(pickFont, SIGNAL(clicked()), this, SLOT(selectFont()));
  QObject::connect(editSchemes, SIGNAL(clicked()), this, SLOT(openColorSchemes()));
  QObject::connect(colorScheme, SIGNAL(currentIndexChanged(int)), this, SLOT(updateColorScheme()));

  setCurrentFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  updateColorScheme();
}

void AppearanceTab::load(const QString& profile)
{
  blockSignals(true);

  QSettings globalSettings;
  QSettings settings(profile, QSettings::IniFormat);
  settings.beginGroup("Appearance");

  bool useGlobalFont = globalSettings.value("Defaults/fontGlobal").toBool();
  QFont globalFont = globalSettings.value("Defaults/font", QFontDatabase::systemFont(QFontDatabase::FixedFont)).value<QFont>();
  if (settings.contains("font")) {
    setCurrentFont(settings.value("font").value<QFont>());
  } else {
    setCurrentFont(globalFont);
  }
  useFontForAll->setChecked(useGlobalFont && globalFont == currentFont);

  QString colors = settings.value("colors").toString();
  int colorIndex = 0;
  if (!colors.isEmpty()) {
    colorIndex = colorScheme->findText(colors);
    if (colorIndex < 0) {
      colorIndex = 0;
    }
  }
  colorScheme->setCurrentIndex(colorIndex);

  blockSignals(false);
}

bool AppearanceTab::save(const QString& profile)
{
  QSettings settings(profile, QSettings::IniFormat);
  settings.beginGroup("Appearance");

  if (colorScheme->currentIndex() == 0) {
    settings.remove("colors");
  } else {
    settings.setValue("colors", colorScheme->currentText());
  }

  QSettings globalSettings;
  if (useFontForAll->isChecked()) {
    globalSettings.setValue("Defaults/font", currentFont);
    globalSettings.setValue("Defaults/fontGlobal", true);
    settings.remove("font");
  } else {
    globalSettings.setValue("Defaults/fontGlobal", false);
    settings.setValue("font", currentFont);
  }

  return true;
}

void AppearanceTab::selectFont()
{
  QFontDialog* dlg = new QFontDialog(fontPreview->font(), this);
  dlg->setAttribute(Qt::WA_DeleteOnClose, true);
  dlg->setOption(QFontDialog::ProportionalFonts, false);
  dlg->setOption(QFontDialog::MonospacedFonts, true);
  dlg->open(this, SLOT(setCurrentFont(QFont)));
}

void AppearanceTab::openColorSchemes()
{
  ColorSchemes* dlg;
  if (colorScheme->currentIndex() == 0) {
    dlg = new ColorSchemes(this);
  } else {
    dlg = new ColorSchemes(colorScheme->currentText(), this);
  }
  dlg->open();
}

void AppearanceTab::updateColorScheme()
{
  ColorScheme scheme = ColorSchemes::scheme(colorScheme->currentText());
  QPalette p = palette();
  p.setColor(QPalette::Base, scheme[ColorScheme::Background]);
  p.setColor(QPalette::Text, scheme[ColorScheme::Foreground]);
  fontPreview->setPalette(p);
}

void AppearanceTab::setCurrentFont(const QFont& font)
{
  if (font == currentFont) {
    return;
  }
  currentFont = font;
  fontPreview->setFont(font);
  fontPreview->setText(QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize()));
  emit markDirty();
}
