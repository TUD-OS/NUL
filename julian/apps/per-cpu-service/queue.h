// -*- Mode: C++ -*-

#pragma once

template<class T>
class Queue {

  T * volatile _head;

public:

  class ListElement {
  public:
    T * volatile next;
  };

  void enqueue(T *e) {
    T *head;
    do {
      head     = _head;
      e->next  = head;
    } while (not __sync_bool_compare_and_swap(&_head, head, e));

  }

  T *dequeue() {
    T *head;
    do {
      head = _head;
      if (head == NULL) return NULL;
    } while (not __sync_bool_compare_and_swap(&_head, head, head->next));
    return head;
  }

  Queue() : _head(NULL) { }
};

// EOF
