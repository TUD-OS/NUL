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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define MAX_CLIENTS 3

// NVT codes (http://www.ietf.org/rfc/rfc854.txt)
#define  SE   "\xF0"
#define  AYT  "\xF6"
#define  SB   "\xFA"
#define  WILL "\xFB"
#define  WONT "\xFC"
#define  DO   "\xFD"
#define  DONT "\xFE"
#define  IAC  "\xFF"

const string are_you_there(IAC AYT);
// const string reset_on (IAC SB "\x2C\x32\x25" IAC SE);
// const string reset_off(IAC SB "\x2C\x32\x15" IAC SE);
// const string power_on (IAC SB "\x2C\x32\x26" IAC SE);
// const string power_off(IAC SB "\x2C\x32\x26" IAC SE);

enum {
  FD_LISTEN_LP,
  FD_LISTEN_HP,
  FD_IP_RELAY,
  FD_CLIENT,
  FD_COUNT = FD_CLIENT + MAX_CLIENTS
};

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
  char time_str[30];
  time_t t = time(NULL);
  strftime(time_str, sizeof(time_str), "%F %T", localtime(&t));
  fprintf(stderr, "%s %s\n", time_str, str);
  free(str);
  return ret;
}


class Client {

  class CommandStr : public string {
    bool  _can_match_later;
  public:
    CommandStr() : string(), _can_match_later(false) {}

    void reset_match() {
      _can_match_later = false;
    }

    bool match(const string &str)
    {
      if (*this == str) {
        clear();
        return true;
      }

      if (*this == str.substr(0, length()))
        _can_match_later = true;

      return false;
    }

    bool matched() const { return empty(); }
    bool can_match_later() const { return _can_match_later; }
  };

  Client             *next;
  int                 fd;
  struct sockaddr_in  addr;
  char                buf[4096];
  string              to_relay;
  CommandStr          command;

  Client(const Client&);     // disable copy constructor
  Client& operator=(const Client&);
public:
  static Client *active;
  static int     fd_ip_relay;
  bool           high_prio;

  Client(int fd, struct sockaddr_in &addr, bool hp = false) :
    next(0), fd(fd), addr(addr), to_relay(), command(), high_prio(hp) {}

  string name()
  {
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), sizeof(addr), hbuf, sizeof(hbuf), sbuf,
                    sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
      return string(hbuf)+string(":")+string(sbuf);
    else
      return NULL;
  }

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

  Client *add_to_queue()
  {
    Client *kicked = 0;
    if (!active)
      active = this;
    else {
      // Add ourselves to the queue
      Client *c;
      if (!high_prio) {
        for (c = active; c->next; c = c->next);
        c->next = this;
      } else {
        // High prio clients can interrupt low prio ones :)
        if (!active->high_prio) {
          active->bye_bye("More privileged user connected");
          kicked = active;
          next = active->next;
          active = this;
        } else {
          Client *last_hp = 0;
          for (c = active; c && c->high_prio; last_hp = c, c = c->next);
          next = last_hp->next;
          last_hp->next = this;
        }
      }
    }
    return kicked;
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

    asprintf(&bye_msg, "%s. Closing connecion.\n", message.c_str());
    msg("%s", bye_msg);
    send(bye_msg);
    free(bye_msg);
    close(fd);
  }

  bool is_active() const { return active == this; }

  unsigned clients_before() const
  {
    unsigned i;
    Client *c = active;
    for (i = 0; c != this; i++, c = c->next);
    return i;
  }

  bool interpret_as_command(char ch)
  {
    if (ch != IAC[0] && command.empty())
      return false;

    command += ch;
    command.reset_match();

    if (command.match(are_you_there)) {
      char buf[100];
      if (is_active())
        snprintf(buf, sizeof(buf), "<iprelayd: connected>\n");
      else {
        unsigned q = clients_before();
        snprintf(buf, sizeof(buf), "<iprelayd: not connected (%d client%s before you)>\n", q, q > 1 ? "s":"");
      }
      send(buf, strlen(buf));
    }

    if (!command.matched() && !command.can_match_later()) {
      to_relay += command;
      command.clear();
    }
    return true;
  }

  bool handle()
  {
    int ret;
    ret = read(fd, buf, sizeof(buf));

    if (ret < 0) {
      msg("read: %s", strerror(errno));
      return false;
    }

    msg("read len=%d\n", ret);

    for (int i = 0; i < ret; i++) {
      if (!interpret_as_command(buf[i]))
        to_relay += buf[i];
    }

    if (!to_relay.empty()) {
      if (active == this) {
        write(fd_ip_relay, to_relay.data(), to_relay.length());
        to_relay.clear();
      } else {
        bye_bye("IP relay is used by somebody else");
        return false;
      }
    }
    return true;
  }

  int send(const char *str, size_t len)
  {
    return write(fd, str, len);
  }

  int send(const string &str)
  {
    return write(fd, str.data(), str.length());
  }
};

Client *Client::active;
int     Client::fd_ip_relay;

int connect_ip_relay(const char *node, const char *service)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int ret, sfd = -1;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_protocol = 0;          /* Any protocol */
  ret = getaddrinfo(node, service, &hints, &result);
  if (ret != 0) {
    msg("getaddrinfo(%s, %s): %s\n", node, service, gai_strerror(ret));
    exit(1);
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype,
                 rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break;                  /* Success */

    close(sfd);
  }

  freeaddrinfo(result);

  if (rp == NULL) {               /* No address succeeded */
    static bool first = true;
    if (first)
      msg("Could not connect to IP relay at %s:%s - will try later\n", node, service);
    first = false;
    return -1;
  }
  msg("Connected to IP relay at %s:%s\n", node, service);
  return sfd;
}

#define CHECK(cmd) ({ int ret = (cmd); if (ret == -1) { perror(#cmd); exit(1); }; ret; })

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

  Client::fd_ip_relay = -1;    // Connect later

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
    if (Client::fd_ip_relay == -1)
      Client::fd_ip_relay = connect_ip_relay(argv[1], argc > 2 ? argv[2] : "23");

    if (Client::fd_ip_relay != -1)
      pollfds[FD_IP_RELAY] = { Client::fd_ip_relay, POLLIN | POLLRDHUP };

    CHECK(poll(pollfds, FD_COUNT, Client::fd_ip_relay != -1 ? -1 : 1000));

    if (pollfds[FD_LISTEN_LP].revents) {
      socklen_t          sin_size;
      struct sockaddr_in their_addr;
      unsigned           i;

      sin_size   = sizeof(their_addr);
      int new_fd = CHECK(accept(pollfds[FD_LISTEN_LP].fd, (struct sockaddr *)&their_addr, &sin_size));
      Client *c = new Client(new_fd, their_addr);
      c->msg("connected");
      if (Client::fd_ip_relay == -1) {
        c->bye_bye("IP relay is offline");
        delete c;
      } else {
        for (i = FD_CLIENT; i < FD_COUNT && pollfds[i].events; i++);
        if (i < FD_COUNT) {
          pollfds[i] = { new_fd, POLLIN | POLLRDHUP };
          clients[i] = c;
          c->add_to_queue();
        } else {
          c->bye_bye("Too many connections");
          delete c;
        }
      }
    }

    if (pollfds[FD_LISTEN_HP].revents) {
      socklen_t           sin_size;
      struct sockaddr_in  their_addr;
      unsigned            i;

      sin_size   = sizeof(their_addr);
      int new_fd = CHECK(accept(pollfds[FD_LISTEN_HP].fd, (struct sockaddr *)&their_addr, &sin_size));
      Client *c = new Client(new_fd, their_addr, true);
      c->msg("connected (high prio)");
      if (Client::fd_ip_relay == -1) {
        c->bye_bye("IP relay is offline");
        delete c;
      } else {
        Client *kicked = c->add_to_queue();
        if (kicked) {
          for (i = FD_CLIENT; i < FD_COUNT && clients[i] != kicked; i++);
          assert(i < FD_COUNT);
          memset(&pollfds[i], 0, sizeof(pollfds[i]));
          clients[i] = 0;
          delete kicked;
        } else
          for (i = FD_CLIENT; i < FD_COUNT && pollfds[i].events; i++);
        if (i < FD_COUNT) {
          pollfds[i] = { new_fd, POLLIN | POLLRDHUP };
          clients[i] = c;
        } else {
          c->bye_bye("Too many connections");
          c->del_from_queue();
          delete c;
        }
      }
    }

    if (pollfds[FD_IP_RELAY].revents) {
      unsigned revents = pollfds[FD_IP_RELAY].revents;
      msg("ip_relay handle %#x\n", revents);
      if (revents & POLLIN) {
        char buf[2000];
        int ret;
        ret = read(Client::fd_ip_relay, buf, sizeof(buf));

        if (ret == -1) {
          msg("iprelay read: %s", strerror(errno));
          exit(1);
        }

        if (Client::active)
          Client::active->send(buf, ret);
        write(1, buf, ret);     // Copy to stdout
      }

      if (revents & POLLRDHUP) {
        msg("IP relay disconnected\n");
        Client::fd_ip_relay = -1;
        memset(&pollfds[FD_IP_RELAY], 0, sizeof(pollfds[FD_IP_RELAY]));
      }
    }

    for (unsigned i = FD_CLIENT; i < FD_COUNT; i++) {
      if (pollfds[i].revents) {
        Client *c = clients[i];
        c->msg("handle %#x", pollfds[i].revents);
        bool disconnected = false;
        if (pollfds[i].revents & POLLIN)
          disconnected = !c->handle();

        if (disconnected || pollfds[i].revents & POLLRDHUP) {
          c->msg("disconnected");
          c->del_from_queue();
          delete c;
          memset(&pollfds[i], 0, sizeof(pollfds[i]));
          clients[i] = 0;
        }
      }
    }
  }
}
