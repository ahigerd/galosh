#ifndef GALOSH_ALGORITHMS_H
#define GALOSH_ALGORITHMS_H

#include <QString>
#include <QPoint>
#include <utility>
#include <time.h>

template <typename T>
std::pair<T, T> in_order(T a, T b)
{
  if (a <= b) {
    return { a, b };
  } else {
    return { b, a };
  }
}


template <typename T>
class QtAssociativeIterable
{
public:
  QtAssociativeIterable(T begin, T end) : _begin(begin), _end(end) {}
  QtAssociativeIterable(const QtAssociativeIterable&) = default;
  QtAssociativeIterable(QtAssociativeIterable&&) = default;

  inline T begin() { return _begin; }
  inline T end() { return _end; }

private:
  T _begin, _end;
};

template <typename T>
QtAssociativeIterable<typename T::key_value_iterator> pairs(T& container)
{
  return QtAssociativeIterable(container.keyValueBegin(), container.keyValueEnd());
}

template <typename T>
void pairs(const T& container)
{
  static_assert(false, "use cpairs for const iterator");
}

template <typename T>
QtAssociativeIterable<typename T::const_key_value_iterator> cpairs(const T& container)
{
  return QtAssociativeIterable(container.constKeyValueBegin(), container.constKeyValueEnd());
}


template <typename T>
class QtKeyIterable
{
public:
  QtKeyIterable(T begin, T end) : _begin(begin), _end(end) {}
  QtKeyIterable(const QtKeyIterable&) = default;
  QtKeyIterable(QtKeyIterable&&) = default;

  inline T begin() { return _begin; }
  inline T end() { return _end; }

private:
  T _begin, _end;
};

// Qt's key iterators are always const
template <typename T>
QtKeyIterable<typename T::key_iterator> keys(const T& container)
{
  return QtKeyIterable(container.keyBegin(), container.keyEnd());
}


template <typename VALUE, typename ITER, typename ENUM = int>
class EnumeratingIterator
{
public:
  // using difference_type = typename ITER::difference_type;
  using value_type = std::pair<ENUM, VALUE&>;

  EnumeratingIterator() : _index(0) {}
  EnumeratingIterator(ITER iter) : _iter(iter), _index(0) {}
  EnumeratingIterator(ENUM index, ITER iter) : _iter(iter), _index(index) {}
  EnumeratingIterator(const EnumeratingIterator&) = default;
  EnumeratingIterator(EnumeratingIterator&&) = default;

  EnumeratingIterator& operator=(const EnumeratingIterator&) = default;
  EnumeratingIterator& operator=(EnumeratingIterator&&) = default;

  inline value_type operator*() const { return { _index, *_iter }; }
  inline bool operator==(const EnumeratingIterator& other) { return _iter == other._iter; }
  inline bool operator!=(const EnumeratingIterator& other) { return _iter != other._iter; }
  inline EnumeratingIterator& operator++() { ++_index; ++_iter; return *this; }
  inline EnumeratingIterator operator++(int) { return { _index++, _iter++ }; }

private:
  ITER _iter;
  ENUM _index;
};

template <typename VALUE, typename ITER, typename ENUM = int>
class EnumeratingIterable
{
public:
  using iterator = EnumeratingIterator<VALUE, ITER, ENUM>;

  EnumeratingIterable(ITER begin, ITER end) : _begin(begin), _end(end) {}

  inline iterator begin() { return _begin; }
  inline iterator end() { return _end; }

private:
  iterator _begin, _end;
};

template <typename T, typename ENUM = int>
EnumeratingIterable<typename T::value_type, typename T::iterator, ENUM> enumerate(T& container)
{
  return EnumeratingIterable<typename T::value_type, typename T::iterator, ENUM>(container.begin(), container.end());
}

template <typename T, typename ENUM = int>
EnumeratingIterable<const typename T::value_type, typename T::const_iterator, ENUM> enumerate(const T& container)
{
  return EnumeratingIterable<const typename T::value_type, typename T::const_iterator, ENUM>(container.begin(), container.end());
}


template <typename T>
void benchmark(const QString& label, T fn)
{
  timespec startTime, endTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);

  fn();

  clock_gettime(CLOCK_MONOTONIC, &endTime);
  uint64_t elapsed = (endTime.tv_sec - startTime.tv_sec) * 1e9 + (endTime.tv_nsec - startTime.tv_nsec);
  qDebug("%s: %f ms", qPrintable(label), elapsed / 1000.0 / 1000.0);
}


template <typename T>
inline T signum(T val)
{
  return (val < 0 ? -1 : (val > 0 ? 1 : 0));
}

#endif
