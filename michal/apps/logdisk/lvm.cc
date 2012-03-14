/*
 * Disk benchmarking.
 *
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
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

class Lvm
{
  enum {
    SECTOR_SHIFT = 9L,
    SECTOR_SIZE = 1L << SECTOR_SHIFT,
    LABEL_SIZE = SECTOR_SIZE,
    LABEL_SCAN_SECTORS = 4L,
    LABEL_SCAN_SIZE = LABEL_SCAN_SECTORS << SECTOR_SHIFT,
    INITIAL_CRC = 0xf597a6cf,
    ID_LEN = 32,
    NAME_LEN = 128,
    FMTT_VERSION = 1,
    MDA_HEADER_SIZE = 512,
  };

#define LABEL_ID "LABELONE"
#define LVM2_LABEL "LVM2 001"
#define FMTT_MAGIC "\040\114\126\115\062\040\170\133\065\101\045\162\060\116\052\076"

  template<typename T>
  static T xlate(const T v) {
#ifdef __BYTE_ORDER__
    static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "Big endian is not supported");
#endif
    return v;
  }

  template<typename T>
  class Xlated {
    T _val;
  public:
    Xlated(T val) : _val(xlate(val)) {}
    T val() const { return xlate(_val); }
  } __attribute__ ((packed));

  /* On disk - 32 bytes */
  struct label_header {
    int8           id[8];       /* LABELONE */
    Xlated<uint64> sector;	/* Sector number of this label */
    Xlated<uint32> crc;         /* From next field to end of sector */
    Xlated<uint32> offset;      /* Offset from start of struct to contents */
    int8           type[8];     /* LVM2 001 */

    bool check_crc() {
      uint32 c = calc_crc(INITIAL_CRC, reinterpret_cast<uint8*>(&offset),
                          LABEL_SIZE - (reinterpret_cast<uint8*>(&offset) - reinterpret_cast<uint8*>(this)));
      return c == crc.val();
    };
  } __attribute__ ((packed));

  /* On disk */
  struct disk_locn {
    Xlated<uint64> offset;	/* Offset in bytes to start sector */
    Xlated<uint64> size;	/* Bytes */
  } __attribute__ ((packed));

  /* On disk */
  struct pv_header {
    int8 pv_uuid[ID_LEN];

    /* This size can be overridden if PV belongs to a VG */
    Xlated<uint64> device_size; /* Bytes */

    /* NULL-terminated list of data areas followed by */
    /* NULL-terminated list of metadata area headers */
    struct disk_locn disk_areas[0]; /* Two lists */
  } __attribute__ ((packed));

  /* On disk */
  struct raw_locn {
    enum { IGNORED = 0x00000001 };
    Xlated<uint64> offset;	/* Offset in bytes to start sector */
    Xlated<uint64> size;	/* Bytes */
    Xlated<uint32> checksum;
    uint32 flags;
    raw_locn() : offset(0), size(0), checksum(0), flags(0) {}
    bool is_ignored() { return flags & IGNORED; }
  } __attribute__ ((packed));

  /* On disk */
  /* Structure size limited to one sector */
  struct mda_header {
    Xlated<uint32> checksum;	/* Checksum of rest of mda_header */
    int8 magic[16];		/* To aid scans for metadata */
    Xlated<uint32> version;
    Xlated<uint64> start;       /* Absolute start byte of mda_header */
    Xlated<uint64> size;	/* Size of metadata area */
    struct raw_locn raw_locns[0]; /* NULL-terminated list */

    bool check_crc() {
      uint32 c = calc_crc(INITIAL_CRC, reinterpret_cast<uint8*>(magic), MDA_HEADER_SIZE - sizeof(checksum));
      return c == checksum.val();
    }
  } __attribute__ ((packed));


  template <class T>
  struct List {
    T *head;
    List() : head(0) {}
    void add(T *elem) {
      elem->next = head;
      head = elem;
    }
  };

  template <class T>
  class ListWithCount : public List<T> {
  public:
    unsigned count;
    ListWithCount() : List<T>(), count(0) {}
    void add(T *elem) { List<T>::add(elem); count++; }
  };

  class Uuid {
    int8 _uuid[ID_LEN];
    static const char _c[];
    static bool _built_inverse;
    static char _inverse_c[256];

    void _build_inverse(void) const {
      if (_built_inverse) return;

      _built_inverse = true;
      memset(_inverse_c, 0, sizeof(_inverse_c));

      for (const char *ptr = _c; *ptr; ptr++)
	_inverse_c[static_cast<int>(*ptr)] = 1;

    }
  public:
    Uuid(const int8 uuid[ID_LEN]) { memcpy(_uuid, uuid, sizeof(_uuid)); };

    bool write_format(char *buffer, uint64 size) const
    {
      int i, tot;
      static unsigned group_size[] = { 6, 4, 4, 4, 4, 4, 6 };

      static_assert(ID_LEN == 32, "Unexpected UUID length");

      /* split into groups separated by dashes */
      if (size < (32 + 6 + 1)) {
	Logging::printf("Couldn't write uuid, buffer too small.");
	return false;
      }

      for (i = 0, tot = 0; i < 7; i++) {
	memcpy(buffer, &_uuid[tot], group_size[i]);
	buffer += group_size[i];
	tot += group_size[i];
	*buffer++ = '-';
      }

      *--buffer = '\0';
      return true;
    }

    bool read_format(const char *buffer)
    {
      unsigned out = 0;

      /* just strip out any dashes */
      while (*buffer) {

	if (*buffer == '-') {
	  buffer++;
	  continue;
	}

	if (out >= ID_LEN) {
	  Logging::printf("Too many characters to be uuid.");
	  return false;
	}

	_uuid[out++] = *buffer++;
      }

      if (out != ID_LEN) {
	Logging::printf("Couldn't read uuid: incorrect number of characters.");
	return false;
      }

      return valid();
    }

    bool valid() const {
      _build_inverse();
      for (unsigned i = 0; i < ID_LEN; i++)
	if (!_inverse_c[_uuid[i]]) {
	  Logging::printf("UUID contains invalid character");
	  return false;
	}
      return true;
    }
  };

  /// Physical volume
  struct PV {
    struct Area {
      uint64 offset, size;
      class Area *next;
      Area(uint64 offset, uint64 size) : offset(offset), size(size), next(0) {}
    };

    const unsigned disknum;
    const Uuid uuid;
    const uint64 size;
    List<Area> da;		// Data areas
    List<Area> mda;		// Metadata areas
    struct PV *next;
    PV(unsigned disknum, const struct pv_header *pvh)
      : disknum(disknum), uuid(pvh->pv_uuid), size(pvh->device_size.val()), next(0)
    {
      const struct disk_locn *dl = pvh->disk_areas;
      uint64 offset;
      while (offset = dl->offset.val()) {
	da.add(new Area(offset, dl->size.val()));
	dl++;
      }
      dl++;
      while (offset = dl->offset.val()) {
	mda.add(new Area(offset, dl->size.val()));
	dl++;
      }

      char str[50];
      uuid.write_format(str, sizeof(str));
      Logging::printf("PV uuid=%s, size=%lld\n", str, size);
    }

  };

  MyDiskHelper *disk;
  ListWithCount<PV> pvs;

  static bool isspace(const char c) {return c == ' ' || c == 0x9 || c == 13 || c == 11;}
  static bool isalnum(const char c) {return (c >='0' && c <= '0') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

  /*
   * Device layer names are all of the form <vg>-<lv>-<layer>, any
   * other hyphens that appear in these names are quoted with yet
   * another hyphen.  The top layer of any device has no layer
   * name.  eg, vg0-lvol0.
   */
  static bool validate_name(const char *n)
  {
    register char c;
    register unsigned len = 0;

    if (!n || !*n)
      return false;

    /* Hyphen used as VG-LV separator - ambiguity if LV starts with it */
    if (*n == '-')
      return false;

    if (!strcmp(n, ".") || !strcmp(n, ".."))
      return false;

    while ((len++, c = *n++))
      if (!isalnum(c) && c != '.' && c != '_' && c != '-' && c != '+')
	return false;

    if (len > NAME_LEN)
      return false;

    return 1;
  }

  /// Scans physical volumes for LVM labels
  unsigned scan_pvs(unsigned disknum)
  {
#define skip_if(cond, msg, ...) if (cond) { Logging::printf(msg " on disk %d - skiping\n", ##__VA_ARGS__, disknum); return false; }
    check1(__res, disk->read_synch(disknum, 0, LABEL_SCAN_SIZE));

    struct label_header *lh = 0;
    for (uint64 sector = 0; sector < LABEL_SCAN_SECTORS; sector++) {
      lh = reinterpret_cast<struct label_header *>(&disk->dma_buffer[sector << SECTOR_SHIFT]);
      if (strncmp(reinterpret_cast<char*>(lh->id), LABEL_ID, sizeof(lh->id)) == 0) {
        skip_if (lh->sector.val() != sector, "Label sector mismatch %llu != %llu", lh->sector.val(), sector);
        skip_if (!lh->check_crc(), "Label checksum incorrect");
        Logging::printf("LVM label header found at sector %llu on disk %u\n", sector, disknum);
        break;
      }
    }

    skip_if (!lh, "No LVM label found");

    if (strncmp(reinterpret_cast<char*>(lh->type), LVM2_LABEL, sizeof(lh->type)) != 0) {
      char type[sizeof(lh->type)+1] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      memcpy(type, lh->type, sizeof(lh->type));
      Logging::printf("Unsupported label type: %s\n", type);
    }

    DiskParameter params;
    check1(__res, disk->get_params(*BaseProgram::myutcb(), disknum, &params));

    struct pv_header *pvh = reinterpret_cast<struct pv_header*>(reinterpret_cast<unsigned>(lh) + lh->offset.val());
    assert(pvh->device_size.val() <= params.sectors * SECTOR_SIZE);

    pvs.add(new PV(disknum, pvh));
#undef skip_if
    return ENONE;
  }

  /// Reads metadata from disks and constructs volume groups
  unsigned find_vgs(unsigned disknum) {
    for (PV *pv = pvs.head; pv; pv = pv->next) {
      for (PV::Area *mda = pv->mda.head; mda; mda = mda->next) {
#define skip_if(cond, msg, ...) if (cond) { Logging::printf(msg " on disk %d at offset %llu - skiping\n", ##__VA_ARGS__, pv->disknum, mda->offset); continue; }
        check1(__res, disk->read_synch(pv->disknum, mda->offset >> SECTOR_SHIFT, MDA_HEADER_SIZE));

	struct raw_locn rlocn;
	{
	  struct mda_header *mdah = reinterpret_cast<struct mda_header*>(disk->dma_buffer);
	  skip_if (!mdah->check_crc(), "Incorrect metadata area header checksum");

	  skip_if (strncmp(reinterpret_cast<char*>(mdah->magic), FMTT_MAGIC, sizeof(mdah->magic)),
                   "Wrong magic number in metadata area header");

	  skip_if (mdah->version.val() != FMTT_VERSION,
                   "Incompatible metadata area header version: %d", mdah->version.val());

	  skip_if (mdah->start.val() != mda->offset,
                   "Incorrect start sector in metadata area header: %llu", mdah->start.val());

	  skip_if (mdah->size.val() != mda->size, // MS: This test is not present in the original LVM implementation.
                   "Metadata size mismatch: %llu != %llu", mdah->size.val(), mda->size);

	  rlocn = mdah->raw_locns[0];
	}

	skip_if (!rlocn.offset.val(), "Metadata offset must not be zero");

        check1(__res, disk->read_synch(pv->disknum, (mda->offset + rlocn.offset.val()) >> SECTOR_SHIFT, NAME_LEN));

	unsigned len = 0;
	while (disk->dma_buffer[len] && !isspace(disk->dma_buffer[len]) && disk->dma_buffer[len] != '{' && len < (NAME_LEN - 1))
	  len++;

	disk->dma_buffer[len]=0;
	if (!validate_name(disk->dma_buffer)) {
	  Logging::printf("Invalid volume group name on disk %d at offset %llu\n", pv->disknum, mda->offset + rlocn.offset.val());
	  continue;
	}

	unsigned wrap = 0;
	if (rlocn.offset.val() + rlocn.size.val() > mda->size)
	  wrap = ((rlocn.offset.val() + rlocn.size.val()) - mda->size);

	if (wrap > rlocn.offset.val()) {
	  Logging::printf("Metadata too large for circular buffer on disk %d", pv->disknum);
	  continue;
	}

	Logging::printf("vg name: %s\n", disk->dma_buffer);

	//TODO: text_vgname_import

      }
    }
    return ENONE;
  };

  Lvm(MyDiskHelper *disk) : disk(disk) {}

public:
  static void find(MyDiskHelper *disk, unsigned disknum)
  {
    Lvm lvm(disk);
    WVNUL(lvm.scan_pvs(disknum));
    WVNUL(lvm.find_vgs(disknum));
  }
};

const char Lvm::Uuid::_c[256] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!#";
char Lvm::Uuid::_inverse_c[256];
bool Lvm::Uuid::_built_inverse = 0;
