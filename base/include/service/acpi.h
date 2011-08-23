// -*- Mode: C++ -*-

#pragma once

#include <nul/types.h>
#include <nul/compiler.h>

class DmarTableParser {
  union {
    const char *_base;
    const struct PACKED {
      uint32  signature;      // DMAR
      uint32  length;
      // ...
    }          *_header;
  };

public:
  
  enum Type {
    DHRD = 0,
    RMRR = 1,
    ATSR = 2,
    RHSA = 3,
  };

  enum ScopeType {
    MSI_CAPABLE_HPET = 4,
  };

  class DeviceScope {
    union {
      const char *_base;
      struct PACKED {
        uint8  type;
        uint8  length;
        uint16 _res;
        uint8  id;
        uint8  start_bus;
        uint8 path[];
      } *_elem;
    };
    size_t _size_left;

  public:
    uint8     id()   const { return _elem->id; }
    ScopeType type() const { return ScopeType(_elem->type); }
    
    uint16  rid() {
      // We don't know what to do if this is not plain PCI.
      if (_elem->length <= 6) return 0;
      assert(_elem->length - 6 == 2); 
      
      return (_elem->start_bus << 8)
        | (_elem->path[0] << 3)
        | _elem->path[1];
    }

    bool has_next() const {
      return _size_left > _elem->length; }

    DeviceScope next()
    {
      assert(has_next());
      assert(_elem->length > 0);
      return DeviceScope(_base + _elem->length, _size_left - _elem->length);
    }

    DeviceScope(const char *base, size_t size_left)
      : _base(base), _size_left(size_left)
    {
      //printf("%p len = %u (%u), type = %u\n", base, _elem->length, size_left, _elem->type);
    }
  };

  class Dhrd {
    union {
      const char *_base;
      struct PACKED {
        uint8  flags;
        uint8  _res;
        uint16 segment;
        uint64 base;
            
        char     scope[];
      } *_elem;
    };
    
    size_t _size_left;

    
  public:

    uint8  flags()   const { return _elem->flags; }
    uint16 segment() const { return _elem->segment; }
    uint64 base()    const { return _elem->base; }

    bool has_scopes()
    {
      return (_size_left - (_elem->scope - _base)) >= 6;
    }

    DeviceScope get_scope()
    {
      return DeviceScope(_elem->scope, _size_left - (_elem->scope - _base));
    }

    Dhrd(const char *base, size_t size_left)
      : _base(base), _size_left(size_left)
    { }
  };

  class Element {
    union {
      const char *_base;
      const struct PACKED {
        uint16  type;
        uint16  length;
      }          *_elem;
    };
    size_t        _size_left;

  public:
    Type type()    const { return Type(_elem->type); }
    bool has_next() const { return _size_left > _elem->length; }

    Element next()
    {
      assert(has_next());
      assert(_elem->length > 0);
      return Element(_base + _elem->length, _size_left - _elem->length);
    }

    Dhrd get_dhrd() {
      assert(_elem->type == DHRD);
      return Dhrd(_base + 4, _elem->length - 4);
    }
  
  public:
    Element(const char *base, size_t size_left)
      : _base(base), _size_left(size_left)
    { }
  };

  Element get_element()
  {
    return Element(_base + 48, _header->length - 48);
  }

  DmarTableParser(const char *base)
    : _base(base)
  {
    assert(_header->signature == 0x52414d44U); // "DMAR"
  }
};

// EOF
