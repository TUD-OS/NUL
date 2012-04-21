/**
 * @file
 *
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/motherboard.h>
#include <nul/service_disk.h>
#include <nul/sservice.h>
#include <nul/capalloc.h>
#include <nul/program.h>
#include <wvtest.h>

template <typename T> T min(T a, T b) { return (a < b) ? a : b; }
template <typename T> T max(T a, T b) { return (a > b) ? a : b; }


template <class T>
class List {
  T *tail;
public:
  T *head;

  List() : tail(NULL), head(NULL) {}

  void add(T *elem)
  {
    assert(elem);
    elem->next = NULL;
    if (tail)
      tail->next = elem;
    else {
      MEMORY_BARRIER;
      head = elem;
    }
    MEMORY_BARRIER;
    tail = elem;
  }
};

template <class T>
class LockedList : public List<T> {
  Semaphore lock;
public:
  LockedList(cap_sel sm) : List<T>(), lock(sm, true)
  {
    lock.up();
  }

  void add(T *elem)
  {
    SemaphoreGuard l(lock);
    List<T>::add(elem);
  }
};

class Disk {
public:
  enum op { READ, WRITE };
private:
  virtual bool do_read_write(enum op op, unsigned long usertag, unsigned long long sector,
                             unsigned dmacount, DmaDescriptor *dma, unsigned long physoffset, unsigned long physsize) = 0;
public:
  struct Name {
    char *name;
    struct Name *next;

    Name(const char* str) : next(0) {
      name = new char[strlen(str)+1];
      assert(name);
      strcpy(name, str);
    }
  };
  List<Name> names;
  struct stats {
    uint64 bytes[2]; ///< Read/write statistics (in bytes)
  } stats;

  Disk *next;
  Disk(const char *anames[]) : names() {
    for (unsigned i=0; anames[i]; i++)
      names.add(new Name(anames[i]));
  }

  Disk(const char *format, ...) __attribute__ ((format(printf, 2, 3)))
  {
    va_list ap;
    va_start(ap, format);
    char str[30];
    Vprintf::vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    names.add(new Name(str));
  }
  char *get_name() { assert(names.head); return names.head->name; }
  bool read_write(enum op op, unsigned long usertag, unsigned long long sector,
                  unsigned dmacount, DmaDescriptor *dma, unsigned long physoffset, unsigned long physsize)
  {
    unsigned len = DmaDescriptor::sum_length(dmacount, dma);
    stats.bytes[op] += len;
    return do_read_write(op, usertag, sector, dmacount, dma, physoffset, physsize);
  }
  virtual bool flush() = 0;
  virtual bool get_params(DiskParameter &params) = 0;
};

class S0Disk : public Disk {
  DBus<MessageDisk> *bus_disk;
  unsigned disknr;
public:
  S0Disk(DBus<MessageDisk> *bus_disk, unsigned disknr) :
    Disk("%d", disknr), bus_disk(bus_disk), disknr(disknr) {}

  virtual bool do_read_write(op op, unsigned long usertag, unsigned long long sector,
                             unsigned dmacount, DmaDescriptor *dma, unsigned long physoffset, unsigned long physsize)
  {
    MessageDisk msg(op == READ ? MessageDisk::DISK_READ : MessageDisk::DISK_WRITE,
		    disknr, usertag, sector, dmacount, dma, physoffset, physsize);
    return bus_disk->send(msg);
  }

  virtual bool flush()
  {
    MessageDisk msg2(MessageDisk::DISK_FLUSH_CACHE, disknr, 0, 0, 0, 0, 0, 0);
    return bus_disk->send(msg2);
  }

  virtual bool get_params(DiskParameter &params) {
    MessageDisk msg2 = MessageDisk(disknr, &params);
    return bus_disk->send(msg2);
  }
};

typedef DiskProtocol::Segment Segment;

class Partition : public Disk {
  Disk *parent;
  uint64 start, length;
public:
  Partition(Disk *parent, uint64 start, uint64 length, const char *names[]) :
    Disk(names), parent(parent), start(start), length(length) {}

  virtual bool do_read_write(op op, unsigned long usertag, unsigned long long sector,
                             unsigned dmacount, DmaDescriptor *dma, unsigned long physoffset, unsigned long physsize)
  {
    uint64   size = 0;
    if (sector >= length) {
      Logging::printf("disk: %s: attempt to %s behind the partition len:0x%llx sector:0x%llx\n",
                      get_name(), op == READ ? "read" : "write", length, sector);
      return false;
    }
    for (unsigned i = 0; i < dmacount; i++)
      size += dma[i].bytecount;
    if (sector+(size+511)/512 > length) {
      Logging::printf("disk: %s: attempt to %s behind the partition len:0x%llx sector:0x%llx+0x%llx\n",
                      get_name(), op == READ ? "read" : "write", length, sector, (size+511)/512);
      return false;
    }
    return parent->read_write(op, usertag, start+sector, dmacount, dma, physoffset, physsize);
  }

  virtual bool flush() { return parent->flush(); }

  virtual bool get_params(DiskParameter &params) {
    if(!parent->get_params(params)) return false;
    params.sectors = length;
    strcpy(params.name, get_name());
    return true;
  }
};

// per client data
struct DiskClient : public PerCpuIdClientData {
  enum {
    MAXDISKS           = 32,	// Maximum number of disks per client
  };
  DiskProtocol::DiskProducer prod_disk;
  cap_sel 	    sem;
  Disk 	   *disks[MAXDISKS];
  unsigned char   disk_count;
  struct {
    unsigned add_disk:1;
  } perms;

  struct {
    unsigned char disk;
    unsigned long usertag;
  } tags [DiskProtocol::MAXDISKREQUESTS];

  cap_sel deleg_pt;
  char  *dma_buffer;
  size_t dma_size;

  Disk *disk(unsigned num) { return num < disk_count ? disks[num] : NULL; }

  // Align disk clients to allow using the sum of their address and
  // the request number as tag.
  static inline void *operator new (size_t s)
  { return ::new (DiskProtocol::MAXDISKREQUESTS) char[s]; }

  static_assert((DiskProtocol::MAXDISKREQUESTS & (DiskProtocol::MAXDISKREQUESTS-1)) == 0,
		"MAXDISKREQUESTS should be a power of two");
};

class DiskService :
  public NoXlateSService<DiskClient, DiskService>,
  public CapAllocator, public StaticReceiver<DiskService>
{
private:
  LockedList<Disk> disks;
  Motherboard &_mb;
  Semaphore _lock;

  cap_sel _deleg_ec[Config::MAX_CPUS];

  DiskClient *_get_client_from_deleg_pt(cap_sel pt) {
    DiskClient *client = 0;
    for (client = _sessions.next(client); client && pt; client = _sessions.next(client))
      if (client->deleg_pt == pt) {
        return reinterpret_cast<DiskClient *>(reinterpret_cast<unsigned long>(client));
      }
    return NULL;
  }

  unsigned portal_func_delegate(cap_sel pt, Utcb &utcb, Utcb::Frame &input) {
    DiskClient *client = 0;
    //unsigned res;
    Sessions::Guard guard_c(&_sessions, utcb, this);
    client =_get_client_from_deleg_pt(pt);
    if (!client) return EEXISTS;

    Crd consumer_mem = input.translated_cap(0);
    DiskProtocol::DiskConsumer *consumer = reinterpret_cast<DiskProtocol::DiskConsumer *>(consumer_mem.base());
    client->prod_disk = DiskProtocol::DiskProducer(consumer, client->sem);

    unsigned long last_base = 0, last_size = 0;

    for (unsigned i = 1; i < input.typed(); i++) {
      Crd dma_mem = input.translated_cap(i);
      if (dma_mem.value() == 0) return EPROTO;
      if (i == 1) {
	client->dma_buffer = reinterpret_cast<char*>(dma_mem.base());
	input.get_word(client->dma_size);
      }
      // Check that the memory is contiguous
      assert(dma_mem.base() >= last_base + last_size && dma_mem.base() < reinterpret_cast<unsigned long>(client->dma_buffer) + client->dma_size);
      last_base = dma_mem.base();
      last_size = dma_mem.size();
    }

    nova_revoke_self(Crd(pt, 0, DESC_CAP_ALL));
    client->deleg_pt = 0;
    return ENONE;
  }

  static void static_portal_delegate(unsigned pt, DiskService *tls, Utcb *utcb) __attribute__((regparm(1))) {
      utcb->add_frame().head.untyped++;
      Utcb::Frame input = utcb->get_nested_frame();
      utcb->msg[0] = tls->portal_func_delegate(pt, *utcb, input);
      utcb->skip_frame();
      asmlinkage_protect("m"(tls), "m"(utcb));
  }

  unsigned _create_deleg_ecs(Hip &hip) {
    for (phy_cpu_no i = 0; i < hip.cpu_desc_count(); i++) {
      Hip_cpu const &cpu = hip.cpus()[i];
      if (not cpu.enabled()) continue;

      // Create delegation EC
      Utcb *utcb_service;
      _deleg_ec[i] = create_ec4pt(i, &utcb_service);
      assert(_deleg_ec[i]);
#ifdef DISK_SERVICE_IN_S0
      utcb_service->head.crd_translate = Crd(0, 31, DESC_MEM_ALL).value();
#else
      virt = _free_virt.alloc(TODO);
      utcb_service->head.crd = TODO virt;
#endif
    }
    return ENONE;
  }

  /**
   * Find a free disk tag for a client.
   */
  bool find_free_tag(DiskClient *client, unsigned char disknr, unsigned long usertag, unsigned long &tag) {
    assert (disknr < client->disk_count);
    for (unsigned i = 0; i < DiskProtocol::MAXDISKREQUESTS; i++)
      if (!client->tags[i].disk)
	{
	  client->tags[i].disk = disknr + 1;
	  client->tags[i].usertag = usertag;

	  tag = reinterpret_cast<unsigned long>(client);
	  assert((tag & (DiskProtocol::MAXDISKREQUESTS-1)) == 0);
	  tag |= i;
	  return true;
	}
    return false;
  }

  unsigned attach_drives(Utcb &utcb, cap_sel identity)
  {
    unsigned res;
    Sessions::Guard guard_c(&_sessions, utcb, this);
    DiskClient *client = 0;

    if (res = _sessions.get_client_data(utcb, client, identity))
      return res;

    client->perms.add_disk = ParentProtocol::get_quota(utcb, client->pseudonym, "diskadd", 0) == ENONE;

    for (Disk *d = disks.head; d; d = d->next) {
      char diskstr[256];
      for (Disk::Name *name = d->names.head; name; name = name->next) {
	Vprintf::snprintf(diskstr, sizeof(diskstr), "disk::%s", name->name);
	if (ParentProtocol::get_quota(utcb, client->pseudonym, diskstr, 0) == ENONE) {
	  client->disks[client->disk_count++] = d;
	  break;
	}
      }
    }

    return ENONE;
  }

  virtual unsigned new_session(DiskClient *client) {
    return attach_drives(*BaseProgram::myutcb(), client->get_identity());
  }

  virtual unsigned handle_request(DiskClient *client, unsigned op, Utcb::Frame &input, Utcb &utcb, bool &free_cap)
    {
      SemaphoreGuard l(_lock);
      unsigned res;

      switch (op) {
      case DiskProtocol::TYPE_GET_DISK_COUNT:
	  utcb << static_cast<mword>(client->disk_count);
	  return ENONE;
      case DiskProtocol::TYPE_GET_PARAMS:
	{
	  DiskParameter param;
	  unsigned disk;

	  if (input.get_word(disk))     return EPROTO;
	  Disk *d = client->disk(disk);
	  if (!d) return EPERM;

	  bool ok = d->get_params(param);
	  if (ok) utcb << param;
	  return ok ? ENONE : ERESOURCE;
	}
      case DiskProtocol::TYPE_GET_MEM_PORTAL: {
	Crd sem(input.received_item(0));
	client->sem = sem.cap();
	free_cap = false;

	cap_sel deleg_pt = alloc_cap();
	res = nova_create_pt(deleg_pt, _deleg_ec[utcb.head.nul_cpunr], reinterpret_cast<unsigned long>(static_portal_delegate), 0);

	assert(res == NOVA_ESUCCESS);

	client->deleg_pt = deleg_pt;
	utcb << Utcb::TypedMapCap(deleg_pt);
	return ENONE;
      }
      case DiskProtocol::TYPE_READ:
      case DiskProtocol::TYPE_WRITE: {
	DmaDescriptor *dma;
	unsigned disk, dmacount;
	unsigned long usertag;
	unsigned long long sector;
	if (input.get_word(disk))     return EPROTO;
	if (input.get_word(usertag))  return EPROTO;
	if (input.get_word(sector))   return EPROTO;
	if (input.get_word(dmacount)) return EPROTO;
	dma = reinterpret_cast<DmaDescriptor*>(input.get_ptr());
	if (input.unconsumed() * sizeof(unsigned) != dmacount*sizeof(*dma))
	  return EPROTO;

	unsigned long internal_tag;
	if (disk >= client->disk_count)
	  return ERESOURCE;
	if (!find_free_tag(client, disk, usertag, internal_tag))
	  return ERESOURCE;

	Disk *d = client->disks[disk];
	bool ok;
	ok = d->read_write(op == DiskProtocol::TYPE_READ ? Disk::READ : Disk::WRITE,
			   internal_tag, sector, dmacount, dma,
			   reinterpret_cast<unsigned long>(client->dma_buffer), client->dma_size);

	return ok ? ENONE : EABORT;
      }
      case DiskProtocol::TYPE_FLUSH_CACHE:
	{
	  unsigned disk;
	  if (input.get_word(disk))     return EPROTO;
	  Disk *d = client->disk(disk);
	  if (!d) return EPERM;

	  return d->flush() ? ENONE : ERESOURCE;
	}
      case DiskProtocol::TYPE_ADD_LOGICAL_DISK:
	{
	  if (!client->perms.add_disk) return EPERM;

	  unsigned len;
	  const char *names[5];
	  unsigned name_cnt;
	  input.get_word(name_cnt);
	  for (unsigned i=0; i<name_cnt && i < (sizeof(names)/sizeof(names[0])-1); i++) {
	    len = 0;
	    names[i] = input.get_zero_string(len);
	    if (!len) return EPROTO;
	  }
	  names[name_cnt] = NULL;

	  Segment *segment = reinterpret_cast<Segment*>(input.get_ptr());
	  static_assert(sizeof(Segment)%sizeof(unsigned) == 0, "Bad size of segment data");
	  if (input.unconsumed()*sizeof(unsigned) % sizeof(Segment) != 0) return EPROTO;
	  len = input.unconsumed()*sizeof(unsigned)/sizeof(Segment);
	  if (len == 0) return EPROTO;

	  while (len--) {
	    Disk *disk = client->disk(segment->disknum);
	    if (!disk) return EPERM;
	    add_disk(new Partition(disk, segment->start_lba, segment->len_lba, names));
	    len=0;		// TODO
	  }
	  return ENONE;
	}
      case DiskProtocol::TYPE_CHECK_NAME:
        {
          unsigned disknum;
          bool match = false;
	  if (input.get_word(disknum))     return EPROTO;
          Disk *disk = client->disk(disknum);
          if (!disk) return EPERM;
          unsigned len;
          const char *name = input.get_zero_string(len);

          for (Disk::Name *dn = disk->names.head; dn && !match; dn = dn->next)
            match = (strcmp(name, dn->name) == 0);
          Logging::printf("Checking %d for %s: %d\n", disknum, name, match);
          utcb << match;
          return ENONE;
        }
      case DiskProtocol::TYPE_GET_STATS:
        {
          unsigned disknum;
	  if (input.get_word(disknum))     return EPROTO;
          Disk *disk = client->disk(disknum);
          if (!disk) return EPERM;
          utcb << disk->stats;
          return ENONE;
        }
      default:
	Logging::panic("Unknown disk op!!!!\n");
        return EPROTO;
      }
    }

  void add_disk(Disk *disk)
  {
    disks.add(disk);
    char message[200], *end = message+sizeof(message), *start = message;
    Vprintf::snprintf(start, end-start, "disk: Added");
    start += 11;
    for (Disk::Name *name = disk->names.head; name && start < end; name = name->next) {
      Vprintf::snprintf(start, end-start, " '%s'", name->name);
      start += strlen(name->name) + 3;
    }
    Logging::printf("%s\n", message);
  }

public:
  bool receive(MessageDiskCommit &msg)
  {
    // user provided write?
    if (msg.usertag) {
      DiskClient *client = reinterpret_cast<DiskClient*>(msg.usertag & ~(DiskProtocol::MAXDISKREQUESTS-1));
      unsigned short index = msg.usertag & (DiskProtocol::MAXDISKREQUESTS-1);
      MessageDiskCommit item(client->tags[index].disk-1, client->tags[index].usertag, msg.status);
      if (!client->prod_disk.produce(item))
        Logging::panic("s0: [%p] produce disk (%x) failed\n", client, index);
      client->tags[index].disk = 0;
    }
    return true;
  }

  virtual cap_sel alloc_cap(unsigned count = 1)
  { return CapAllocator::alloc_cap(count); }

  virtual void    dealloc_cap(cap_sel c)
  { return CapAllocator::dealloc_cap(c); }

  virtual cap_sel create_ec4pt(phy_cpu_no cpu, Utcb **utcb_out, cap_sel ec = ~0u)
  {
    bool ret;
    MessageHostOp msg = MessageHostOp::create_ec4pt(ec, this, cpu, utcb_out);
    ret = _mb.bus_hostop.send(msg);
    return ret ? msg._create_ec4pt.ec : 0;
  }

  DiskService(Motherboard &mb, unsigned _cap, unsigned _cap_order)
    : CapAllocator(_cap, _cap, _cap_order), disks(alloc_cap()), _mb(mb)
  {
    _lock = Semaphore(alloc_cap());
    unsigned res = nova_create_sm(_lock.sm());
    assert(res == ENONE);
    _lock.up();

    _create_deleg_ecs(*mb.hip());
    _mb.bus_diskcommit.add(this, receive_static<MessageDiskCommit>);
    for (unsigned i = 0; i < _mb.bus_disk.count(); i++) {
      add_disk(new S0Disk(&_mb.bus_disk, i));
    }
    register_service("/disk", *mb.hip());
  }
};


PARAM_HANDLER(service_disk,
	      "disk service - provides access to the physical disks")
{
  unsigned cap_region = alloc_cap_region(1 << 12, 12);
  new DiskService(mb, cap_region, 12);
}
