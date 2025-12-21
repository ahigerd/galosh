#include "commandresult.h"
#include <QtDebug>

static quint64 nextCallbackId = 1;

CommandResult CommandResult::fail()
{
  return CommandResult(true);
}

CommandResult CommandResult::success()
{
  return CommandResult(false);
}

CommandResult::Impl::Impl(quint64 id)
: callbackId(id), finished(false), error(false)
{
  // initializers only
}

CommandResult::CommandResult()
: d(new CommandResult::Impl(nextCallbackId++))
{
  // initializers only
}

CommandResult::CommandResult(bool err)
: d(nullptr), staticError(err)
{
  // initializers only
}

quint64 CommandResult::callbackId() const
{
  if (!d) {
    return 0;
  }
  return d->callbackId;
}

bool CommandResult::isFinished() const
{
  if (!d) {
    return true;
  }
  return d->finished;
}

bool CommandResult::hasError() const
{
  if (!d) {
    return staticError;
  }
  return d->error;
}

void CommandResult::setCallback(CommandResult::Callback callback)
{
  if (isFinished()) {
    qWarning() << "CommandResult::setCallback() after command finished";
  }
  if (d->callback) {
    qWarning() << "CommandResult::setCallback() after setCallback";
  }
  d->callback = callback;
}

void CommandResult::done(bool error)
{
  if (isFinished()) {
    qWarning() << "CommandResult::done() after command finished";
    return;
  }
  d->finished = true;
  d->error = error;
  if (d->callback) {
    d->callback(this);
  }
}

void CommandResult::reset()
{
  if (!isFinished()) {
    qWarning() << "CommandResult::reset() during command execution";
  }
  d->finished = false;
  d->error = false;
}
