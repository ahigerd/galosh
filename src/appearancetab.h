#ifndef GALOSH_APPEARANCETAB_H
#define GALOSH_APPEARANCETAB_H

#include <QWidget>
#include <QFont>
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

class AppearanceTab : public QWidget
{
Q_OBJECT
public:
  AppearanceTab(QWidget* parent = nullptr);

  void load(const QString& profile);
  bool save(const QString& profile);

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
