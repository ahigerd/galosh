#include "dialogtabbase.h"
#include <QLineEdit>
#include <QAbstractButton>
#include <QComboBox>

DialogTabBase::DialogTabBase(const QString& label, QWidget* parent)
: QWidget(parent), m_label(label)
{
  // initializers only
}

void DialogTabBase::autoConnectMarkDirty()
{
  for (QObject* w : children()) {
    if (qobject_cast<QLineEdit*>(w)) {
      QObject::connect(w, SIGNAL(textEdited(QString)), this, SIGNAL(markDirty()));
    } else if (qobject_cast<QAbstractButton*>(w)) {
      QObject::connect(w, SIGNAL(toggled(bool)), this, SIGNAL(markDirty()));
    } else if (qobject_cast<QComboBox*>(w)) {
      QObject::connect(w, SIGNAL(currentIndexChanged(int)), this, SIGNAL(markDirty()));
    }
  }
}
