#ifndef GALOSH_COLORSCHEMES_H
#define GALOSH_COLORSCHEMES_H

#include <QDialog>
#include <QColor>
#include <QSet>
#include <QMap>
#include "CharacterColor.h"
class QComboBox;
class QCheckBox;
class QToolButton;
class QPushButton;
class QLabel;

struct ColorScheme
{
  enum Role {
    Foreground = 0,
    Background,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Intense = 10,
    ForegroundIntense = 10,
    BackgroundIntense,
    BlackIntense,
    RedIntense,
    GreenIntense,
    YellowIntense,
    BlueIntense,
    MagentaIntense,
    CyanIntense,
    WhiteIntense,
  };

  QString name;
  Konsole::ColorEntry colors[20];
  bool isDarkBackground;

  inline QColor operator[](Role role) const { return colors[role].color; }
  inline QColor& operator[](Role role) { return colors[role].color; }
};

constexpr ColorScheme::Role operator!(ColorScheme::Role lhs)
{
  if (lhs >= ColorScheme::Intense) {
    return ColorScheme::Role(lhs - ColorScheme::Intense);
  }
  return ColorScheme::Role(lhs + ColorScheme::Intense);
}

class ColorSchemes : public QDialog
{
Q_OBJECT
public:
  ColorSchemes(QWidget* parent = nullptr);
  ColorSchemes(const QString& selected, QWidget* parent = nullptr);

  static QString defaultScheme();
  static void setDefaultScheme(const QString& scheme);

  static QStringList availableSchemes();
  static ColorScheme scheme(const QString& name, bool defaultOnly = false);

public slots:
  void selectScheme(const QString& name, bool forceDefault = false);
  void selectColor(int index);
  void setAsDefault(bool on);
  void newScheme();
  void resetScheme();
  void deleteScheme();
  virtual void done(int r) override;

private:
  void refreshColors();

  QComboBox* schemes;
  QCheckBox* isDefault;
  QLabel* previews[20];
  QToolButton* colorButtons[20];
  QPushButton* resetButton;
  QPushButton* deleteButton;
  ColorScheme current;
  bool currentIsDefault;
  QFont defaultFont;

  QString selectedDefault;
  QMap<QString, ColorScheme> pending;
  QSet<QString> toDelete;
};

#endif
