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
#include <signal.h>

#define CONNECT_TO_FRIEND 999
#define WAIT_FOR_FRIEND   998

void handle_err(int lastcode) {
  perror(strerror(errno));
  exit(lastcode);
}

/*
   Stick to TCP for now.
   ITEM-2: Overload this to handle other types of connections.
*/

class peer {
  int mode;
  struct sockaddr_in peersock_addr;
  socklen_t peersock_addr_size;
  int fd_this_side;
  int fd_other_side;

public:

  peer() {
    fd_this_side = -1;
    fd_other_side = -1;
    mode = 0;
  }

  int set_up(int mode_to_set, char *IP, int port) {

    mode = mode_to_set;

    fd_this_side = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_this_side == -1)
      /* handle_err here is not really C++ */
      return -1;

    memset(&peersock_addr, 0, sizeof(struct sockaddr_in));
    peersock_addr.sin_family = AF_INET;
    peersock_addr.sin_port = htons(port);
    inet_pton(AF_INET, IP, &peersock_addr.sin_addr);

    if(mode == WAIT_FOR_FRIEND) {
      if(bind(fd_this_side, (struct sockaddr *)&peersock_addr, sizeof(struct sockaddr_in)) == -1) {
        stop();
        return -2;
      }
      if(listen(fd_this_side, 1) == -1) {
        stop();
        return -3;
      }
      printf("Will LISTEN for connections on %s:%d\n\n", IP, port);
    }
    else if(mode == CONNECT_TO_FRIEND) {
      printf("Will CONNECT to %s:%d\n\n", IP, port);
    }

    return 0;
  }

  int start(void) {

    printf("Starting up peer with mode = %d\n", mode);
    peersock_addr_size = sizeof(struct sockaddr_in);

    /* There are only 2 modes */
    if(mode == WAIT_FOR_FRIEND) {
      fd_other_side = accept(fd_this_side, (struct sockaddr *) &peersock_addr,
                            &peersock_addr_size);

      if(fd_other_side < 0) {
        perror(strerror(errno));
        stop();
      }

      return fd_other_side;
    }
    else if(mode == CONNECT_TO_FRIEND) {
      if(connect(fd_this_side, (struct sockaddr *) &peersock_addr,
                 peersock_addr_size) != 0) {

        perror(strerror(errno));
        stop();
        return -1;
      }
      return fd_this_side;
    }

    return -2;
  }

  void stop(void) {
    if(fd_this_side > 0)
      close(fd_this_side);

    if(fd_other_side > 0)
      close(fd_other_side);
  }

};

/* ITEM-1 : Uncomment this block if really required. Kept around if needed later.
void SIGINT_handler(int signum) {
  Close all TCP connections?
}
*/

int main(int argc, char *argv[]) {

  char USAGE[200];
  char buffer[1025];

  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;

  int retval, stdinfd, nfds;
  int connectedfd;

  char *msg_for_friend = NULL;
  int peermode;

  stdinfd = nfds = 0;

  sprintf(USAGE, "Usage: \n\t%s -listen listenIP listenPORT\n\tOR\n\t%s -friend friendIP friendPort\n", argv[0], argv[0]);

  /* Handle command line arguments for flexibility */
  if(argc!=4) {
    printf("%s", USAGE);
    return 1;
  }

  if(!strcmp(argv[1], "-listen")) {
    peermode = WAIT_FOR_FRIEND;
  }
  else if(!strcmp(argv[1], "-friend")) {
    peermode = CONNECT_TO_FRIEND;
  }
  else {
    printf("%s", USAGE);
    return 2;
  }

  /*
     ITEM-1 : Uncomment this block if really required. Kept around if needed later.
     // Set up a SIGINT catch
     // struct sigaction SIGINT_ACTION;
     // memset(&SIGINT_ACTION, 0, sizeof(struct sigaction));
     // SIGINT_ACTION.sa_handler = SIGINT_handler;
     // if(sigaction(SIGINT, &SIGINT_ACTION, NULL) == -1)
     // return 5;
  */

  peer myself;
  if (myself.set_up(peermode, argv[2], atoi(argv[3])) < 0)
    return -1;

  connectedfd = myself.start();
  if(connectedfd < 0) {
    return -2;
  }

  printf("Connection established with connectedfd = %d.\n", connectedfd);

  /* Main conversation */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    FD_SET(stdinfd, &readfds);
    FD_SET(connectedfd, &readfds);
    FD_SET(connectedfd, &writefds);
    FD_SET(connectedfd, &exceptfds);
    nfds = connectedfd + 1;

    /* Wait for data to arrive on the socket or stdin or a new connection */
    int selection = select(nfds, &readfds, &writefds, &exceptfds, NULL);

    if(selection == -1) {
      myself.stop();
      handle_err(7);
    }
    else if(selection) {

      /* Exceptions with the connected peer */
      if(FD_ISSET(connectedfd, &exceptfds)) {
        myself.stop();
        return 18;
      }

      /* Got a message over stdin */
      if(FD_ISSET(stdinfd, &readfds)) {
        int bytes_read = read(0, buffer, 1024);

        if(bytes_read < 0) {
          myself.stop();
          return 8;
        }

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        if(bytes_read > 1) {
          /* Mask the newline character we get in from stdin */
          buffer[bytes_read - 1] = '\0';

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

          if(retval == -1) {
            myself.stop();
            return 11;
          }
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

        if(retval == -1) {
          myself.stop();
          return 10;
        }
        if(retval == 0) {
          myself.stop();
          return 0;
        }

        buffer[retval] = '\0';
        printf("> %s \n", buffer);
      }
    }
  }

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  myself.stop();

  return 0;
}
