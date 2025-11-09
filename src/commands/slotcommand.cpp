#include "slotcommand.h"
#include <QtDebug>

SlotCommand::SlotCommand(const QString& name, QObject* obj, const char* slot, const QString& _briefHelp, const QString& _fullHelp)
: TextCommand(name), obj(obj), briefHelp(_briefHelp), fullHelp(_fullHelp)
{
  const QMetaObject* mo = obj->metaObject();

  QByteArray slotBA = mo->normalizedSignature(slot);
  Q_ASSERT_X(!slotBA.isEmpty(), slot, "empty slot name");

  if (slotBA[0] >= '0' && slotBA[0] <= '9') {
    // remove signal/slot prefix
    slotBA = slotBA.mid(1);
  }

  QList<QMetaMethod> overloads;
  if (slotBA.contains('(')) {
    // explicit overload
    int index = mo->indexOfMethod(slotBA);
    Q_ASSERT_X(index >= 0, slot, "could not resolve slot");
    overloads << mo->method(index);
  } else {
    int methodCount = mo->methodCount();
    for (int i = 0; i < methodCount; i++) {
      QMetaMethod m = mo->method(i);
      if (m.name() == slotBA) {
        overloads << m;
      }
    }
    Q_ASSERT_X(!overloads.isEmpty(), slot, "could not resolve slot");
  }

  for (const QMetaMethod& method : overloads) {
    if (method.parameterCount() == 0) {
      method0 = method;
    } else if (method.parameterCount() == 1) {
      if (method.parameterType(0) == qMetaTypeId<QString>()) {
        method1 = method;
      } else if (method.parameterType(0) == qMetaTypeId<QStringList>()) {
        methodN = method;
      } else {
        Q_ASSERT_X(false, slot, "could not resolve slot parametertype");
      }
    }
  }

  Q_ASSERT_X(method0.isValid() || method1.isValid() || methodN.isValid(), slot, "could not resolve slot overload");

  if (briefHelp.endsWith(".")) {
    briefHelp.chop(1);
  }
  if (fullHelp.isEmpty()) {
    fullHelp = briefHelp + ".";
  }
}

int SlotCommand::minimumArguments() const
{
  if (method0.isValid() || methodN.isValid()) {
    return 0;
  } else {
    return 1;
  }
}

int SlotCommand::maximumArguments() const
{
  if (methodN.isValid()) {
    return -1;
  } else if (method1.isValid()) {
    return 1;
  } else {
    return 0;
  }
}

QString SlotCommand::helpMessage(bool brief) const
{
  if (brief || fullHelp.isEmpty()) {
    return briefHelp;
  }
  return fullHelp;
}

void SlotCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (args.isEmpty() && method0.isValid()) {
    method0.invoke(obj);
  } else if (args.length() == 1 && method1.isValid()) {
    method1.invoke(obj, Q_ARG(QString, args[0]));
  } else if (methodN.isValid()) {
    methodN.invoke(obj, Q_ARG(QStringList, args));
  } else {
    showError("unsupported arguments");
  }
}
