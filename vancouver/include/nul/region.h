/*
 * Region list.
 *
 * Copyright (C) 2009 Bernhard Kauer <bk@vmmon.org>
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
#include "service/helper.h"

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
    del(region);

    Region *r;
    if (region.virt && (r = find(region.virt-1)) && r->virt + r->size == region.virt && r->phys + r->size == region.phys) {
      region.virt = r->virt;
      region.phys = r->phys;
      region.size+= r->size;
      del(*r);
    }
    if (region.end() && (r = find(region.end())) && region.end() == r->virt && region.phys + region.size == r->phys) {
      region.size += r->size;
      del(*r);
    }

    _count++;
    assert(_count < SIZE);
    _list[_count-1] = region;
  }

  /**
   * Remove regions from the list.
   */
  void del(Region value)
  {
    for (Region *r = _list ; r < _list + _count; r++)
      {
	if (r->virt >= value.end() || value.virt >= r->end()) continue;
	if (value.virt > r->virt)
	  {
	    // we have to split the current one
	    if (r->end() > value.end())
	      {
		_count++;
		assert(_count < SIZE);
		memmove(r+1, r, (_count - (r - _list) - 1) * sizeof(Region));
		(r+1)->phys += value.end() - r->virt;
		(r+1)->size -= value.end() - (r+1)->virt;
		(r+1)->virt  = value.end();
	      }
	    r->size = value.virt - r->virt;
	  }
	else
	  {
	    // we can remove from the beginning
	    if (value.end() >= r->end())
	      {
		memmove(r, r+1, (_count - (r - _list) - 1) * sizeof(Region));
		_count--;
		r--;
	      }
	    else
	      {
		r->phys += value.end() - r->virt;
		r->size -= value.end() - r->virt;
		r->virt  = value.end();
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


  void debug_dump(const char *prefix)
  {
    Logging::printf("Region %s count %d\n", prefix, _count);
    for (Region *r = _list; r < _list + _count; r++)
      Logging::printf("\t%4d virt %8lx end %8lx size %8lx phys %8lx\n", r - _list, r->virt, r->end(), r->size, r->phys);
  }

 RegionList() : _count(0) {}
};
