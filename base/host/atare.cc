/** @file
 * ATARE - ACPI table IRQ routing extraction.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"


/**
 * ATARE - ACPI table IRQ routing extraction.
 *
 * This extends the ideas from "ATARE: ACPI Tables and Regular
 * Expressions, Bernhard Kauer, TU Dresden technical report
 * TUD-FI09-09, Dresden, Germany, August 2009".
 *
 * State: testing
 * Features: direct PRT, referenced PRTs, exact name resolution, Routing Entries
 */
class Atare : public StaticReceiver<Atare>
{
  /**
   * A single PCI routing entry.
   */
  struct PciRoutingEntry {
    PciRoutingEntry *  next;
    unsigned      adr;
    unsigned char pin;
    unsigned char gsi;
    PciRoutingEntry(PciRoutingEntry *_next, unsigned _adr, unsigned char _pin, unsigned char _gsi) : next(_next), adr(_adr), pin(_pin), gsi(_gsi) {}
  };


  /*
   * A reference to a named entry.
   * Name is an absolute name in the ACPI namespace without the leading backslash.
   */
  struct NamedRef {

    NamedRef *next;
    const unsigned char *ptr;
    unsigned len;
    const char *name;
    int namelen;
    PciRoutingEntry *routing;
    NamedRef(NamedRef *_next, const unsigned char *_ptr, unsigned _len, const char *_name, int _namelen)
      : next(_next), ptr(_ptr), len(_len), name(_name), namelen(_namelen), routing(0) {}
  };

  NamedRef *_head;


  void debug_show_items() {
    Atare::NamedRef *ref = _head;
    for (unsigned i=0; ref; ref = ref->next, i++)
      Logging::printf("at: %3d %p+%04x type %02x %.*s\n", i, ref->ptr, ref->len, ref->ptr[0], ref->namelen, ref->name);
  }


  void debug_show_routing() {
    if (!Atare::search_ref(_head, 0, "_PIC", false)) Logging::printf("at: APIC mode unavailable - no _PIC method\n");

    for (Atare::NamedRef *dev = _head; dev; dev = dev->next)
      if (dev->ptr[0] == 0x82) {
        unsigned bdf = Atare::get_device_bdf(_head, dev);
        Logging::printf("at: %04x:%02x:%02x.%x tag %p name %.*s\n",
          bdf >> 16, (bdf >> 8) & 0xff, (bdf >> 3) & 0x1f, bdf & 7, dev->ptr - 1, 4, dev->name + dev->namelen - 4);
        for (Atare::PciRoutingEntry *p = dev->routing; p; p = p->next)
          Logging::printf("at:\t  parent %p addr %02x_%x gsi %x\n",
        dev->ptr - 1, p->adr >> 16, p->pin, p->gsi);
      }
  }



  /**
   * Returns the number of bytes for this package len.
   */
  static unsigned get_pkgsize_len(const unsigned char *data) { return ((data[0] & 0xc0) && (data[0] & 0x30)) ? 0 :  1 + (data[0] >> 6); }

  /**
   * Read the pkgsize.
   */
  static unsigned read_pkgsize(const unsigned char *data) {
    unsigned res = data[0] & 0x3f;
    for (unsigned i=1; i < get_pkgsize_len(data); i++)
      res += data[i] << (8*i - 4);
    return res;
  }


  /**
   * Returns whether this is a nameseg.
   */
  static bool name_seg(const unsigned char *res) {
    for (unsigned i=0; i < 4; i++)
      if (!((res[i] >= 'A' && res[i] <= 'Z') || (res[i] == '_') || (i && res[i] >= '0' && res[i] <= '9')))
	return false;
    return true;
  }

  /**
   * Get the length of the nameprefix;
   */
  static unsigned get_nameprefix_len(const unsigned char *table) {
    const unsigned char *res = table;

    if (*res == 0x5c) res++;
    else while (*res == 0x5e) res++;

    if (*res == 0x2e) res++;
    else if (*res == 0x2f) res+=2;
    return res - table;
  }

  /**
   * Get the length of a name.
   *
   * Returns 0 on failure or the length.
   */
  static unsigned get_name_len(const unsigned char *table) {
    const unsigned char *res = table;

    if (*res == 0x5c) res++;
    else while (*res == 0x5e) res++;

    if (*res == 0x2e) {
      if (name_seg(res + 1) && name_seg(res + 5))  return res - table + 9;
    }
    else if (*res == 0x2f) {
      unsigned i;
      for (i=0; i < res[1]; i++)
	if (!name_seg(res + 2 + i*4)) return 0;
      if (i)  return  res - table + 2 + 4*i;
    }
    else if (name_seg(res)) return res - table + 4;
    return res - table;
  }


  /**
   * Calc an absname.
   */
  static void get_absname(NamedRef *parent, const unsigned char *name, int &namelen, char *res, unsigned skip=0) {

    unsigned nameprefixlen = get_nameprefix_len(name);
    namelen = namelen - nameprefixlen;

    // we already have an absname?
    if (*name == 0x5c || !parent)
      memcpy(res, name + nameprefixlen, namelen);
    else {
	int parent_namelen = parent->namelen - skip*4;
	if (parent_namelen < 0) parent_namelen = 0;
	for (unsigned pre = 0; name[pre] == 0x5e && parent_namelen; pre++)
	  parent_namelen -=4;
	memcpy(res, parent->name, parent_namelen);
	memcpy(res + parent_namelen, name +  nameprefixlen, namelen);
	namelen += parent_namelen;
      }
  }

  /**
   * Get the length of a data item and the data value.
   *
   * Note: We support only numbers here.
   */
  static unsigned read_data(const unsigned char *data, unsigned &length) {
    switch (data[0]) {
    case 0:     length = 1; return 0;
    case 1:     length = 1; return 1;
    case 0xff:  length = 1; return 0xffffffff;
    case 0x0a:  length = 2; return data[1];
    case 0x0b:  length = 3; return data[1] | (data[2] << 8);
    case 0x0c:  length = 5; return data[1] | (data[2] << 8) | (data[3] << 16)  | (data[4] << 24);
    default:    length = 0; return 0;
    }
  }


  /**
   * Search and read a packet with 4 entries.
   */
  static long get_packet4(const unsigned char *table, long len, unsigned *x)
  {
    for (const unsigned char *data = table; data < table + len; data++)
      if (data[0] == 0x12) {

	unsigned pkgsize_len  = get_pkgsize_len(data + 1);
	if (!pkgsize_len || data[1 + pkgsize_len] != 0x04) continue;

	unsigned offset = 1 + pkgsize_len + 1;
	for (unsigned i=0; i < 4; i++) {
	  unsigned len;
	  x[i] = read_data(data + offset, len);
	  if (!len)  return 0;
	  offset += len;
	}
	return data - table;
      }
    return 0;
  }


  /**
   * Searches for PCI routing information in some region and adds them to dev.
   */
  static void search_prt_direct(NamedRef *dev, NamedRef *ref) {

    unsigned l;
    for (unsigned offset = 0; offset < ref->len; offset += l) {
      unsigned x[4];
      unsigned packet = get_packet4(ref->ptr + offset, ref->len - offset, x);
      l = 1;
      if (!packet) continue;

      l = packet + read_pkgsize(ref->ptr + offset + packet + 1);
      dev->routing = new PciRoutingEntry(dev->routing, x[0], x[1], x[3]);
    }
  }

  /**
   * Searches for PCI routing information by following references in
   * the PRT method and adds them to dev.
   */
  static void search_prt_indirect(NamedRef *head, NamedRef *dev, NamedRef *prt) {

    unsigned found = 0;
    unsigned name_len;

    for (unsigned offset = get_pkgsize_len(prt->ptr + 1); offset < prt->len; offset+= name_len) {
      name_len = get_name_len(prt->ptr + offset);
      if (name_len) {

	// skip our name
	if (!found++) continue;

	char name[name_len+1];
	memcpy(name, prt->ptr + offset, name_len);
	name[name_len] = 0;
	NamedRef *ref = search_ref(head, dev, name, true);
	if (ref)  search_prt_direct(dev, ref);
      }
      else name_len = 1;
    }
  }


  /**
   * Return a single value of a namedef declaration.
   */
  static unsigned get_namedef_value(NamedRef *head, NamedRef *parent, const char *name) {

    NamedRef *ref = search_ref(head, parent, name, false);
    if (ref && ref->ptr[0] == 0x8) {

      unsigned name_len = get_name_len(ref->ptr + 1);
      unsigned len;
      unsigned x = read_data(ref->ptr + 1 + name_len, len);
      if (len)  return x;
    }
    return 0;
  }


  /**
   * Search some reference per name, either absolute or relative to some parent.
   */
  static NamedRef *search_ref(NamedRef *head, NamedRef *parent, const char *name, bool upstream) {

    int slen = strlen(name);
    int plen = parent ? parent->namelen : 0;
    for (int skip=0 ; skip <= plen / 4; skip++) {
      int n = slen;
      char output[slen + plen];
      get_absname(parent, reinterpret_cast<const unsigned char *>(name), n, output, skip);
      for (NamedRef *ref = head; ref; ref = ref->next)
	if (ref->namelen == n && !memcmp(ref->name, output, n))
	  return ref;
      if (!upstream) break;
    }
    return 0;
  }


  /**
   * Return a single bdf for a device struct by combining different device properties.
   */
  static unsigned long long get_device_bdf(NamedRef *head, NamedRef *dev) {

    unsigned adr = get_namedef_value(head, dev, "_ADR");
    unsigned bbn = get_namedef_value(head, dev, "_BBN");
    unsigned seg = get_namedef_value(head, dev, "_SEG");
    return (seg << 16) + (bbn << 8) + ((adr >> 16)<<3) + (adr & 0xffff);
  }


  /**
   * Add all named refs from a table and return the list head pointer.
   */
  static NamedRef *add_refs(const unsigned char *table, unsigned len, NamedRef *res = 0) {

    for (const unsigned char *data = table; data < table + len; data++)
      if ((data[0] == 0x5b && data[1] == 0x82) // devices
	  || (data[0] == 0x08)  // named objects
	  || (data[0] == 0x10)  // scopes
	  || (data[0] == 0x14)  // methods
	  ) {
	if (data[0] == 0x5b) data++;

	bool has_pkgsize = (data[0] == 0x10) || (data[0] == 0x14) || (data[0] == 0x82);
	unsigned pkgsize_len  = 0;
	unsigned pkgsize = 0;
	if (has_pkgsize) {
	  pkgsize_len = get_pkgsize_len(data + 1);
	  pkgsize = read_pkgsize(data + 1);
	}
	int name_len = get_name_len(data + 1 + pkgsize_len);
	if ((has_pkgsize && !pkgsize_len) || !name_len || data + pkgsize > table + len) continue;

	// fix previous len
	if (res && !res->len) res->len = data - res->ptr;

	// search for the parent in this table
	NamedRef *parent = res;
	for (; parent; parent = parent->next)
	  if (parent->ptr < data && parent->ptr + parent->len > data)
	    break;

	// to get an absolute name
	char *name = new char[name_len + (parent ? parent->namelen : 0)];
	get_absname(parent, data + 1 + pkgsize_len, name_len, name);
	res = new NamedRef(res, data, pkgsize, name, name_len);

	// at least skip the header
	data += pkgsize_len;
      }

    // fix len of last item
    if (res && !res->len) res->len = table + len - res->ptr;
    return res;
  }


  /**
   * Add the PCI routing information to the devices.
   */
  static void add_routing(NamedRef *head) {
    for (NamedRef *dev = head; dev; dev = dev->next)
      if (dev->ptr[0] == 0x82) {

	NamedRef *prt = search_ref(head, dev, "_PRT", false);
	if (prt) {
	  search_prt_direct(dev, prt);
	  search_prt_indirect(head, dev, prt);
	}
      }
  }


public:


  bool  receive(MessageAcpi &msg)
  {
    if (msg.type != MessageAcpi::ACPI_GET_IRQ) return false;

    Logging::printf("at: ATARE - search for %x_%x parent %x\n", msg.bdf, msg.pin, msg.parent_bdf);

    // find the device
    for (Atare::NamedRef *dev = _head; dev; dev = dev->next)
      if (dev->ptr[0] == 0x82 && Atare::get_device_bdf(_head, dev) == msg.parent_bdf) {

	// look for the right entry
	for (Atare::PciRoutingEntry *p = dev->routing; p; p = p->next)
	  if ((p->adr >> 16) == ((msg.bdf >> 3) & 0x1f) && (msg.pin == p->pin)) {

	    Logging::printf("at: ATARE - found %x for %x_%x parent %x\n", p->gsi, msg.bdf, msg.pin, msg.parent_bdf);

	    msg.gsi = p->gsi;
	    return true;
	  }
      }
    Logging::printf("at: ATARE - search for %x_%x parent %x failed\n", msg.bdf, msg.pin, msg.parent_bdf);
    return false;
  }


  Atare(DBus<MessageAcpi> &bus_acpi, unsigned debug) : _head(0) {

    // add entries from the SSDT
    MessageAcpi msg("DSDT");
    if (bus_acpi.send(msg, true) && msg.table)
      _head = add_refs(reinterpret_cast<unsigned char *>(msg.table), msg.len, _head);

    // and from the SSDTs
    msg.name = "SSDT";
    for (; bus_acpi.send(msg, true) && msg.table; msg.instance++)
      _head = add_refs(reinterpret_cast<unsigned char *>(msg.table), msg.len, _head);

    add_routing(_head);

    if (debug & 1) debug_show_items();
    if (debug & 2) debug_show_routing();

    Logging::printf("at: ATARE initialized\n");

  };
};

class GsiOverride : public StaticReceiver<GsiOverride>
{
  unsigned _bdf;
  unsigned _gsi;
public:
  bool  receive(MessageAcpi &msg)
  {
    if (msg.type != MessageAcpi::ACPI_GET_IRQ) return false;
    if (msg.bdf != _bdf) return false;
    msg.gsi = _gsi;
    return true;
  }

  GsiOverride(unsigned bdf, unsigned gsi) : _bdf(bdf), _gsi(gsi) {}
};

PARAM_HANDLER(atare,
	      "atare:debug=0 - provide GSI lookup to PCI drivers.")
{
  mb.bus_acpi.add(new Atare(mb.bus_acpi, ~argv[0] ? argv[0] : 0), Atare::receive_static<MessageAcpi>);
}

PARAM_HANDLER(gsi_override,
	      "gsi_override:bdf,gsi - allow to override GSI interrupts.",
	      "Example: 'gsi_override:0xfa,19' specifies gsi 19 for device 0:1f:2.")
{
  mb.bus_acpi.add(new GsiOverride(argv[0], argv[1]), GsiOverride::receive_static<MessageAcpi>);
}
