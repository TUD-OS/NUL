/*
 * Sigma0 portal functions.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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
PT_FUNC_NORETURN(do_pf,
#if 0
	_free_phys.debug_dump("free phys");
	_free_virt.debug_dump("free virt");
	_virt_phys.debug_dump("virt->phys");
#endif
		 Logging::panic("got #PF at %llx eip %x error %llx esi %x edi %x ecx %x\n", utcb->qual[1], utcb->eip, utcb->qual[0], utcb->esi, utcb->edi, utcb->ecx);
	)
PT_FUNC(do_map,
	// make sure we have enough words to reply
	//Logging::printf("\t\t%s(%x, %x, %x, %x) pid %x\n", __func__, utcb->head.mtr.value(), utcb->msg[0], utcb->msg[1], utcb->msg[2], utcb->head.pid);
	assert(~utcb->head.mtr.untyped() & 1);
	return Mtd(0, utcb->head.mtr.untyped()/2).value();
	)
PT_FUNC(do_gsi_boot,
	utcb->eip = reinterpret_cast<unsigned long>(&do_gsi_wrapper);
	)
PT_FUNC_NORETURN(do_gsi,
		 bool shared = utcb->msg[1] >> 8;
		 unsigned char res;
		 unsigned irq = utcb->msg[0];
		 {
		   SemaphoreGuard s(_lock);
		   Logging::printf("%s(%x) initial vec %x\n", __func__, irq,  utcb->msg[1]);
		 }
		 MessageIrq msg(shared ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ, utcb->msg[1] & 0xff);
		 while (!(res = semdown(irq)))
		   {
		     SemaphoreGuard s(_lock);
		     _mb->bus_hostirq.send(msg);
		     if (msg.line == 1)
		       COUNTER_SET("absto", _timeouts.timeout());
		   }
		 Logging::panic("%s(%x) request failed with %x\n", __func__, irq, res);
		 )
PT_FUNC(do_startup,
	unsigned short client = (utcb->head.pid & 0xffe0) >> 5;
	ModuleInfo *modinfo = _modinfo + client;
	{
	  SemaphoreGuard s(_lock);
	  Logging::printf("[%02x] eip %lx hip %p mem %p size %lx\n",
			  client, modinfo->rip, modinfo->hip, modinfo->mem, modinfo->physsize);
	}

	utcb->eip = modinfo->rip;
	utcb->esp = HIP_ADDRESS;
	utcb->head.mtr = Mtd(MTD_RIP_LEN | MTD_RSP, 0);

	utcb->add_mappings(true, reinterpret_cast<unsigned long>(modinfo->mem), modinfo->physsize, MEM_OFFSET, 0x1c | 1);
	utcb->add_mappings(true, reinterpret_cast<unsigned long>(modinfo->hip), 0x1000, utcb->esp, 0x1c | 1);
	)

PT_FUNC(do_request,
	unsigned short client = (utcb->head.pid & 0xffe0) >> 5;
	ModuleInfo *modinfo = _modinfo + client;
	COUNTER_INC("request");

	// Logging::printf("[%02x] request (%x,%x,%x) mtr %x\n", client, utcb->msg[0],  utcb->msg[1],  utcb->msg[2], utcb->head.mtr.value());
	// XXX check whether we got something mapped and do not map it back but clear the receive buffer instead
	SemaphoreGuard l(_lock);
	if (utcb->head.mtr.untyped() < 0x1000)
	  {
	    switch (utcb->msg[0])
	      {
	      case REQUEST_PUTS:
		{
		  char * buffer = reinterpret_cast<PutsRequest *>(utcb->msg+1)->buffer;
		  if (convert_client_ptr(modinfo, buffer, 4096)) goto fail;
		  if (modinfo->log)
		    Logging::printf("[%02x, %lx] %4096s\n",  client, modinfo->console, buffer);
		  utcb->msg[0] = 0;
		  break;
		}
	      case REQUEST_STDIN_ATTACH:
		handle_attach<StdinConsumer>(modinfo, modinfo->prod_stdin, utcb);
		break;
	      case REQUEST_DISKS_ATTACH:
		handle_attach<DiskConsumer>(modinfo, modinfo->prod_disk, utcb);
		break;
	      case REQUEST_TIMER_ATTACH:
		handle_attach<TimerConsumer>(modinfo, modinfo->prod_timer, utcb);
		break;
	      case REQUEST_NETWORK_ATTACH:
		handle_attach<NetworkConsumer>(modinfo, modinfo->prod_network, utcb);
		break;
	      case REQUEST_IOIO:
		if ((utcb->msg[2] & 0x3) == 2)
		  {
		    // XXX check permissions
		    // XXX move to hostops
		    Logging::printf("[%02x] ioports %x granted\n", client, utcb->msg[2]);
		    utcb->head.mtr = Mtd(1, 1);
		  }
		else
		  Logging::printf("[%02x] ioport request dropped %x ports %x\n", client, utcb->msg[2], utcb->msg[2]>>Utcb::MINSHIFT);
		break;
	      case REQUEST_IOMEM:
		if ((utcb->msg[2] & 0x3) == 1)
		  {
		    unsigned long addr = utcb->msg[2] & ~0xfff;
		    char *ptr = map_self(utcb, addr, Crd(utcb->msg[2]).size());
		    utcb->msg[1] = 0;
		    utcb->msg[2] = Crd(reinterpret_cast<unsigned long>(ptr) >> 12, Crd(utcb->msg[2]).order(),  0x1c | 1).value();
		    utcb->head.mtr = Mtd(1, 1);
		    Logging::printf("[%02x] iomem %lx+%x o %d granted from %x\n", client, addr, Crd(utcb->msg[2]).size(), Crd(utcb->msg[2]).order(), utcb->msg[2]);
		  }
		else
		  Logging::printf("[%02x] iomem request dropped %x\n", client, utcb->msg[2]);
		break;
	      case REQUEST_IRQ:
		if ((utcb->msg[2] & 0x3) == 3 && (utcb->msg[2] >> Utcb::MINSHIFT) >= _hip->cfg_exc && (utcb->msg[2] >> Utcb::MINSHIFT) <  _hip->cfg_exc + _hip->cfg_gsi)
		  {
		    Logging::printf("[%02x] irq %x granted\n", client, utcb->msg[2]);
		    utcb->head.mtr = Mtd(1, 1);
		  }
		else
		  Logging::printf("[%02x] irq request dropped %x pre %x nr %x\n", client, utcb->msg[2], _hip->cfg_exc, utcb->msg[2] >> Utcb::MINSHIFT);
		break;
	      case REQUEST_DISK:
		{
		  MessageDisk *msg = reinterpret_cast<MessageDisk *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
		    goto fail;
		  else
		    {
		      MessageDisk msg2 = *msg;

		      if (msg2.disknr >= modinfo->disk_count) { msg->error = MessageDisk::DISK_STATUS_DEVICE; return Mtd(utcb->head.mtr.untyped(), 0).value(); }
		      switch (msg2.type)
			{
			case MessageDisk::DISK_GET_PARAMS:
			  if (convert_client_ptr(modinfo, msg2.params, sizeof(*msg2.params))) goto fail;
			  break;
			case MessageDisk::DISK_WRITE:
			case MessageDisk::DISK_READ:
			  if (convert_client_ptr(modinfo, msg2.dma, sizeof(*msg2.dma)*msg2.dmacount)) goto fail;

			  if (msg2.physoffset - MEM_OFFSET > modinfo->physsize) goto fail;
			  if (msg2.physsize > (modinfo->physsize - msg2.physoffset + MEM_OFFSET))
			    msg2.physsize = modinfo->physsize - msg2.physoffset + MEM_OFFSET;
			  msg2.physoffset += reinterpret_cast<unsigned long>(modinfo->mem) - MEM_OFFSET;

			  utcb->msg[0] = find_free_tag(client, msg2.disknr, msg2.usertag, msg2.usertag);
			  if (utcb->msg[0]) break;
			  assert(msg2.usertag);
			  break;
			case MessageDisk::DISK_FLUSH_CACHE:
			  break;
			default:
			  goto fail;
			}
		      msg2.disknr = modinfo->disks[msg2.disknr];
		      msg->error = _mb->bus_disk.send(msg2) ? MessageDisk::DISK_OK : MessageDisk::DISK_STATUS_DEVICE;
		      utcb->msg[0] = 0;
		    }
		}
		break;
	      case REQUEST_CONSOLE:
		{
		  MessageConsole *msg = reinterpret_cast<MessageConsole *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) goto fail;
		  {
		    MessageConsole msg2 = *msg;
		    if ((msg2.type != MessageConsole::TYPE_ALLOC_VIEW &&
			 msg2.type != MessageConsole::TYPE_SWITCH_VIEW &&
			 msg2.type != MessageConsole::TYPE_GET_MODEINFO)
			||
			(msg2.type == MessageConsole::TYPE_ALLOC_VIEW &&
			 (convert_client_ptr(modinfo, msg2.ptr, msg2.size)
			  || convert_client_ptr(modinfo, msg2.name, 4096)
			  || convert_client_ptr(modinfo, msg2.regs, sizeof(*msg2.regs))))
			||
			(msg2.type == MessageConsole::TYPE_GET_MODEINFO &&
			 (convert_client_ptr(modinfo, msg2.info, sizeof(*msg2.info))))
			|| !modinfo->console)
		      break;
		    msg2.id = modinfo->console;
		    // alloc a new console and set the name from the commandline
		    utcb->msg[0] = !_mb->bus_console.send(msg2);
		    if (!utcb->msg[0])      msg->view = msg2.view;
		  }
		}
		break;
	      case REQUEST_HOSTOP:
		{
		  MessageHostOp *msg = reinterpret_cast<MessageHostOp *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) goto fail;

		  switch (msg->type)
		    {
		    case MessageHostOp::OP_GET_MODULE:
		      {
			if (modinfo->mod_count <= msg->module)  break;
			Hip_mem *mod  = _hip->get_mod(modinfo->mod_nr + msg->module);
			char *cstart  = msg->start;
			char *s   = cstart;
			char *cmdline = map_string(utcb, mod->aux);
			unsigned slen = strlen(cmdline) + 1;
			// msg destroyed!

			// align the end of the module to get the cmdline on a new page.
			unsigned long msize =  (mod->size + 0xfff) & ~0xffful;
			if (convert_client_ptr(modinfo, s, msize + slen)) goto fail;

			memcpy(s, map_self(utcb, mod->addr, (mod->size | 0xfff)+1), mod->size);
			s += msize;
			char *p = strstr(cmdline, "sigma0::attach");
			unsigned clearsize = 14;
			if (!p) { p = cmdline + slen - 1; clearsize = 0; }
			memcpy(s, cmdline, p - cmdline);
			memset(s + (p - cmdline), ' ', clearsize);
			memcpy(s + (p - cmdline) + clearsize, p + clearsize, slen - (p - cmdline));
			// build response
			memset(utcb->msg, 0, sizeof(unsigned) + sizeof(*msg));
			utcb->head.mtr = Mtd(sizeof(unsigned) + sizeof(*msg), 0);
			msg->start   = cstart;
			msg->size    = mod->size;
			msg->cmdline = cstart + msize;
			msg->cmdlen  = slen;
		      }
		      break;
		    case MessageHostOp::OP_GET_UID:
		      {
			/**
			 * A client needs a uniq-ID for shared
			 * identification, such as MAC addresses.
			 * For simplicity we use our client number.
			 * Using a random number would also be
			 * possible.  For debugging reasons we
			 * simply increment and add the client number.
			 */
			msg->value = (client << 8) + ++modinfo->uid;
			utcb->msg[0] = 0;
		      }
		      break;
		    case MessageHostOp::OP_ASSIGN_PCI:
		      if (modinfo->dma)
			{
			  utcb->msg[0] = assign_pci_device(modinfo->cap_pd, msg->value, msg->len);
			  Logging::printf("assign_pci() PD %x bdf %lx vfbdf %x = %x\n", client, msg->value, msg->len, utcb->msg[0]);
			  break;
			}
		    case MessageHostOp::OP_ATTACH_IRQ:
		      Logging::printf("assign_gsi PD %x gsi %lx\n", client, msg->value);
		      assign_gsi(_hip->cfg_exc + (msg->value & 0xff), modinfo->cpunr);
		      utcb->msg[0] = 0;
		      break;
		    case MessageHostOp::OP_ATTACH_MSI:
		      attach_msi(msg, modinfo->cpunr);
		      utcb->msg[0] = 0;
		      break;
		    case MessageHostOp::OP_ALLOC_IOIO_REGION:
		    case MessageHostOp::OP_ALLOC_IOMEM:
		    case MessageHostOp::OP_GUEST_MEM:
		    case MessageHostOp::OP_ALLOC_FROM_GUEST:
		    case MessageHostOp::OP_VIRT_TO_PHYS:
		    case MessageHostOp::OP_NOTIFY_IRQ:
		    default:
		      // unhandled
		      Logging::printf("[%02x] unknown request (%x,%x,%x) dropped \n", client, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
		    }
		}
		break;
	      case REQUEST_TIMER:
		{
		  MessageTimer *msg = reinterpret_cast<MessageTimer *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
		    utcb->msg[0] = ~0x10u;
		  else
		    if (msg->type == MessageTimer::TIMER_REQUEST_TIMEOUT)
		      {
			msg->nr = client;
			if (_mb->bus_timer.send(*msg))
			  utcb->msg[0] = 0;
		      }
		}
		break;
	      case REQUEST_TIME:
		{
		  MessageTime msg;
		  if (_mb->bus_time.send(msg))
		    {
		      // we assume the same mb->clock() source here
		      *reinterpret_cast<MessageTime *>(utcb->msg+1) = msg;
		      utcb->head.mtr = Mtd((sizeof(msg)+2*sizeof(unsigned) - 1)/sizeof(unsigned), 0);
		      utcb->msg[0] = 0;
		    }
		}
		break;
	      case REQUEST_NETWORK:
		{
		  MessageNetwork *msg = reinterpret_cast<MessageNetwork *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
		    utcb->msg[0] = ~0x10u;
		  else
		    {
		      MessageNetwork msg2 = *msg;
		      if (convert_client_ptr(modinfo, msg2.buffer, msg2.len)) goto fail;
		      msg2.client = client;
		      _mb->bus_network.send(msg2);
		    }
		}
		break;
	      case REQUEST_PCICFG:
		{
		  MessagePciConfig *msg = reinterpret_cast<MessagePciConfig *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
		    utcb->msg[0] = ~0x10u;
		  else
		    utcb->msg[0] = !_mb->bus_hwpcicfg.send(*msg);
		}
		break;
	      case REQUEST_ACPI:
		{
		  MessageAcpi *msg = reinterpret_cast<MessageAcpi *>(utcb->msg+1);
		  if (utcb->head.mtr.untyped()*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
		    utcb->msg[0] = ~0x10u;
		  else
		    if (msg->type == MessageAcpi::ACPI_GET_IRQ)
		      utcb->msg[0] = !_mb->bus_acpi.send(*msg, true);
		  break;
		}
	      default:
		Logging::printf("[%02x] unknown request (%x,%x,%x) dropped \n", client, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
	      }
	    return utcb->head.mtr.value();
	  fail:
	    utcb->msg[0] = ~0x10u;
	    utcb->head.mtr = Mtd(1, 0);
	  }
	else
	  {
	    Logging::printf("[%02x] second request %x for %llx at %x -> killing\n", client, utcb->head.mtr.value(), utcb->qual[1], utcb->eip);
	    if (revoke(Crd(client, 4), true))
	      {
		_lock.up();
		Logging::panic("kill connection to %x failed", client);
	      }
	  }
	)
