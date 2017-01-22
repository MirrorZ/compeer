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

#include "common.h"

#define HOSTPORT 26347
#define HOST "127.0.0.1"
#define FRIENDPORT 26348
#define FRIEND "127.0.0.1"

/* Set up a listener socket and returns an fd > 0 which can be used
   to accept connections */
int setup_tcplistener(void) {
  struct sockaddr_in host_addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);
  int hostfd = socket(AF_INET, SOCK_STREAM, 0);

  /* Socket creation failure */
  if(hostfd == -1)
    return -1;
     
  memset(&host_addr, 0, addr_size);

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(HOSTPORT);
  inet_pton(AF_INET, HOST, &host_addr.sin_addr);
  
  if(bind(hostfd, (struct sockaddr *)&host_addr, addr_size) == -1)
    return -1;

  if(listen(hostfd, 1) == -1)
    return -1;

  return hostfd;
}

/* Returns an fd > 0 which can be used to send and receive messages to/from a friend */
int setup_tcpfriend() {
  int friendfd = socket(AF_INET, SOCK_STREAM, 0);

  if(friendfd == -1)
    return -1;

  sockaddr_in friendaddr;
  memset(&friendaddr, 0, sizeof(struct sockaddr_in));
  friendaddr.sin_family = AF_INET;
  friendaddr.sin_port = htons(FRIENDPORT);
  inet_pton(AF_INET, FRIEND, &friendaddr.sin_addr);
  
  if(connect(friendfd, (const struct sockaddr *)&friendaddr, sizeof(struct sockaddr_in)) == -1)
    return -1;

  return friendfd;
}

int main(int argc, char *argv[]) {
  int listenerfd=-1;
  bool should_listen_for_connections = false;
  bool should_connect_to_friend = false;
  int connectedfd=-1;
  
  /* Nothing to do */
  if(!should_listen_for_connections && !should_connect_to_friend)
    return -1;
  if(should_listen_for_connections && should_connect_to_friend)
    return -1;

  if(should_listen_for_connections) {
    listenerfd = setup_tcplistener();
    if(listenerfd == -1)
      return -2;
  }
  else if(should_connect_to_friend) {
    connectedfd = setup_tcpfriend();
    if(connectedfd == -1)
      return -3;
  }

  char buffer[1025];
  fd_set readfds;
  fd_set writefds;
  int stdinfd = 0;
  char *msg_for_friend = NULL;
  bool noonetotalkto = true;
  int nfds = 0;
  
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(stdinfd, &readfds);
    
    if(connectedfd == -1) {
      FD_SET(listenerfd, &readfds);
      nfds = listenerfd + 1;
    }
    else {
      FD_SET(connectedfd, &readfds);
      FD_SET(connectedfd, &writefds);
      nfds = connectedfd + 1;
    }

    /* Wait for data to arrive on the socket or stdin or a new connection */
    int selection = select(nfds, &readfds, &writefds, NULL, NULL);

    if(selection == -1)
      return -1;
    else if(selection) {

      /* Got a message over stdin */
      if(FD_ISSET(stdinfd, &readfds)) {

        if(read(0, buffer, 1024) < 0)
          return -2;

        int oldmsgfsize = strlen(msg_for_friend);
        int newmsgfsize = oldmsgfsize + strlen(buffer) + 1;
        msg_for_friend = (char *) realloc(msg_for_friend, newmsgfsize);
        strcpy(msg_for_friend + oldmsgfsize, buffer);
        msg_for_friend[newmsgfsize - 1] = '\0';

        if(connectedfd == -1)
          printf("[INFO] Message queued for later. No one to talk to.\n");
      }

      /* Some things should only happen when we are disconnected */
      if(connectedfd == -1) {
        /* Accept a new connection request */
        if(FD_ISSET(listenerfd, &readfds)) {
          struct sockaddr_in clientaddr;
          socklen_t addrsize;
          int clientfd = accept(listenerfd, (struct sockaddr *)&clientaddr, (socklen_t *)&addrsize);

          if(clientfd < 0)
            return -1;

          connectedfd = clientfd;
        }
      }
      /* Some things should only happen when we are connected */
      else {
        /* Got a message over TCP */
        if(FD_ISSET(connectedfd, &readfds)) {

          int retval = read(connectedfd, buffer, sizeof(buffer));

          if(retval == -1)
            return -1;
          else if(retval == 0) {
            /* Bye Bye friend */
            connectedfd = -1;
            free(msg_for_friend);
            msg_for_friend = NULL;
          }
          else
            printf(PROMPT "%s \n", buffer);
        }

        /* Send a message over TCP */
        if(FD_ISSET(connectedfd, &writefds)) {

          if(msg_for_friend != NULL) {
            int retval = send(connectedfd, msg_for_friend, strlen(msg_for_friend), 0);

            if(retval == -1)
              return -1;
            else if(retval == strlen(msg_for_friend)) {
              free(msg_for_friend);
              msg_for_friend = NULL;
            }
            else
              msg_for_friend+=retval;
          }
        }
      }
    }
  }

  /* TODO: Close FDs */

  return 0;
}
