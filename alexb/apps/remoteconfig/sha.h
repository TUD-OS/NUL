/*
 * \brief   header of sha.c
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#pragma once

#include <service/string.h>
#include <service/logging.h>

class Sha1 {
  public:

  struct Context
  {
    unsigned int index;
    unsigned long blocks;
    unsigned char buffer[64+4];
    unsigned char hash[20];
  };

  static void init(struct Context *ctx);
  static void hash(struct Context *ctx, unsigned char* value, unsigned count);
  static void finish(struct Context *ctx);

  private:
    static inline unsigned int get_w(unsigned char * value, unsigned int round);
    static inline void process_block(struct Context *ctx);

    static inline void ERROR(int result, bool value, const char * msg) {
      if (value) {
        Logging::panic("[sha1]    - %d %s\n", result, msg);
      }
    }
};
