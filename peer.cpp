#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//#include "common.h"

void handle_err(int lastcode) {
  perror(strerror(errno));
  exit(lastcode);
}

/* Packing the necessary data structures required for the peer */
class peeraddr {

  public:

  struct sockaddr_in addr;
  socklen_t addr_size;

  char ipv4[16];
  int port;

  peeraddr(void) {
    addr_size = sizeof(struct sockaddr_in);
  }

  /* Stick to TCP right now
     TODO: overload this to create UDP sockets
  */
  peeraddr(char ipaddr[], int portnum);

  /* TODO: add get_ip,port for peers connected */

};

peeraddr::peeraddr(char ipaddr[], int portnum) {
    strcpy(this->ipv4, ipaddr);
    this->port = portnum;
    addr_size = sizeof(struct sockaddr_in);

    if(memset(&(this->addr), 0, (size_t)addr_size) == NULL)
      handle_err(errno);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipv4, &addr.sin_addr);
}

class peer {
public:
  peeraddr listen_addr;
  int listenfd;
  peeraddr friend_addr;
  int friendfd;

  /* Set up a listener socket and accept connections, return connectedfd */
  int tcp_listener(peeraddr laddr);

  /* Returns an fd > 0 which can be used to send and receive messages to/from a friend */
  int tcp_friend(peeraddr faddr);
};


int peer::tcp_listener(peeraddr laddr) {

  this->listen_addr = laddr;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* Socket creation failure */
  if(sockfd == -1)
    handle_err(1);

  this->listenfd = sockfd;

  /* Bind failure */
  if(bind(this->listenfd, (struct sockaddr *)&this->listen_addr.addr, this->listen_addr.addr_size) == -1)
    handle_err(2);

  /* Listen for peer */
  if(listen(this->listenfd, 1) == -1)
    handle_err(3);

  printf("Listening on %s:%d\n\n", this->listen_addr.ipv4, this->listen_addr.port);

  int clientfd = accept(this->listenfd, (struct sockaddr *)&this->friend_addr.addr, (socklen_t *)&this->friend_addr.addr_size);

  if(clientfd == -1)
    handle_err(4);

  this->friendfd = clientfd;

  return this->friendfd;
}

int peer::tcp_friend(peeraddr faddr) {

  this->friend_addr = faddr;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1)
    handle_err(5);

  this->friendfd = sockfd;

  printf("Connecting to %s:%d\n", faddr.ipv4, faddr.port);


  if(connect(this->friendfd, (const struct sockaddr *)&faddr.addr, faddr.addr_size) == -1)
    handle_err(6);

  printf("Connected to %s:%d\n", faddr.ipv4, faddr.port);

  return this->friendfd;
}

int main(int argc, char *argv[]) {

  bool should_listen_for_connections = false;
  bool should_connect_to_friend = false;

  char USAGE[200];
  char buffer[1025];

  fd_set readfds;
  fd_set writefds;

  int retval, stdinfd, nfds;

  char listenIP[16], friendIP[16];
  int listenport, friendport, connectedfd;

  char *msg_for_friend = NULL;

  peer listener, remote_friend;
  //bool noonetotalkto = true;
  stdinfd = nfds = 0;

  sprintf(USAGE, "Usage: \n\t%s -listen listenIP listenPORT\n\tOR\n\t%s -friend friendIP friendPort\n", argv[0], argv[0]);

  /* Handle command line arguments for flexibility */
  if(argc!=4) {
    printf("%s", USAGE);
    return 1;
  }

  if(!strcmp(argv[1], "-listen")) {
    should_listen_for_connections = true;
    strcpy(listenIP, argv[2]);
    listenport = atoi(argv[3]);
  }
  else if(!strcmp(argv[1], "-friend")) {
    should_connect_to_friend = true;
    strcpy(friendIP, argv[2]);
    friendport = atoi(argv[3]);
  }
  else {
    printf("%s", USAGE);
    return 2;
  }

  /* Nothing to do */
  if(!should_listen_for_connections && !should_connect_to_friend)
    return 3;
  if(should_listen_for_connections && should_connect_to_friend)
    return 4;

  if(should_listen_for_connections) {
    peeraddr listen_addr(listenIP, listenport);
    connectedfd = listener.tcp_listener(listen_addr);
  }
  else if(should_connect_to_friend) {
    peeraddr friend_addr(friendIP, friendport);
    connectedfd = remote_friend.tcp_friend(friend_addr);
  }

  printf("Connection.\n");

  if(connectedfd < 0)
    return connectedfd*-1;

  /*
  /* Main conversation */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(stdinfd, &readfds);
    FD_SET(connectedfd, &readfds);
    FD_SET(connectedfd, &writefds);
    nfds = connectedfd + 1;

    /* Wait for data to arrive on the socket or stdin or a new connection */
    int selection = select(nfds, &readfds, &writefds, NULL, NULL);

    if(selection == -1)
      handle_err(7);
    else if(selection) {

      /* Got a message over stdin */
      if(FD_ISSET(stdinfd, &readfds)) {
        int bytes_read = read(0, buffer, 1024);

        //        printf("Got some on stdin\n", selection);

        if(bytes_read < 0)
          return 8;

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        if(bytes_read > 1) {
          /* Mask the newline character we get in from stdin */
          buffer[bytes_read - 1] = '\0';

          //          printf("Got stdin: %d bytes and %d length\n", bytes_read, (int)strlen(buffer));

          int msgfsize = strlen(buffer) + 1;
          msg_for_friend = (char *) realloc(msg_for_friend, msgfsize);
          strcpy(msg_for_friend, buffer);
          msg_for_friend[msgfsize - 1] = '\0';
          printf("msg: %s\n", msg_for_friend);
        }
      }

      /* Send a message over TCP */
      if(FD_ISSET(connectedfd, &writefds)) {
        if(msg_for_friend != NULL) {
          int retval = send(connectedfd, msg_for_friend, strlen(msg_for_friend), 0);

          if(retval == -1)
            return 11;
          else if(retval == strlen(msg_for_friend)) {
            free(msg_for_friend);
            msg_for_friend = NULL;
          }
          else
            msg_for_friend+=retval;
        }
      }

      /* Got a message over TCP */
      if(FD_ISSET(connectedfd, &readfds)) {
        int retval = read(connectedfd, buffer, sizeof(buffer));

        if(retval == -1)
          return 10;

        buffer[retval] = '\0';
        printf("> %s \n", buffer);
      }
    }
  }

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  close(connectedfd);
  if (should_listen_for_connections)
    close(listener.listenfd);
  return 0;
}
