/**
 * TCP throughput measurement.
 *
 * Bernhard Kauer <bk@vmmon.org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/mman.h>
#include <sys/timeb.h>


#define BUFFERSIZE (1<<24)
#define SENDSIZE   (1<<19)
#define SOCKSIZE   (1<<18)

#define CHECK(X) ({ int res; res = X; if (res < 0) { perror(#X); return -1; }; res; })

int main(int argc, char **argv) {
  int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  int prot = PROT_READ | PROT_WRITE;
  void *buffer = mmap(0, BUFFERSIZE, prot, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
  int i;
  int val = SOCKSIZE;

#ifdef RECV
  CHECK(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val)));
#else
  CHECK(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val)));
#endif
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_port = htons(1234);
  addr.sin_family = AF_INET;

  char * saddr = "127.0.0.1";
  if (argc > 1) saddr = argv[1];
  CHECK(inet_aton(saddr, (struct in_addr *) &addr.sin_addr.s_addr));
#ifdef RECV
  CHECK(bind(fd, (struct sockaddr *) &addr, sizeof(addr)));
  CHECK(listen(fd, 1));
  while (1) {
    long long sum=0;
    int count;
    unsigned offset = 0;
    struct timeb start, end;
    int client = accept(fd, 0, 0);

    ftime(&start);
    for (count = 1; count; sum += count) {
      count = read(client, (char *)buffer + offset, SENDSIZE);
      offset += count;
      if (offset > (BUFFERSIZE - SENDSIZE))
	  offset = 0;
    }
    ftime(&end);
    unsigned diff = (((end.time - start.time))*1000) + (end.millitm - start.millitm);
    diff = (sum>>20)*1000/diff;
    printf("%lld MB -> %d MB/s - %d Mbit/s\n", sum >> 20, diff, diff*8);
    close(client);
  }
#else
  CHECK(connect(fd, (struct sockaddr *) &addr, sizeof(addr)));
  while (1) {
    int i;
    for (i=0; i < (BUFFERSIZE/SENDSIZE); i++) {
      if (write(fd, (char *)buffer + (i*SENDSIZE), SENDSIZE) != SENDSIZE) return -1;
    }
  }
#endif
}
