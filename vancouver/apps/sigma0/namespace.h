/*
 * (C) 2010, Alexander Boettcher <boettcher@tudos.org>
 * economic rights: Technische Universitaet Dresden (Germany)
 *
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <sys/utcb.h>
#include <sigma0/namespace.h> //NameSpaceBase

class NameSpace {
    struct entry {
      unsigned pt_base;
      unsigned pt_order;
      char name[64];
    } services[10];

public:
  NameSpace() : services() {}



    template<class T>
    unsigned long handler(unsigned pid, Utcb * utcb, char * _cmdline, T * alloc, unsigned pt_order)
    {
      unsigned long msg_len;
      NameSpaceBase::MessageNS * msg =
        reinterpret_cast<NameSpaceBase::MessageNS *>(&utcb->msg[1]);

      if (utcb->head.mtr.untyped() < 2)
        return 0;

      msg_len = (utcb->head.mtr.untyped() - 1) * sizeof(unsigned long);
      utcb->msg[utcb->head.mtr.untyped()] = 0;
      msg_len = strnlen(msg->name, msg_len);

      switch (utcb->msg[0]) {
        case NameSpaceBase::REQUEST_NS_RESOLVE:
          {
            if (utcb->head.mtr.typed()) //protocol violation - received mappings, but don't want it
              break;

            //parse cmdline of client
            char * cmdline = strstr(_cmdline, "name::");
            while (cmdline) {
              cmdline += 6;

              char * end = strstr(cmdline," ");
              if (!end)
                end = _cmdline + strlen(_cmdline); //final 0 included in name_len

//              Logging::printf("[%u, 0x%x] request for %lx %x | %s | %s\n", Cpu::cpunr(), pid,
//                msg_len, end - cmdline, msg->name, end - msg_len);
              if (msg_len > (0U + (end - cmdline)) ||
                  0 != memcmp(end - msg_len, msg->name, msg_len)) {
                cmdline = strstr(end, "name::");
                continue;
              }

              //found a service match in cmdline - name::nspace:service
              //check whether nspace:service: is registered
              struct entry * service = services;
              unsigned len = (0U + (end - cmdline)) > sizeof(service->name) ?
                              sizeof(service->name) : end - cmdline;

              while (service < (services + sizeof(services) / sizeof(services[0]))) {
                if (!service->pt_base ||
                    0 != memcmp(service->name, cmdline, len))
                {
                  service ++;
                  continue;
                }
                utcb->msg[0] = 0;
                utcb->head.mtr = Mtd(1,0);

                Sigma0Base::add_mappings(utcb, false, service->pt_base << Utcb::MINSHIFT,
                                         1 << service->pt_order << Utcb::MINSHIFT, 0,
                                         DESC_CAP_ALL);

                return utcb->head.mtr.value();
              }
              // we ignore multiple service declaration for now
              // e.g. name::aaa:log name::bbb:log
              // replace 'break' by 'cmdline = strstr(end, "name::")'
              // if you want to support this
              break;
            }
            break;
          }
          break;
        case NameSpaceBase::REQUEST_NS_REGISTER:
          {
            if (!utcb->head.mtr.typed()) //protocol violation - received no mappings
              break;

            char * cmdline = strstr(_cmdline, "namespace::");
            char * end = strstr(cmdline," ");
            if (!end)
              end = _cmdline + strlen(_cmdline); //final 0 included in name_len

            if (!cmdline || cmdline + 11 >= end)
              break;
            cmdline += 11;

            struct entry * service = services;
            unsigned max_len = sizeof(service->name);

            if (max_len < 0U + (end - cmdline))
              break;
            max_len -= end - cmdline;
            if (msg_len < max_len)
              max_len = msg_len;

            while (service < (services + sizeof(services) / sizeof(services[0])))
            {
              if (service->pt_base) {
                service ++;
                continue;
              }

              if (0 != Cpu::cmpxchg4b(&service->pt_base, 0, 1)) {
                service ++;
                continue;
              }

              memcpy(service->name, cmdline, end - cmdline);
              memcpy(service->name + (end - cmdline), msg->name, max_len);
              memset(service->name + (end - cmdline) + max_len, 0, sizeof(service->name) - (end - cmdline) - max_len);

              service->pt_base  = Crd(utcb->head.crd).base() >> Utcb::MINSHIFT;
              service->pt_order = Crd(utcb->head.crd).order();

              unsigned caprange = alloc->alloc_cap(1 << pt_order, pt_order);
              if (caprange == ~0U)
                utcb->head.crd = Crd(0).value(); //nothing can be received anymore
              else
                utcb->head.crd = Crd(caprange, pt_order, DESC_CAP_ALL).value();

//            Logging::printf(" success - registration %s via %x at %x[%x] cap %x\n",
//                              service->name, pid, service->pt_base, 1 << service->pt_order, utcb->head.crd >> Utcb::MINSHIFT);
              utcb->msg[0] = 0;
              utcb->head.mtr = Mtd(1,0);
              return utcb->head.mtr.value();
            }
            break;
          }
          break;
        default:
          break;
      }
      // if we got something mapped -> unmap it
      if (utcb->head.mtr.typed() > 0)
        nova_revoke(Crd(utcb->head.crd), true);

      return 0;
    }
};
