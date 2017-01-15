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

#define PORT 26347
#define HOST "127.0.0.1"

void handle_err(int err_no) {
  perror(strerror(errno));
  exit(-1);
}

int main() {
  int hostfd, peerfd, selection, res;
  size_t sz;
  struct sockaddr_in host_addr, peer_addr;
  socklen_t addr_size;
  fd_set fdset;
  struct timeval tv;
  char buffer[1025];
  
  addr_size = sizeof(struct sockaddr_in);

  hostfd = socket(AF_INET, SOCK_STREAM, 0);

  /* Socket creation failure */
  if(hostfd == -1)
    handle_err(errno);
     
  memset(&host_addr, 0, addr_size);

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, HOST, &host_addr.sin_addr);
  
  if(bind(hostfd, (struct sockaddr *)&host_addr, addr_size) == -1)
    handle_err(errno);

  if(listen(hostfd, 1) == -1)
    handle_err(errno);

  peerfd = accept(hostfd, (struct sockaddr *)& peer_addr, (socklen_t *)&addr_size);

  if(peerfd == -1)
    handle_err(errno);

  tv.tv_sec = 120;
  tv.tv_usec = 0;

  while(1) {

    FD_ZERO(&fdset);
    FD_SET(0, &fdset);
    FD_SET(peerfd, &fdset);


    /* Wait for data to arrive on the socket or stdin */
    selection = select(peerfd+1, &fdset, NULL, NULL, &tv);

    if(selection == -1)
      handle_err(errno);
    else if(selection) {
      //printf("Data Arrived \n");
      /* Message to send has arrived */
      if(FD_ISSET(0, &fdset)) {
	//	printf("Stdin \n");
	sz = read(0, buffer, 1024);

	if(sz == -1) {
	  handle_err(errno);
	}
	
	send(peerfd, buffer, sz, 0);

      }
      /* Message to the server has arrived */
      if(FD_ISSET(peerfd, &fdset)) {
	//printf("The message has arrived \n");
	sz = read(peerfd, buffer, 1024);

	if(sz == -1) {
	  handle_err(errno);
	}
	
	/* replace this with ip/id */
	printf("friend says %s \n",buffer);

      }
    }
    else
      printf("No data arrived for 120 seconds");
  }
  close(peerfd);
  close(hostfd);
  return 0;
}
