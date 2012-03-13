/*
 * Copyright (C) 2011-2012, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

  enum {
    VALUEWIDTH = 2U,
    WIDTH  = 80U,
    HEIGHT = 25U,
    REFILL_PRIO = 255U,
  };

  void get_idle(Hip * hip, unsigned num_cpu_shift) {
    cursor_pos = 1;

    log_cpu_no lcpu_from = num_cpu_shift;
    log_cpu_no lcpu_to   = (2 + 5U * (hip->cpu_count() - num_cpu_shift)) >= (WIDTH - 32U) ? (num_cpu_shift + (WIDTH - 32U) / 5U) : hip->cpu_count();
    lcpu_to = MIN(lcpu_to, hip->cpu_count());

    for (log_cpu_no lcpu = lcpu_from; lcpu < lcpu_to; lcpu++) {
      timevalue time_con = 0;
      ClientData * data = &idle_scs;
      phy_cpu_no pcpu = hip->cpu_physical(lcpu);
      for (unsigned i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
        if (!data->scs[i].idx || data->scs[i].cpu != pcpu) continue;
        time_con += data->scs[i].last[0] - data->scs[i].last[1];
      }

      timevalue rest;
      splitfloat(time_con, rest, pcpu);
      if (time_con >= 100)
        Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH, " 100");
      else
        Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH, "%2llu.%1llu", time_con, rest);

      cursor_pos -= 1;
    }
    memset(_vga_console + VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH, 0, 1);
    cursor_pos = 0;
  }


  unsigned measure(ClientData volatile * data, phy_cpu_no pcpu)
  {
    unsigned i, res = ERESOURCE;

    for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
      if (!data->scs[i].idx || data->scs[i].cpu != pcpu) continue;
      res = ENONE;

      timevalue computetime;
      unsigned res = nova_ctl_sc(data->scs[i].idx, computetime);
      if (res != NOVA_ESUCCESS) return EPERM;

      timevalue diff        = computetime - data->scs[i].last[0]; //XXX handle wraparounds ...
      global_sum[pcpu]      += diff;
      global_sum[pcpu]      -= data->scs[i].last[0] - data->scs[i].last[1];
      global_prio[pcpu][data->scs[i].prio] += diff;
      global_prio[pcpu][data->scs[i].prio] -= data->scs[i].last[0] - data->scs[i].last[1];
      for (unsigned j=9; 0 < j; j--) {
        data->scs[i].last[j]  = data->scs[i].last[j - 1];
      }
      data->scs[i].last[0]  = computetime;
    }
    return res;
  }

  void top_dump_prio(Hip * hip, unsigned num_cpu_shift) {
    unsigned row = 1;
    log_cpu_no lcpu_from = num_cpu_shift;
    log_cpu_no lcpu_to   = 5U * (hip->cpu_count() - num_cpu_shift) >= (WIDTH - 5) ? (num_cpu_shift + (WIDTH - 5) / 5U) : hip->cpu_count();
    lcpu_to = MIN(lcpu_to, hip->cpu_count());

    memset(_vga_console, 0, HEIGHT * WIDTH * VALUEWIDTH);
    _vga_regs.cursor_pos = 0;
    _vga_regs.offset = 0;

    cursor_pos = 4;
    for (unsigned i=lcpu_from; i < lcpu_to; i++) {
      Vprintf::printf(&_putc, _vga_console, "%4u", hip->cpu_physical(i));
      cursor_pos -=1;
    }

    for (unsigned prio=255; prio < 256; prio--) {
      log_cpu_no lcpu;
      for (lcpu=lcpu_from; lcpu < lcpu_to; lcpu++) {
        if (global_prio[hip->cpu_physical(lcpu)][prio]) break;
      }
      if (lcpu != lcpu_to) {
        cursor_pos = 0;
        Vprintf::printf(&_putc, _vga_console + row * WIDTH * VALUEWIDTH, "%3u", prio);
        cursor_pos -= 1;
        for (log_cpu_no llcpu=lcpu_from; llcpu < lcpu_to; llcpu++) {
          phy_cpu_no pcpu = hip->cpu_physical(llcpu);
          timevalue rest, val = global_prio[pcpu][prio];
          splitfloat(val, rest, pcpu);
          if (val >= 100)
            Vprintf::printf(&_putc, _vga_console + row * WIDTH * VALUEWIDTH, " 100");
          else
            Vprintf::printf(&_putc, _vga_console + row * WIDTH * VALUEWIDTH, "%2llu.%1llu", val, rest);

          cursor_pos -= 1;
        }
        memset(_vga_console + row * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH, 0, 1);
        row += 1;
        cursor_pos = 0;
      }
      if (row >= HEIGHT) {
        Vprintf::printf(&_putc, _vga_console + (HEIGHT - 1) * WIDTH * VALUEWIDTH, "more prios available - to many to show ...");
        break;
      }
    }
  }

  void measure_scs(Hip * hip) {
    ClientData * data = &own_scs;
    data->next = &idle_scs;
    idle_scs.next = _storage.next();

    do {
      for (phy_cpu_no cpunr=0; cpunr < hip->cpu_desc_count(); cpunr++) {
        Hip_cpu const *cpu = &hip->cpus()[cpunr];
        if (not cpu->enabled()) continue;
        if (EPERM == measure(data, cpunr)) Logging::printf("measure error ... cpu %u client '%s' \n", cpunr, data->name);
      }
    } while (data = _storage.next(data));
  }

  void top_dump_scs(Utcb &utcb, Hip * hip, unsigned &client_num, unsigned num_cpu_shift) {

    memset(_vga_console, 0, HEIGHT * WIDTH * VALUEWIDTH);
    _vga_regs.cursor_pos = 0;
    _vga_regs.offset = 0;

    log_cpu_no lcpu_from = num_cpu_shift;
    log_cpu_no lcpu_to   = (2 + 5U * (hip->cpu_count() - num_cpu_shift)) >= (WIDTH - 32U) ? (num_cpu_shift + (WIDTH - 32U) / 5U) : hip->cpu_count();
    lcpu_to = MIN(lcpu_to, hip->cpu_count());
    phy_cpu_no pcpu_from = hip->cpu_physical(lcpu_from);
    phy_cpu_no pcpu_to   = hip->cpu_physical(lcpu_to - 1) + 1;

    cursor_pos = 1;
    for (unsigned i=lcpu_from; i < lcpu_to; i++) {
      Vprintf::printf(&_putc, _vga_console, "%4u", hip->cpu_physical(i));
      cursor_pos -= 1;
    }
    memset(_vga_console + cursor_pos * 2, 0, 1);
    cursor_pos = 1 + 5 * (1 + lcpu_to - lcpu_from);
    Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH, "idle");
    memset(_vga_console + VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
    cursor_pos = 0;

    ClientData * data = &own_scs;
    ClientData * client;
    unsigned client_count = HEIGHT - 1, res, more = 0;
    unsigned client_select = client_num < (HEIGHT - 2) ? client_count - client_num : 2; //client_count - (client_num % (HEIGHT - 2));
    unsigned client_offset = client_num < client_count - 2 ? 0 : client_num - client_count + 2;
    data->next = _storage.next();

    if (client_offset) {
      Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH * HEIGHT - 12 * VALUEWIDTH, "...%u", client_offset);
      memset(_vga_console + VALUEWIDTH * WIDTH * HEIGHT - 12 * VALUEWIDTH +  cursor_pos * VALUEWIDTH - 1, 0, 1);
      cursor_pos = 0;
    }

    do {
      if (client_offset) client_offset--;
      else {
        utcb.head.mtr = 1; //manage utcb by hand (alternative using add_frame, drop_frame which copies unnessary here)
        client = reinterpret_cast<ClientData *>(reinterpret_cast<unsigned long>(data));

        res = get_usage(utcb, client);
        if (client_count > 1) {
          cursor_pos = 1 + 5 * (1 + lcpu_to - lcpu_from);
          Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%s", client->name);
          memset(_vga_console + client_count * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
          if (res != ENONE) {
            Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%s", "error");
            continue;
          }

          unsigned i = 1;
          cursor_pos = 0;
          if (client_count == client_select)
            Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH - 1 * VALUEWIDTH,"%c ", 175);
          else
            Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH - 1 * VALUEWIDTH,"  ");
          memset(_vga_console + client_count * VALUEWIDTH * WIDTH + (cursor_pos - 2) * VALUEWIDTH, 0, 1);
          while (i < sizeof(utcb.msg) / sizeof(utcb.msg[0]) && utcb.msg[i] != ~0UL) {
            //Logging::printf("pcpu_from %u <= utcb.msg[i] %u < pcu_to %u (logical %u - %u)\n", pcpu_from, utcb.msg[i], pcpu_to, lcpu_from, lcpu_to);
            if (pcpu_from <= utcb.msg[i] && utcb.msg[i] < pcpu_to) {
              cursor_pos = 1;
              for (unsigned j=pcpu_from; j < pcpu_to; j++) { //physical -> logical cpu mapping
                Hip_cpu const *cpu = &hip->cpus()[j];
                if (utcb.msg[i] == j) break;
                if (not cpu->enabled()) continue;
                cursor_pos += 5;
              }
              //cursor_pos = utcb.msg[i] * 5;
              if (utcb.msg[i+1] >= 100)
                Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, " 100 ");
              else
                Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%2u.%1u ", utcb.msg[i+1], utcb.msg[i+2]);
              memset(_vga_console + client_count * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
            }
            i += 3;
          }
          client_count --;
        } else {
          more += 1;
          cursor_pos = 0;
          Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH * 2 - 12 * VALUEWIDTH, "...%u", more);
          memset(_vga_console + VALUEWIDTH * WIDTH * 2 - 12 * VALUEWIDTH +  cursor_pos * VALUEWIDTH - 1, 0, 1);
        }
      }
    } while (data = _storage.next(data));
    get_idle(hip, num_cpu_shift);
  }

  void sort(timevalue * ptr, unsigned * pos, unsigned n) {
    bool exchange;
    unsigned i, tmp_pos;
    timevalue tmp;

    assert(n > 0);
    do {
      exchange = false;
      for (i=0; i < n - 1; i++) {
        if (ptr[i] < ptr[i + 1]) {
          tmp = ptr[i]; ptr[i] = ptr[i + 1], ptr[i + 1] = tmp;
          tmp_pos = pos[i]; pos[i] = pos[i + 1]; pos[i + 1] = tmp_pos;
          exchange = true;
        }
      }
      n -= 1;
    } while (exchange && n > 1);
  }

  void top_dump_thread(ClientData volatile * data, unsigned num_sc) {
    unsigned sort_len = sizeof(data->scs[num_sc].last) / sizeof(data->scs[num_sc].last[0]) - 1;
    timevalue tmp_sort[sort_len];
    timevalue tmp_list[sort_len];
    unsigned i, pos[sort_len];
    for (i=0; i < sort_len; i++) {
      tmp_sort[i] = tmp_list[i] = data->scs[num_sc].last[i] - data->scs[num_sc].last[i + 1];
      pos[i] = i;
    }

    sort(tmp_sort, pos, sort_len);

    unsigned row = HEIGHT - sort_len - 2;
    cursor_pos = 0;
    Vprintf::printf(&_putc, _vga_console + row * VALUEWIDTH * WIDTH, "Aggregated runtime per interval (us/mint) - '%s':\n", data->scs[num_sc].name);
    row++;
    cursor_pos = 0;
    Vprintf::printf(&_putc, _vga_console + row * VALUEWIDTH * WIDTH, "   us/mint");
    cursor_pos = 11;
    for (i=sort_len; i > 0; i--) {
      Vprintf::printf(&_putc, _vga_console + row * VALUEWIDTH * WIDTH, "%si%d", i ? "": " ", 1 - i);
      cursor_pos -= 1;
    }
    memset(_vga_console + row * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH, 0, 1);

    char const * msg;
    for (i=0; i < sort_len; i++) {
      cursor_pos = 0; row +=1;
      Vprintf::printf(&_putc, _vga_console + row * VALUEWIDTH * WIDTH, "%10llu", tmp_sort[i]);
      cursor_pos -= 1;
      memset(_vga_console + row * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH, 0, 1);

      if (pos[i] > 0)
        if (tmp_list[pos[i]] <= tmp_list[pos[i]-1])
          if ((pos[i]+1 >= sort_len) || tmp_list[pos[i]] >= tmp_list[pos[i]+1])
            msg = "__/"; else msg = "\\_/";
        else
          if ((pos[i]+1 >= sort_len) || tmp_list[pos[i]] >= tmp_list[pos[i]+1])
            msg = "___"; else msg = "\\__";
      else
        if (tmp_list[pos[i]] < tmp_list[pos[i]+1])
          msg = "\\__"; else msg = "___";
      Vprintf::printf(&_putc, _vga_console + row * VALUEWIDTH * WIDTH + (sort_len - 1 - pos[i]) * 4 * VALUEWIDTH, "%s", msg);
      cursor_pos -= 1;
      memset(_vga_console + row * VALUEWIDTH * WIDTH + (sort_len - 1 - pos[i]) * 4 * VALUEWIDTH + cursor_pos * VALUEWIDTH, 0, 1);
    }
    cursor_pos = 0;
  }

  void top_dump_client(unsigned client_num, unsigned interval, Hip * hip, unsigned num_sc) {
    unsigned i = 0, show_max, show_start;
    ClientData * data = &own_scs;
    data->next = _storage.next();

    while (client_num != i && (data = _storage.next(data))) { i++; }
    if (!data) return;

    memset(_vga_console, 0, HEIGHT * WIDTH * VALUEWIDTH);
    _vga_regs.cursor_pos = 0;
    _vga_regs.offset = 0;

    show_max = HEIGHT - 12 - 3 - 3;
    show_start = (num_sc >= show_max) ? num_sc - show_max + 1 : 0;

    Logging::printf("application: %s\n", data->name);
    Logging::printf("\n  cpu prio  util(  max)      t(us)   q(us) thread name\n");

    for (i = show_start; i < MIN(show_start + show_max, sizeof(data->scs) / sizeof(data->scs[0])); i++) {
      if (!data->scs[i].idx) {
        Logging::printf("%c --- ---- ---.-(---.-) ---------- ------- unused slot %u %c\n", num_sc == i ? 175 : ' ', i, num_sc == i ? 174 : ' ');
        continue;
      }

      timevalue rest, val = data->scs[i].last[0] - data->scs[i].last[1];
      splitfloat(val, rest, data->scs[i].cpu);

      Logging::printf("%c %3u %4u %3llu.%1llu(%3u.%1u) %10llu %7u %s %c\n", num_sc == i ? 175 : ' ',
          data->scs[i].cpu, data->scs[i].prio,
		      val, rest, data->scs[i].prio > REFILL_PRIO ? data->scs[i].quantum * 100 / (interval * 1000) : 100,
		      data->scs[i].prio > REFILL_PRIO ? (data->scs[i].quantum * 100 / (interval * 100)) % 10 : 0,
          data->scs[i].last[0] - data->scs[i].last[1], data->scs[i].quantum, data->scs[i].name, num_sc == i ? 174 : ' ');
    }
    Logging::printf("\nmint=%ums, tsc f=%ukHz, bus f=%ukHz\n", interval, hip->freq_tsc, hip->freq_bus);
    Logging::printf("legend: q - quantum, mint - measure interval, t - execution time per mint\n");

    if (data->scs[num_sc % (sizeof(data->scs)/sizeof(data->scs[0]))].idx)
      top_dump_thread(data, num_sc % (sizeof(data->scs)/sizeof(data->scs[0])));
  }

  void splitfloat(timevalue &val, timevalue &rest, phy_cpu_no pcpu) {
    val = val * 1000;
    if (val && global_sum[pcpu]) Math::div64(val, global_sum[pcpu]);
    if (val < 1000) {
      timevalue util = val;
      if (util && global_sum[pcpu]) Math::div64(util, 10U);
      rest = val - util * 10;
      val  = util;
    } else {
      val = 100; rest = 0;
    }
  }

  static unsigned cursor_pos;
  static void _putc(void * data, int value) {
    value &= 0xff;
    if (value == -1 || value == - 2) return; // -1 start, -2 end, can be used for locking
    Screen::vga_putc(0xf00 | value, reinterpret_cast<unsigned short *>(data), cursor_pos);
  }

