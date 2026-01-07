#ifndef GALOSH_COMMANDRESULT_H
#define GALOSH_COMMANDRESULT_H

#include <QSharedData>
#include <QExplicitlySharedDataPointer>
#include <QObject>
#include <memory>
#include <functional>

class CommandResult
{
public:
  using Callback = std::function<void(const CommandResult*)>;

  static CommandResult fail();
  static CommandResult success();

  CommandResult();
  CommandResult(const CommandResult&) = default;
  CommandResult(CommandResult&&) = default;
  CommandResult& operator=(const CommandResult&) = default;
  CommandResult& operator=(CommandResult&&) = default;

  quint64 callbackId() const;
  bool isFinished() const;
  bool wasAborted() const;
  bool hasError() const;

  void setCallback(Callback callback);

  template <typename T>
  void setCallback(T* context, void(T::* callback)(const CommandResult*)) {
    setCallback([=](const CommandResult* r) { if (context) (context->*callback)(r); });
  }

  template <typename T>
  void setCallback(T* context, void(T::* callback)()) {
    setCallback([=](const CommandResult*) { if (context) (context->*callback)(); });
  }

  void done(bool error);
  void abort();
  void reset();

private:
  CommandResult(bool err);

  struct Impl : public QSharedData {
    Impl(quint64 id);

    const quint64 callbackId;
    Callback callback;
    bool finished;
    bool error;
    bool aborted;
  };

  QExplicitlySharedDataPointer<Impl> d;
  bool staticError;
};

#endif
