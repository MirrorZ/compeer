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

void handle_err(int err_no) {
  perror(strerror(errno));
  exit(-1);
}

/* Set up a listener socket and accept connections, return connectedfd */
int setup_tcplistener(void) {

  struct sockaddr_in host_addr;
  struct sockaddr_in clientaddr;
  socklen_t addrsize;
  
  int hostfd = socket(AF_INET, SOCK_STREAM, 0);

  /* Socket creation failure */
  if(hostfd == -1)
    handle_err(errno);

  memset(&host_addr, 0, sizeof(struct sockaddr_in));

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(listenport);
  inet_pton(AF_INET, listenIP, &host_addr.sin_addr);

  /* Bind failure */
  if(bind(hostfd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in)) == -1)
    handle_err(errno);

  if(listen(hostfd, 1) == -1)
    handle_err(errno);

  printf("Listening on %s:%d\n\n", listenIP, listenport);

  int clientfd = accept(hostfd, (struct sockaddr *)&clientaddr, (socklen_t *)&addrsize);

  if(clientfd == -1)
    handle_err(errno);
  
  return clientfd;
}

/* Returns an fd > 0 which can be used to send and receive messages to/from a friend */
int setup_tcpfriend() {

  int friendfd = socket(AF_INET, SOCK_STREAM, 0);

  if(friendfd == -1)
    handle_err(errno);

  sockaddr_in friendaddr;
  memset(&friendaddr, 0, sizeof(struct sockaddr_in));
  friendaddr.sin_family = AF_INET;
  friendaddr.sin_port = htons(friendport);
  inet_pton(AF_INET, friendIP, &friendaddr.sin_addr);

  printf("Connecting to %s:%d\n", friendIP, friendport);
  
  if(connect(friendfd, (const struct sockaddr *)&friendaddr, (socklen_t)sizeof(struct sockaddr_in)) == -1)
    handle_err(errno);

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

  if(connectedfd < 0)
    return connectedfd*-1;

  while(1){
    ;
  }
  return 0;
}
