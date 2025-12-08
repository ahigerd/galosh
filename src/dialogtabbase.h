#ifndef GALOSH_DIALOGTABBASE_H
#define GALOSH_DIALOGTABBASE_H

#include <QWidget>
#include "profiledialog.h"
class UserProfile;

class DialogTabBase : public QWidget
{
Q_OBJECT
public:
  DialogTabBase(const QString& label, QWidget* parent = nullptr);

  inline QString label() const { return m_label; }
  virtual void load(UserProfile* profile) = 0;
  virtual bool save(UserProfile* profile) = 0;

protected:
  inline static QFrame* horizontalLine(QWidget* parent = nullptr) { return ProfileDialog::horizontalLine(parent); }
  void autoConnectMarkDirty();

signals:
  void markDirty();

private:
  QString m_label;
};

#endif
