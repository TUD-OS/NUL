/**
 * @file IP relay daemon
 *
 * This program serves as a proxy to the IP relay controlling the test
 * bed. Compared to the IP relay itself, it supports multiple
 * connections. If somebody else is connected to the IP relay, the
 * user is notified about this and can, with the help of novaboot,
 * wait for the IP relay to become free.
 *
 * This program supports two user priorities. A high priority user can
 * interrupt sessions of low priority users. The low priority is meant
 * for remote (PASSIVE) users, high priority is for local (TUD) users.
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define MAX_CLIENTS 3

// NVT codes (http://www.ietf.org/rfc/rfc854.txt)
#define  SE   "\xF0"
#define  NOP  "\xF1"
#define  AYT  "\xF6"
#define  SB   "\xFA"
#define  WILL "\xFB"
#define  WONT "\xFC"
#define  DO   "\xFD"
#define  DONT "\xFE"
#define  IAC  "\xFF"

const string are_you_there(IAC AYT);
const string nop(IAC NOP);
const string reset_on (IAC SB "\x2C\x32\x26" IAC SE);
const string reset_off(IAC SB "\x2C\x32\x16" IAC SE);
const string power_on (IAC SB "\x2C\x32\x25" IAC SE);
const string power_off(IAC SB "\x2C\x32\x15" IAC SE);

const string reset_on_confirmation (IAC SB "\x2C\x97\xBF" IAC SE);
const string reset_off_confirmation(IAC SB "\x2C\x97\xFF" IAC SE);
const string power_on_confirmation (IAC SB "\x2C\x97\xDF" IAC SE);
const string power_off_confirmation(IAC SB "\x2C\x97\xFF" IAC SE);

enum {
  FD_LISTEN_LP,
  FD_LISTEN_HP,
  FD_IP_RELAY,
  FD_CLIENT,
  FD_COUNT = FD_CLIENT + MAX_CLIENTS
};

#define CHECK(cmd) ({ int ret = (cmd); if (ret == -1) { printf("error on line %d: " #cmd ": %s\n", __LINE__, strerror(errno)); exit(1); }; ret; })

int msg(const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));
int msg(const char* fmt, ...)
{
  va_list ap;
  int ret;
  char *str;
  va_start(ap, fmt);
  ret = vasprintf(&str, fmt, ap);
  va_end(ap);
  unsigned l = strlen(str);
  if (l > 0 && str[l-1] == '\n')
    str[l-1] = 0;
  printf("%s\n", str);
  fflush(stdout);
  free(str);
  return ret;
}

class CommandStr : public string {
  bool _can_match_later;
  bool _matched;
public:
  CommandStr() : string(), _can_match_later(false), _matched(false) {}
  CommandStr(char *str, size_t len) : string(str, len), _can_match_later(false), _matched(false) {}

  void reset_match() {
    _can_match_later = false;
    _matched = false;
  }

  bool match(const string &str)
  {
    if (*this == str) {
      _matched = true;
      return true;
    }

    if (*this == str.substr(0, length()))
      _can_match_later = true;

    return false;
  }

  int dio_confirmation()
  {
    size_t prefix = find(IAC SB "\x2C\x97");
    if (prefix == string::npos)
      return -1;
    if (find(IAC SE, prefix + 5) != prefix + 5)
      return -1;
    return at(prefix + 4) & 0xff;
  }


  bool can_match_later() const { return _can_match_later; }
  bool matched() const { return _matched; }

  string as_hex()
  {
    string hex;
    char b[3];

    for (size_t i=0; i<length(); i++) {
      snprintf(b, sizeof(b), "%02hhx", at(i));
      hex.append(b);
    }
    return hex;
  }

  void remove_nop() {
    size_t where;
    while ((where = find(nop)) != string::npos)
      erase(where, nop.length());
  }
};



class IpRelay {
  int              fd;
  struct addrinfo *ai;
  const char      *node;
  const char      *service;
  enum state {
    OFF, RST1, RST2, PWRON1, PWRON2, DATA, PWROFF1, PWROFF2
  } state;
  pollfd          &pfd;
  string           to_relay;
  struct timeval   poweroff_countdown;

  IpRelay(const IpRelay&);
  IpRelay& operator=(const IpRelay&);
public:
  IpRelay(const char *node, const char *service, pollfd &pfd) : fd(-1), ai(0), node(node), service(service), state(DATA), pfd(pfd), to_relay(), poweroff_countdown()
  {
    struct addrinfo hints;
    int ret;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_protocol = 0;          /* Any protocol */
    ret = getaddrinfo(node, service, &hints, &ai);
    if (ret != 0) {
      msg("getaddrinfo(%s, %s): %s\n", node, service, gai_strerror(ret));
      exit(1);
    }

    start_poweroff_countdown();
  }

  ~IpRelay()
  {
    freeaddrinfo(ai);
  }

  int connect()
  {
    struct addrinfo *aip;

    for (aip = ai; aip != NULL; aip = aip->ai_next) {
      fd = socket(aip->ai_family, aip->ai_socktype,
                  aip->ai_protocol);
      if (fd == -1)
        continue;

      if (::connect(fd, aip->ai_addr, aip->ai_addrlen) != -1) {
        CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
        break; /* Success */
      }

      close(fd);
    }

    if (aip == NULL) {          /* No address succeeded */
      static bool first = true;
      if (first)
        msg("Could not connect to IP relay at %s:%s - will try later\n", node, service);
      first = false;
      fd = -1;
    } else
      msg("Connected to IP relay at %s:%s\n", node, service);
    return fd;
  }

  bool connected() const { return fd != -1; }

  void disconnect()
  {
    close(fd);
    fd = -1;
  }

  void send(const string &str)
  {
    if (!connected())
      msg("attempt to write to disconnected relay");

    int len = str.length();
    int ret = CHECK(write(fd, str.data(), len));
    if (ret < len) {
      to_relay.append(str.c_str() + ret);
      pfd.events |= POLLOUT;
    }
  }


  void reset()
  {
    msg("Starting reset/power-on sequence");
    send(reset_on);
    state = RST1;
  }

  void poweroff()
  {
    msg("Starting power-off sequence");
    send(power_on);
    state = PWROFF1;
    poweroff_countdown = {0,0};
  }

  int handle(pollfd &pfd, char *buf, size_t size)
  {
    if (pfd.revents & POLLRDHUP) {
      msg("IP relay disconnected\n");
      disconnect();
      memset(&pfd, 0, sizeof(pfd));
      return 0;
    }

    if (pfd.revents & POLLIN) {
      int ret = ::read(fd, buf, size);
      if (ret == -1)
        return ret;

      CommandStr reply(buf, ret);
      reply.remove_nop();

      //msg("reply: %s\n", reply.as_hex().c_str());
      switch (state) {
      case OFF:
        if (reply.length() > 0)
          msg("data in OFF state");
      case DATA:
        strncpy(buf, reply.c_str(), size); // Copy back without NOPs
        return reply.length();

      case RST1:
        if (~reply.dio_confirmation() & 0x40) {
	  usleep(100000);
	  send(reset_off);
          state = RST2;
        }
        break;
      case RST2:
        if (reply.dio_confirmation() & 0x40) {
          send(power_on);
          state = PWRON1;
        }
        break;
      case PWRON1:
        if (~reply.dio_confirmation() & 0x20) {
          usleep(100000);
          send(power_off);
          state = PWRON2;
        }
        break;
      case PWRON2:
	if (reply.dio_confirmation() == 0xff)
          state = DATA;
        break;
      case PWROFF1:
        if (~reply.dio_confirmation() & 0x20) {
          sleep(6);
          send(power_off);
          state = PWROFF2;
        }
        break;
      case PWROFF2:
	if (reply.dio_confirmation() & 0x20)
          state = OFF;
	break;
      }
      return 0;
    }

    if (pfd.revents & POLLOUT) {
      int len = to_relay.length();
      int ret = CHECK(write(fd, to_relay.data(), len));

      if (ret == len) {
        to_relay.clear();
        pfd.events &= ~POLLOUT;
      } else {
        to_relay.erase(0, ret);
      }
      return 0;
    }
    msg("ip_relay unhandled revent %#x\n", pfd.revents);
    return -1;
  }

  void start_poweroff_countdown()
  {
    gettimeofday(&poweroff_countdown, 0);
    msg("starting power off countdown");
  }

  int secs_until_poweroff()
  {
    struct timeval tv;
    gettimeofday(&tv, 0);
    if (poweroff_countdown.tv_sec == 0)
      return -1;
    else
      return max(0, 10*60 - static_cast<int>(tv.tv_sec - poweroff_countdown.tv_sec));
  }
};

class Client {
  Client             *next;
  int                 fd;
  pollfd             *pfd;
  struct sockaddr_in  addr;
  char                buf[4096];
  string              to_client;
  CommandStr          command;
  struct timeval      last_activity;

  Client(const Client&);        // disable copy constructor
  Client&         operator = (const Client&);
public:
  static Client  *active;
  static IpRelay *ip_relay;
  bool            high_prio;

  Client(pollfd &pfd, struct sockaddr_in &addr, bool hp = false) :
    next(0), fd(pfd.fd), pfd(&pfd), addr(addr), to_client(), command(), last_activity(), high_prio(hp)
  {
    msg("client connected%s", hp ? " (high prio)" : "");
    CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
    touch();
  }

  Client(int fd, struct sockaddr_in &addr, bool hp = false) :
    next(0), fd(fd), pfd(0), addr(addr), to_client(), command(), last_activity(), high_prio(hp)
  {
    msg("client connected%s", hp ? " (high prio)" : "");
    CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
    touch();
  }

  void touch()
  {
    gettimeofday(&last_activity, 0);
  }

  unsigned idle_secs()
  {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec - last_activity.tv_sec;
  }

  static const int timeout_secs = 20*60;

  unsigned secs_until_timeout()
  {
    return max(0, timeout_secs - static_cast<int>(idle_secs()));
  }

  string name()
  {
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), sizeof(addr), hbuf, sizeof(hbuf), sbuf,
                    sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
      return string(hbuf)+string(":")+string(sbuf);
    else
      return NULL;
  }

  pollfd *get_pfd() const { return pfd; }

  int msg(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)))
  {
    va_list ap;
    int ret;
    char *str;
    va_start(ap, fmt);
    ret = vasprintf(&str, fmt, ap);
    va_end(ap);
    unsigned l = strlen(str);
    if (l > 0 && str[l-1] == '\n')
      str[l-1] = 0;
    ::msg("%s: %s\n", name().c_str(), str);
    free(str);
    return ret;
  }

  void add_to_queue(bool as_first = false)
  {
    if (!active) {
      active = this;
      return;
    }

    if (as_first) {
      next = active;
      active = this;
      return;
    }

    Client *c;
    if (!high_prio) {
      // Add ourselves to the end of the queue
      for (c = active; c->next; c = c->next);
      c->next = this;
    } else {
      // Add ourselves to the end of high prio clients in the queue
      Client *last_hp = 0;
      for (c = active; c && c->high_prio; last_hp = c, c = c->next);
      next = last_hp->next;
      last_hp->next = this;
    }
  }

  void print_queue()
  {
    Client *c;
    printf("queue:\n");
    for (c = active; c; c = c->next)
      printf("  %s next=%p\n", c->name().c_str(), c->next);
    printf("end\n");
  }

  void del_from_queue()
  {
    if (active == this)
      active = next;
    else {
      Client *c;
      for (c = active; c->next != this; c = c->next);
      c->next = next;
    }

    if (active)
      active->handle();
  }

  void bye_bye(const string &message)
  {
    char *bye_msg;

    int res = asprintf(&bye_msg, "%s. Closing connection.\n", message.c_str());
    if (res < 0) goto done;
    msg("%s", bye_msg);
    send(bye_msg);
    free(bye_msg);
   done:
    close(fd);
  }

  bool is_active() const { return active == this; }

  unsigned clients_before(string &who)
  {
    unsigned i;
    Client *c = active;
    for (i = 0; c != this; i++, c = c->next) {
      if (!who.empty())
        who += ", ";
      who += c->name();
      if  (c->idle_secs() > 15) {
        char t[30];
        if (c->idle_secs() > 60)
          snprintf(t, sizeof(t), " (idle %u/%d min)", c->idle_secs()/60, timeout_secs/60);
        else
          snprintf(t, sizeof(t), " (idle %u s/%d min)", c->idle_secs(), timeout_secs/60);
        who += t;
      }
    }
    return i;
  }

  bool interpret_as_command(char ch, string &to_relay)
  {
    if (ch != IAC[0] && command.empty())
      return false;

    command += ch;
    command.reset_match();
    //msg("command: %s", command.as_hex().c_str());

    if (command.match(are_you_there)) {
      char *buf;
      msg("received AYT");
      if (is_active())
        asprintf(&buf, "<iprelayd: connected>\n");
      else {
        string names;
        unsigned q = clients_before(names);
        asprintf(&buf, "<iprelayd: not connected - %d client%s before you: %s>\n", q, q > 1 ? "s":"", names.c_str());
      }
      send(buf, strlen(buf));
      free(buf);
    } else if (command.match(reset_on)) {
      msg("received reset on");
      ip_relay->reset();
      send(reset_on_confirmation);
    } else if (command.match(reset_off)) {
      msg("received reset off");
      send(reset_off_confirmation);
    } else if (command.match(power_on)) {
      msg("received power on");
      send(power_on_confirmation);
    } else if (command.match(power_off)) {
      msg("received power off");
      send(power_off_confirmation);
    } else if (!command.can_match_later()) {
      msg("unknown command: %s", command.as_hex().c_str());
      to_relay += command;
      command.clear();
    }
    if (command.matched())
      command.clear();
    return true;
  }

  bool handle()
  {
    int ret;
    string to_relay;

    if (pfd->revents & POLLRDHUP) {
      msg("disconnected");
      return false;
    }

    if (secs_until_timeout() == 0) {
      bye_bye("Timeout (iprelayd)");
      return false;
    }

    if (pfd->revents & POLLIN) {
      touch();
      ret = read(fd, buf, sizeof(buf));

      if (ret < 0 && errno != EAGAIN) {
        msg("read: %s", strerror(errno));
        return false;
      }

      for (int i = 0; i < ret; i++) {
        if (!interpret_as_command(buf[i], to_relay)) {
          to_relay += buf[i];
          msg("received non-command len=%d\n", ret);
        }
      }

      if (!to_relay.empty()) {
        if (active == this) {
          ip_relay->send(to_relay);
          to_relay.clear();
        } else {
          bye_bye("IP relay is used by somebody else");
          return false;
        }
      }
    }

    if (pfd->revents & POLLOUT) {
      int len = to_client.length();
      ret = CHECK(write(fd, to_client.data(), len));

      if (ret == len) {
        to_client.clear();
        pfd->events &= ~POLLOUT;
      } else {
        to_client.erase(0, ret);
      }
      touch();
    }

    return true;
  }

  void send(const char *str, size_t len)
  {
    int ret = CHECK(write(fd, str, len));
    if (ret >= 0 && ret < static_cast<int>(len)) {
      to_client.append(&str[ret], len - ret);
      pfd->events |= POLLOUT;
    }
    touch();
  }

  void send(const string &str)
  {
    send(str.data(), str.length());
  }
};

Client  *Client::active;
IpRelay *Client::ip_relay;

int main(int argc, char *argv[])
{
  struct sockaddr_in    my_addr;
  int                   yes = 1;
  static struct pollfd  pollfds[FD_COUNT];
  static Client        *clients[FD_COUNT];

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <IP_address> [<port>]\n", argv[0]);
    exit(1);
  }

  msg("Starting iprelayd");

  IpRelay ip_relay(argv[1], argc > 2 ? argv[2] : "23", pollfds[FD_IP_RELAY]);
  Client::ip_relay = &ip_relay;

  int sfd;

  sfd = CHECK(socket(PF_INET, SOCK_STREAM, 0));
  CHECK(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(2323);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  CHECK(bind(sfd, reinterpret_cast<const sockaddr*>(&my_addr), sizeof(my_addr)));
  CHECK(listen(sfd, 10));
  pollfds[FD_LISTEN_LP] = { sfd, POLLIN };

  sfd = CHECK(socket(PF_INET, SOCK_STREAM, 0));
  CHECK(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));
  my_addr.sin_port = htons(2324);
  CHECK(bind(sfd, reinterpret_cast<const sockaddr*>(&my_addr), sizeof(my_addr)));
  CHECK(listen(sfd, 10));
  pollfds[FD_LISTEN_HP] = { sfd, POLLIN };

  while (1) {
    if (!ip_relay.connected()) {
      int fd = ip_relay.connect();
      if (ip_relay.connected())
        pollfds[FD_IP_RELAY] = { fd, POLLIN | POLLRDHUP };
    }

    int timeout;
    if (!ip_relay.connected())
      timeout = 1000;
    else if (Client::active)
      timeout = 1000 * Client::active->secs_until_timeout() + 100;
    else {
      int secs = ip_relay.secs_until_poweroff();
      timeout = (secs >= 0) ? 1000 * secs + 100 : -1;
    }

    CHECK(poll(pollfds, FD_COUNT, timeout));

    if (!Client::active && ip_relay.secs_until_poweroff() == 0)
      ip_relay.poweroff();

    if (pollfds[FD_LISTEN_LP].revents) {
      socklen_t          sin_size;
      struct sockaddr_in their_addr;
      unsigned           i;

      sin_size   = sizeof(their_addr);
      int new_fd = CHECK(accept(pollfds[FD_LISTEN_LP].fd, (struct sockaddr *)&their_addr, &sin_size));
      for (i = FD_CLIENT; i < FD_COUNT && pollfds[i].events; i++);
      if (i < FD_COUNT) {
        pollfds[i] = { new_fd, POLLIN | POLLRDHUP };
        Client *c = new Client(pollfds[i], their_addr);
        c->add_to_queue();
        clients[i] = c;
      } else {
        Client c(new_fd, their_addr);
        c.bye_bye("Too many connections");
      }
    }

    if (pollfds[FD_LISTEN_HP].revents) {
      socklen_t           sin_size;
      struct sockaddr_in  their_addr;
      unsigned            i;
      bool                add_as_first = false;

      sin_size   = sizeof(their_addr);
      int new_fd = CHECK(accept(pollfds[FD_LISTEN_HP].fd, (struct sockaddr *)&their_addr, &sin_size));
      if (Client::active && !Client::active->high_prio) {
        Client *c = Client::active;
        c->bye_bye("More privileged user connected");
        c->del_from_queue();
        i = c->get_pfd() - pollfds;
        delete c;
        add_as_first = true;
      } else {
        for (i = FD_CLIENT; i < FD_COUNT && pollfds[i].events; i++);
      }

      if (i < FD_COUNT) {
        pollfds[i] = { new_fd, POLLIN | POLLRDHUP };
        Client *c = new Client(pollfds[i], their_addr, true);
        c->add_to_queue(add_as_first);
        clients[i] = c;
      } else {
        Client c(new_fd, their_addr);
        c.bye_bye("Too many connections");
      }
    }

    if (pollfds[FD_IP_RELAY].revents) {
      char buf[2000];
      int ret = ip_relay.handle(pollfds[FD_IP_RELAY], buf, sizeof(buf));

      if (ret == -1) {
        msg("iprelay error: %s", strerror(errno));
        exit(1);
      }

      if (ret > 0) {
        if (Client::active)
          Client::active->send(buf, ret);
        int res = write(1, buf, ret);     // Copy to stdout
        (void)res;
      }
    }

    for (unsigned i = FD_CLIENT; i < FD_COUNT; i++) {
      Client *c = clients[i];

      if (c && !c->handle()) {
        // client disconnected
        c->del_from_queue();
        delete c;
        close(pollfds[i].fd);
        memset(&pollfds[i], 0, sizeof(pollfds[i]));
        clients[i] = 0;
        if (!Client::active)
          ip_relay.start_poweroff_countdown();
      }
    }
  }
}
