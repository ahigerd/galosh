#ifndef GALOSH_REFABLE_H
#define GALOSH_REFABLE_H

#include <QSet>

template <typename CRTP> class Refable;
template <typename T> class Ref;
template <typename T> class MRef;
template <typename T> uint qHash(const Ref<T>& value, uint seed);

template <typename T>
class Ref
{
protected:
  friend class Refable<T>;
  template <typename T2>
  friend uint qHash(const Ref<T2>& value, uint seed);
  using Source = Refable<T>;

  Ref(Source* src) : src(src) { if (src) src->add(this); }

  Source* src;

public:
  Ref(std::nullptr_t) : src(nullptr) {}
  Ref(const Ref& other) : src(other.src) { if (src) src->add(this); }
  Ref(const MRef<T>& other) : Ref(other.src) {}
  ~Ref() { if (src) src->remove(this); }

  operator bool() const { return src; }
  bool operator!() const { return !src; }
  bool operator==(const Ref<T>& other) const { return src == other.src; }
  bool operator==(const T* other) const { return src == other; }
  bool operator!=(const Ref<T>& other) const { return src != other.src; }
  bool operator!=(const T* other) const { return src != other; }
  auto operator<=>(const Ref<T>& other) const { return src <=> other.src; }
  auto operator<=>(const T* other) const { return src <=> other; }

  Ref& operator=(const Ref& other)
  {
    src = other.src;
    if (src) src->add(this);
    return *this;
  }

  Ref& operator=(const MRef<T>& other)
  {
    src = other.src;
    if (src) src->add(this);
    return *this;
  }

  const T& operator*() const {
    if (!src) {
      throw std::range_error("dereferencing null ref");
    }
    return *static_cast<const T*>(src);
  }

  const T* operator->() const {
    if (!src) {
      throw std::range_error("dereferencing null ref");
    }
    return static_cast<const T*>(src);
  }
};

template <typename T>
class MRef : public Ref<T>
{
protected:
  friend class Refable<T>;
  friend class Ref<T>;
  using Ref<T>::src;

  MRef(Refable<T>* src) : Ref<T>(src) {}

public:
  MRef(std::nullptr_t) : Ref<T>(nullptr) {}
  MRef(const Ref<T>& other) = delete;
  MRef(const MRef<T>& other) : Ref<T>(other) {}

  MRef& operator=(const Ref<T>& other) = delete;

  MRef& operator=(const MRef<T>& other)
  {
    src = other.src;
    if (src) src->add(this);
    return *this;
  }

  T& operator*() const {
    if (!Ref<T>::src) {
      throw std::range_error("dereferencing null ref");
    }
    return *static_cast<T*>(Ref<T>::src);
  }

  T* operator->() const {
    if (!Ref<T>::src) {
      throw std::range_error("dereferencing null ref");
    }
    return static_cast<T*>(src);
  }

  operator Ref<T>() { return Ref<T>(src); }
  operator Ref<T>() const { return Ref<T>(src); }
};

template <typename CRTP>
class Refable
{
public:
  using RefPtr = ::Ref<CRTP>*;
  using Ref = ::Ref<CRTP>;
  using RefR = const ::Ref<CRTP>&;
  using MRef = ::MRef<CRTP>;
  using MRefR = const ::MRef<CRTP>&;
  using value_type = CRTP;

  Refable() {}

  Refable(Refable&& other)
  : refs(other.refs)
  {
    *this = std::move(other);
  }

  Refable& operator=(Refable&& other)
  {
    for (RefPtr ref : other.refs) {
      ref->src = this;
    }
    other.refs.clear();
    return *this;
  }

  virtual ~Refable()
  {
    for (RefPtr ref : refs) {
      ref->src = nullptr;
    }
  }

  MRef operator&() { return MRef(this); }
  Ref operator&() const { return Ref(const_cast<CRTP*>(static_cast<const CRTP*>(this))); }

private:
  friend class ::Ref<CRTP>;
  friend class ::MRef<CRTP>;
  void add(RefPtr ref) { refs << ref; }
  void remove(RefPtr ref) { refs.remove(ref); }

  QSet<RefPtr> refs;
};

template <typename T>
bool operator==(const Refable<T>* lhs, const typename Refable<T>::Ref& rhs)
{
  return rhs == lhs;
}

template <typename T>
uint qHash(const Ref<T>& value, uint seed)
{
  return qHash(value.src, seed);
}

#endif
