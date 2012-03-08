/** @file
 * Region list.
 *
 * Copyright (C) 2009 Bernhard Kauer <bk@vmmon.org>
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

#include <service/helper.h>

struct Region
{
  unsigned long long virt;
  unsigned long long size;
  unsigned long long phys;
  unsigned long long end() const { return virt + size; }

  Region(unsigned long long _virt = 0, unsigned long long _size = 0, unsigned long long _phys = 0)
    : virt(_virt), size(_size), phys(_phys)
  { }
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
  unsigned      count() const { return _count; }
  const Region *list()  const { return _list; }

  /**
   * Find the region to a virtual address.
   */
  Region *find(unsigned long long pos)
  {
    for (Region *r = _list + _count; --r >= _list;)
      if (r->virt <= pos && pos - r->virt <  r->size)
        return r;
    return 0;
  };

  /**
   * Find the virtual address to a physical region.
   */
  unsigned long long find_phys(unsigned long long phys, unsigned long long size)
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
    if (!region.size) return;
    del(region);

    Region *r;
    if (region.virt && (r = find(region.virt-1)) && (r->virt + r->size == region.virt)
        && (r->phys + r->size == region.phys)) {
      region.virt = r->virt;
      region.phys = r->phys;
      region.size+= r->size;
      del(*r);
    }

    if (region.end() && (r = find(region.end())) && (region.end() == r->virt)
        && (region.phys + region.size == r->phys)) {
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
  unsigned long long alloc(unsigned long long size, unsigned align_order, unsigned nb_offset = 0)
  {
    assert(_count < SIZE);
    assert(align_order < 8*sizeof(unsigned long long));

    Region *min = 0;

    for (Region *r = _list; r < _list + _count; r++)
      {
        unsigned long long virt = ((r->end() - size) & ~((1ULL << align_order) - 1)) - nb_offset;
        if ((size + nb_offset <= r->size) && (virt >= r->virt)
            && (!min || min->size > r->size)) min = r;
      }

    if (min) {
      unsigned long long virt = ((min->end() - size) & ~((1ULL << align_order) - 1)) - nb_offset;
      del(Region(virt, size + nb_offset));
      return virt;
    }

    return 0;
  }

  void debug_dump(const char *prefix)
  {
    Logging::printf("Region %s count %d\n", prefix, _count);
    for (Region *r = _list; r < _list + _count && r < _list + SIZE; r++)
      Logging::printf("\t%4d virt %8llx end %8llx size %8llx phys %8llx\n", r - _list, r->virt, r->end(), r->size, r->phys);
  }

  /*
   * Alloc the largest aligned contiguous region from the list.
   */
  Region alloc_max(unsigned align_order) {
    unsigned long long size_max = 0, virt_max = 0;

    assert(_count < SIZE);
    assert(align_order < 8*sizeof(unsigned long long));
    for (Region *r = _list; r < _list + _count; r++)
      {
        unsigned long long virt = (r->virt + (1ULL << align_order) - 1) & ~((1ULL << align_order) - 1);
        if (r->size <= (virt - r->virt)) continue;
        if ((r->size - (virt - r->virt)) > size_max) {
          size_max = r->size - (virt - r->virt);
          virt_max = virt;
        }
      }
    if (virt_max && size_max) del(Region(virt_max, size_max));
    return Region(virt_max, size_max);
  }

 RegionList() : _count(0) {}
};
