/*
 * Copyright ©2020 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2020 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

#define HNAME_SIZE 1024

extern "C" {
  #include "libhw1/CSE333.h"
}

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd".

  // STEP 1:
  // the following code is inspired from getadrinfo man 3 &
  // the example code from lecture
  struct addrinfo hints;
  struct addrinfo *res, *rp;
  int sfd;

  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = ai_family;      // ollows the given ai_family
  hints.ai_socktype = SOCK_STREAM;  // sequenced, 2 way socket
  hints.ai_flags = AI_PASSIVE;      // for wildcard IP address
  hints.ai_protocol = 0;            // any protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  int s = getaddrinfo(nullptr, std::to_string(port_).c_str(), &hints, &res);
  if (s != 0) {
    std::cerr << "getaddrinfo() failed: "<< gai_strerror(s) << std::endl;
    return false;
  }

  // search through the list of addressed returned by getaddrinfo
  // until successful bind
  for (rp = res; rp != nullptr; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) {
      std::cerr << "socket() failed " << strerror(errno) << std::endl;
      continue;
    }

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // success
      sock_family_ = rp->ai_family;
      break;
    }

    close(sfd);
  }

  // no address succeeded
  if (rp == nullptr) {
    std::cerr << "Could not bind" << std::endl;
    return false;
  }

  // res is no longer neede
  freeaddrinfo(res);


  if (sfd <= 0)
    return false;

  // create listening socket through the sfd
  // listen (2) man
  if (listen(sfd, SOMAXCONN) != 0) {
    std::cerr << "listen() failed " << strerror(errno) << std::endl;
    close(sfd);
    return false;
  }

  // set the listening socket
  listen_sock_fd_ = sfd;
  *listen_fd = sfd;

  return true;
}

bool ServerSocket::Accept(int *accepted_fd,
                          std::string *client_addr,
                          uint16_t *client_port,
                          std::string *client_dnsname,
                          std::string *server_addr,
                          std::string *server_dnsname) {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  struct sockaddr_storage addr_storage;
  struct sockaddr *addr = reinterpret_cast<sockaddr*>(&addr_storage);
  socklen_t addr_len = sizeof(sockaddr_storage);

  int cfd;
  while (true) {
    cfd = accept(listen_sock_fd_, addr, &addr_len);
    if (cfd < 0) {
      // retry when error is EAGAIN or EINTR
      if ((errno == EAGAIN) || (errno == EINTR))
         continue;
      std::cerr << "accept() failed " << strerror(errno) << std::endl;
      return false;
    }
    break;
  }

  // assign the file descriptor returned by accept to accepted_fd
  *accepted_fd = cfd;

  // 2 cases of sock_family_: AF_INET and AF_INET6
  // the following code is inspired from lecture code example
  switch (addr->sa_family) {
  case AF_INET:
    {
      // IPv4
      char ipstring[INET_ADDRSTRLEN];
      struct sockaddr_in *v4addr = reinterpret_cast<struct sockaddr_in*>(addr);
      // convert from binary to test form
      inet_ntop(AF_INET, &(v4addr->sin_addr), ipstring, INET_ADDRSTRLEN);
      *client_addr = std::string(ipstring);
      *client_port = htons(v4addr->sin_port);
      break;
    }
  case AF_INET6:
    {
      // IPv6
      char ipstring[INET6_ADDRSTRLEN];
      struct sockaddr_in6 *v6addr = reinterpret_cast
                             <struct sockaddr_in6*>(addr);
      inet_ntop(AF_INET6, &(v6addr->sin6_addr), ipstring, INET6_ADDRSTRLEN);
      *client_addr = std::string(ipstring);
      *client_port = htons(v6addr->sin6_port);
      break;
    }
  default:
    {
      std::cerr << "address is neither IPv4 nor IPv6" << std::endl;
      close(cfd);
      *accepted_fd = -1;
      return false;
    }
  }

  // DNS name
  char hostname[1024];
  Verify333(getnameinfo(addr, addr_len, hostname, 1024, nullptr, 0, 0)
            == 0);
  *client_dnsname = std::string(hostname);

  // server IP address and DNS name
  char hname[1024];
  hname[0] = '\0';

  // from lecture code
  switch (sock_family_) {
  case AF_INET:
    {
      // IPv4
      struct sockaddr_in srvr;
      socklen_t srvrlen = sizeof(srvr);
      char addrbuf[INET_ADDRSTRLEN];
      getsockname(cfd, (struct sockaddr *) &srvr, &srvrlen);
      inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
      // Get the server's dns name, or return it's IP address as
      // a substitute if the dns lookup fails.
      getnameinfo((const struct sockaddr *) &srvr,
                   srvrlen, hname, 1024, NULL, 0, 0);

      *server_addr = std::string(addrbuf);
      *server_dnsname = std::string(hname);
    }
  default:
    {
      struct sockaddr_in6 srvr;
      socklen_t srvrlen = sizeof(srvr);
      char addrbuf[INET6_ADDRSTRLEN];
      getsockname(cfd, (struct sockaddr *) &srvr, &srvrlen);
      inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
      // Get the server's dns name, or return it's IP address as
      // a substitute if the dns lookup fails.
      getnameinfo((const struct sockaddr *) &srvr,
                   srvrlen, hname, 1024, nullptr, 0, 0);

      *server_dnsname = std::string(hname);
      *server_addr = std::string(addrbuf);
    }
  }
  return true;
}

}  // namespace hw4
