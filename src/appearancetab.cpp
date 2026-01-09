#include "appearancetab.h"
#include "userprofile.h"
#include "colorschemes.h"
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFontDialog>

AppearanceTab::AppearanceTab(QWidget* parent)
: DialogTabBase(tr("Appearance"), parent)
{
  QFormLayout* layout = new QFormLayout(this);

  layout->addRow("&Color scheme:", colorScheme = new QComboBox(this));
  colorScheme->addItem("Default");
  for (const QString& name : ColorSchemes::availableSchemes()) {
    colorScheme->addItem(name);
  }
  QPushButton* editSchemes = new QPushButton("&Edit Schemes...", this);
  layout->addRow("", editSchemes);

  layout->addRow(horizontalLine(this));

  layout->addRow("Font:", fontPreview = new QLabel(this));
  fontPreview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  fontPreview->setBackgroundRole(QPalette::Base);
  fontPreview->setAutoFillBackground(true);
  fontPreview->setMargin(4);

  layout->addRow("", useDefaultFont = new QCheckBox("&Use default font", this));
  layout->addRow("", pickFont = new QPushButton("Select &Font...", this));
  layout->addRow("", setAsDefault = new QPushButton("Set as &Default Font", this));

  QObject::connect(useDefaultFont, SIGNAL(toggled(bool)), this, SIGNAL(markDirty()));
  QObject::connect(useDefaultFont, SIGNAL(toggled(bool)), this, SLOT(updateFontPreview()));
  QObject::connect(pickFont, SIGNAL(clicked()), this, SLOT(selectFont()));
  QObject::connect(setAsDefault, SIGNAL(clicked()), this, SLOT(setDefaultFont()));

  QObject::connect(editSchemes, SIGNAL(clicked()), this, SLOT(openColorSchemes()));
  QObject::connect(colorScheme, SIGNAL(currentIndexChanged(int)), this, SLOT(updateColorScheme()));

  defaultFont = UserProfile::defaultFont();
  setCurrentFont(defaultFont);
  updateColorScheme();

  autoConnectMarkDirty();
}

void AppearanceTab::load(UserProfile* profile)
{
  blockSignals(true);

  setCurrentFont(profile->font());
  useDefaultFont->setChecked(profile->useDefaultFont);

  QString colors = profile->colorScheme;
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

bool AppearanceTab::save(UserProfile* profile)
{
  if (colorScheme->currentIndex() == 0) {
    profile->colorScheme.clear();
  } else {
    profile->colorScheme = colorScheme->currentText();
  }

  profile->useDefaultFont = useDefaultFont->isChecked();
  profile->selectedFont = currentFont;

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
  currentFont = font;
  useDefaultFont->setChecked(false);
  updateFontPreview();
  if (font != currentFont) {
    emit markDirty();
  }
}

void AppearanceTab::updateFontPreview()
{
  QFont font = currentFont;
  if (useDefaultFont->isChecked()) {
    font = defaultFont;
  }
  fontPreview->setFont(font);
  fontPreview->setText(QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize()));
  setAsDefault->setEnabled(!useDefaultFont->isChecked());
}

void AppearanceTab::setDefaultFont()
{
  UserProfile::setDefaultFont(currentFont);
  defaultFont = currentFont;
  useDefaultFont->setChecked(true);
  emit markDirty();
}
