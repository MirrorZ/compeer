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

void handle_err(int err) {
  perror(strerror(err));
}

int main() {
  int sockfd, result, selection, res;
  char buffer[1025];
  struct sockaddr_in addr;
  size_t sz;

  fd_set fdset;
  struct timeval tv;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1)
    handle_err(errno);

  sz = sizeof(struct sockaddr_in);
  
  memset(&addr, 0, sz);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  inet_pton(AF_INET, HOST, &addr.sin_addr);

  result = connect(sockfd, (const struct sockaddr *)&addr, (socklen_t) sz);

  if(result == -1)
    handle_err(errno);

  tv.tv_sec = 120;
  tv.tv_usec = 0;

  while(1) {
   
    FD_ZERO(&fdset);
    FD_SET(0, &fdset);
    FD_SET(sockfd, &fdset);
 
    
    /* Wait for data to arrive on the socket or stdin */
    selection = select(sockfd+1, &fdset, NULL, NULL, &tv);

    if(selection == -1)
      handle_err(errno);
    else if(selection) {
      /* Message to send has arrived */
      if(FD_ISSET(0, &fdset)) {
	//printf("Message to send has arrived\n");
	sz = read(0, buffer, 1024);

	if(sz == -1) {
	  handle_err(errno);
	}
	
	res = send(sockfd, buffer, sz, 0);
	if(res == -1)
	  handle_err(errno);
	//else
	  //printf("Sent %d bytes\n", res);


      }
      /* Message to the client has arrived */
      if(FD_ISSET(sockfd, &fdset)) {
	//printf("Message to client \n");
	sz = read(sockfd, buffer, 1024);

	if(sz == -1) {
	  handle_err(errno);
	}
	
	/* replace this with ip/id */
	printf("friend says %s \n",buffer);

      }
    }
    else
      printf("No data arrived for 120 secnds");
  }
  close(sockfd);
  return 0;
}
