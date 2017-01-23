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

char listenIP[16] = "";
char friendIP[16] = "";
int listenport;
int friendport;

void handle_err(int lastcode) {
  perror(strerror(errno));
  exit(lastcode);
}

/* Set up a listener socket and accept connections, return connectedfd */
int setup_tcplistener(void) {

  struct sockaddr_in host_addr;
  struct sockaddr_in clientaddr;
  socklen_t addrsize;
  
  int hostfd = socket(AF_INET, SOCK_STREAM, 0);

  /* Socket creation failure */
  if(hostfd == -1)
    handle_err(1);

  memset(&host_addr, 0, sizeof(struct sockaddr_in));

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(listenport);
  inet_pton(AF_INET, listenIP, &host_addr.sin_addr);

  /* Bind failure */
  if(bind(hostfd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in)) == -1)
    handle_err(2);

  if(listen(hostfd, 1) == -1)
    handle_err(3);

  printf("Listening on %s:%d\n\n", listenIP, listenport);

  int clientfd = accept(hostfd, (struct sockaddr *)&clientaddr, (socklen_t *)&addrsize);

  if(clientfd == -1)
    handle_err(4);
  
  return clientfd;
}

/* Returns an fd > 0 which can be used to send and receive messages to/from a friend */
int setup_tcpfriend() {

  int friendfd = socket(AF_INET, SOCK_STREAM, 0);

  if(friendfd == -1)
    handle_err(5);

  sockaddr_in friendaddr;
  memset(&friendaddr, 0, sizeof(struct sockaddr_in));
  friendaddr.sin_family = AF_INET;
  friendaddr.sin_port = htons(friendport);
  inet_pton(AF_INET, friendIP, &friendaddr.sin_addr);

  printf("Connecting to %s:%d\n", friendIP, friendport);
  
  if(connect(friendfd, (const struct sockaddr *)&friendaddr, (socklen_t)sizeof(struct sockaddr_in)) == -1)
    handle_err(6);

  printf("Connected to %s:%d\n", friendIP, friendport);

  return friendfd;
}

int main(int argc, char *argv[]) {
  bool should_listen_for_connections = false;
  bool should_connect_to_friend = false;
  int connectedfd=-1;
  char USAGE[200];
  char buffer[1025];
  fd_set readfds;
  fd_set writefds;
  int retval, stdinfd, nfds;;
  char *msg_for_friend = NULL;
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
    connectedfd = setup_tcplistener();
  }
  else if(should_connect_to_friend) {
    connectedfd = setup_tcpfriend();
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

        printf("Got some on stdin\n", selection);

        if(bytes_read < 0)
          return 8;

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        if(bytes_read > 1) {
          /* Mask the newline character we get in from stdin */
          buffer[bytes_read - 1] = '\0';

          printf("Got stdin: %d bytes and %d length\n", bytes_read, (int)strlen(buffer));

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

  /* TODO: Close FDs */
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  close(connectedfd);

  return 0;
}
