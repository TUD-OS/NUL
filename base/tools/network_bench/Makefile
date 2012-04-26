OBJS=tcp_sender tcp_recv
all:	$(OBJS)
.PHONY:	all
tcp_recv: tcp_sender.c
	$(CC) -DRECV -o $@ $<
clean:
	rm $(OBJS)