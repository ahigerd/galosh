#ifndef GALOSH_APPEARANCETAB_H
#define GALOSH_APPEARANCETAB_H

#include "dialogtabbase.h"
#include <QFont>
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

class AppearanceTab : public DialogTabBase
{
Q_OBJECT
public:
  AppearanceTab(QWidget* parent = nullptr);

  virtual void load(UserProfile* profile) override;
  virtual bool save(UserProfile* profile) override;

signals:
  void markDirty();

private slots:
  void selectFont();
  void openColorSchemes();
  void updateColorScheme();
  void setCurrentFont(const QFont& font);

private:
  QComboBox* colorScheme;
  QLabel* fontPreview;
  QCheckBox* useFontForAll;
  QPushButton* pickFont;
  QFont currentFont;
};

#endif
