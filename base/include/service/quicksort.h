// -*- Mode: C++ -*-
/** @file
 * In-place Quicksort (wikipedia style).
 *
 * Copyright (C) 2010, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */


#pragma once

template <typename T>
class Quicksort {
private:
  Quicksort() { }

public:
  typedef bool (* smallereq_fn)(T const &a, T const &b);

protected:
  static void swap(T &a, T &b)
  {
    T tmp = a;
    a = b;
    b = tmp;
  }

  static int partition(smallereq_fn cmp, T a[], int left, int right, int pivot_i)
  {
    swap(a[pivot_i], a[right]);
    T &pivot = a[right];
    int si = left;

    for (int i = left; i < right; i++)
      if (cmp(a[i], pivot))
        swap(a[i], a[si++]);

    swap(a[si], a[right]);
    return si;
  }

public:
  static void quicksort(smallereq_fn cmp, T a[], int left, int right)
  {
    if (right > left) {
      int pivot_i = partition(cmp, a, left, right, (left + right) / 2);
      quicksort(cmp, a, left, pivot_i - 1);
      quicksort(cmp, a, pivot_i + 1, right);
    }
  }

};

// EOF
