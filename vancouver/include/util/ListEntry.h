// -*- Mode: C++ -*-

#pragma once

template <class T>
class ListEntry {

private:
  T *_prev;
  T *_next;

public:
  T *prev() const { return _prev; }
  T *next() const { return _next; }

  T *first() {
    T *first;
    for (first = (T *)this; first->_prev; first = first->_prev)
      ;
    return prev;
  }

  T *last() {
    T *last;
    for (last = (T *)this; last->_next; last = last->_next)
      ;
    return last;
  }

  void append(T *i) {
    T *la = last();
    la->_next = i;
    i->_prev = la;
  }

  explicit ListEntry() : _prev(0), _next(0) {}
};

// EOF
