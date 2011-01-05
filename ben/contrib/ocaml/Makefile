OCAML  := ocaml-3.11.0

BASE	:= \
	    ocamlopt.opt \
    	stdlib/pervasives.cmi \
	    stdlib/stdlib.cmxa \
    	stdlib/stdlib.a \
	    asmrun/libasmrun.a \

INC		:= \
		callback.h \
		compatibility.h \
		mlvalues.h \
		config.h \
		misc.h \
		memory.h \
		gc.h \
		major_gc.h \
		minor_gc.h \
		freelist.h \
		alloc.h \

INC		:= $(addprefix byterun/, $(INC))
INC		+= config/s.h config/m.h

MODULES	:= printf int32 string list array callback

COPY	:= $(BASE) $(INC) $(addprefix stdlib/,$(foreach mod,$(MODULES),$(mod).cmi $(mod).cmx $(mod).o))

ocamlopt.opt :
	wget http://caml.inria.fr/pub/distrib/ocaml-3.11/$(OCAML).tar.bz2
	tar xjf $(OCAML).tar.bz2
	cd $(OCAML) && ./configure -no-curses -no-tk -no-pthread -no-shared-libs
	$(MAKE) -C $(OCAML) world   | tee $(OCAML)/log.world
	$(MAKE) -C $(OCAML) opt     | tee $(OCAML)/log.opt
	$(MAKE) -C $(OCAML) opt.opt | tee $(OCAML)/log.opt.opt
	cd $(OCAML) && patch -p2 < $(PATCH)
	sed -i 's/^OTHERLIBRARIES.*/OTHERLIBRARIES=str num bigarray/g' $(OCAML)/config/Makefile
	make -C $(OCAML)/asmrun
	make -C $(OCAML)/stdlib allopt
	make -C $(OCAML)/otherlibs/str allopt
	make -C $(OCAML)/otherlibs/num allopt
	make -C $(OCAML)/otherlibs/bigarray allopt
	cp -v  $(addprefix $(OCAML)/,$(COPY)) .
	sed -i 's/^#include "..\/config\//#include "/g' config.h
	rm -v $(OCAML).tar.bz2
	rm -fr $(OCAML)
