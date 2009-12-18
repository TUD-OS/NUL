/*
 * Region list.
 *
 * Copyright (C) 2009 Bernhard Kauer <kauer@tudos.org>
 *
 * This file is part of Vancouver.nova.
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
#include <cassert>

struct Region
{
  unsigned long virt;
  unsigned long size;
  unsigned long phys;
  unsigned long end() { return virt + size; }
  Region(unsigned long _virt=0, unsigned long _size=0, unsigned long _phys=0) : virt(_virt), size(_size), phys(_phys) {}
};

/**
 * A region allocator.
 *
 */
template <unsigned SIZE>
class RegionList
{

private:

  unsigned _count;
  Region  _list[SIZE];

public:
  Region *find(unsigned long pos)
  {
    for (Region *r = _list + _count; --r >= _list;)
      if (r->virt <= pos && pos - r->virt <  r->size)
	return r;
    return 0;
  };

  /**
   * Find a the virtual address to a physical region.
   */
  unsigned long find_phys(unsigned long phys, unsigned long size)
  {
    for (Region *r = _list + _count; --r >= _list;)
      if (r->phys <= phys && r->size >= size && phys - r->phys <=  r->size - size )
	return r->virt + phys - r->phys;
    return 0;
  };

  /**
   * Add a region to the list.
   */
  void add(Region region)
  {
    if (!region.size)  return;

    // Logging::printf("add %lx+%lx\n", region.virt, region.size);
    del(region);
    _count++;
    assert(_count < SIZE);
    _list[_count-1] = region;
  }

  /**
   * Remove regions from the list.
   */
  void del(Region value)
  {
    //Logging::printf("del %8lx+%lx\n", value.virt, value.size);
    for (unsigned i = 0; i < _count; i++) {
      Region *r = &_list[i];

      // Skip this entry, if it is not touched by value.
      if (r->virt >= value.end() || value.virt >= r->end()) continue;

      if (value.virt > r->virt) {
	// We have to split the current entry. The new entry is
	// appended to the end of the list.
	if (value.end() < r->end()) {
	  assert(_count < SIZE);
	  Region &last = _list[_count];

	  last.virt = value.end();
	  last.size = r->end() - value.end();
	  last.phys = r->phys + last.size;

	  _count++;
	}
	r->size = value.virt - r->virt;
      } else {
	if (value.end() >= r->end()) {
	  // Remove this entry by switching it with the last one.
	  *r = _list[_count-- - 1];
	  // As there is now a new entry at this point, rerun this
	  // loop iteration. If we removed the last item, the loop
	  // condition fails and we are done.
	  i--; continue;
	} else {
	  unsigned long off = r->virt - value.end();
	  r->phys += off;
	  r->size -= off;
	  r->virt += off;
	}
      }
    }
  }

  /**
   * Alloc a region from the list.
   */
  unsigned long alloc(unsigned long size, unsigned align_order)
  {
    assert(align_order < 8*sizeof(unsigned long));
    for (Region *r = _list; r < _list + _count; r++)
      {
	unsigned long virt = (r->end() - size) & ~((1ul << align_order) - 1);
	if (size <= r->size && virt >= r->virt)
	  {
	    del(Region(virt, size));
	    return virt;
	  }
	}
    return 0;
  }

  /**
   * Sort region list.
   */
  void sort()
  {
    // It's small: Use Bubble Sort! :-D
    for (unsigned i = 0; i < _count; i++)
      for (unsigned j = i + 1; j < _count; j++) {
	if (_list[i].virt > _list[j].virt) {
	  Region temp;
	  temp = _list[j];
	  _list[j] = _list[i];
	  _list[i] = temp;
	}
      }
  }

  void debug_dump(const char *prefix)
  {
    Logging::printf("Region %s count %d\n", prefix, _count);
    for (Region *r = _list; r < _list + _count; r++)
      Logging::printf("\t%4d virt %8lx end %8lx size %8lx phys %8lx\n", r - _list, r->virt, r->end(), r->size, r->phys);
  }

 RegionList() : _count(0) {}
};
