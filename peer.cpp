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
#include <sys/stat.h>
#include <fcntl.h>

#define CONNECT_TO_FRIEND 999
#define WAIT_FOR_FRIEND   998
#define IN_FILE "/tmp/peerin"
#define OUT_FILE "/tmp/peerout"

void errexit(int lastcode) {
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
  std::string file_input;
  std::string file_output;
  int fd_input_file;
  int fd_output_file;

public:
  peer() {
    fd_this_side = -1;
    fd_other_side = -1;

    /* STDIN and STDOUT by default */
    fd_input_file = 0;
    fd_output_file = 1;

    mode = 0;
  }

  int get_fd_in(void) {
    return fd_input_file;
  }

  int get_fd_out(void) {
    return fd_output_file;
  }

  int set_up(int mode_to_set, char *IP, int port, char *infile, char *outfile) {

    printf("[SETUP] MODE : %s\n", mode_to_set == WAIT_FOR_FRIEND ? "WAIT_FOR_FRIEND" :
           mode_to_set == CONNECT_TO_FRIEND ? "CONNECT_TO_FRIEND" : "UNKNOWN");
    printf("[SETUP] IP:Port : %s:%d\n", IP, port);
    printf("[SETUP] input file : %s\n", infile == NULL ? "stdin" : infile);
    printf("[SETUP] output file : %s\n", outfile == NULL ? "stdout" : outfile);

    mode = mode_to_set;

    if(infile != NULL)
      file_input = std::string(infile);

    if(outfile != NULL)
      file_output = std::string(outfile);

    fd_this_side = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_this_side == -1)
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
    }
    else if(mode == CONNECT_TO_FRIEND) {
      ;
    }

    /* in_file  */
    if(file_input.length() > 0) {
      fd_input_file = open(file_input.c_str(), O_RDONLY);
      if(fd_input_file == -1) {
        stop();
        return -4;
      }
    }

    /* out_file */ 
    if(file_output.length() > 0) {
      fd_output_file = open(file_output.c_str(), O_WRONLY | O_CREAT);
      if(fd_output_file == -1) {
        stop();
        return -5;
      }
    }

    printf("[SETUP] input file descriptor : %d\n", fd_input_file);
    printf("[SETUP] output file descriptor : %d\n", fd_output_file);

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

  int retval, nfds;
  int connectedfd;

  char *msg_for_friend = NULL;
  char *msg_for_us = NULL;
  int peermode;

  nfds = 0;

  sprintf(USAGE, "Usage: \n\t%s -listen listenIP listenPORT [ -infile ] [ -outfile] [ -inoutfile ]\n\tOR\n\t%s -friend friendIP friendPort [ -infile ] [ -outfile] [ -inoutfile ]\n", argv[0], argv[0]);

  /* Handle command line arguments for flexibility */
  if(argc < 4 || argc > 5) {
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
    errexit(2);
  }

  peer myself;
  if(argc > 4) {
    if(!strcmp(argv[4], "-infile")) {
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), IN_FILE, NULL) < 0)
        errexit(-2);
    }
    else if(!strcmp(argv[4], "-outfile")) {
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), NULL, OUT_FILE) < 0)
        errexit(-2);
    }
    else if(!strcmp(argv[4], "-inoutfile")) {
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), IN_FILE, OUT_FILE) < 0)
        errexit(-2);
    }
    else {
      printf("%s", USAGE);
      errexit(1);
    }
  }
  else {
    if(myself.set_up(peermode, argv[2], atoi(argv[3]), NULL, NULL) < 0)
      errexit(-1);
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

  connectedfd = myself.start();
  if(connectedfd < 0) {
    errexit(-2);
  }

  printf("Connection established with connectedfd = %d.\n", connectedfd);

  /* Main conversation */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    FD_SET(myself.get_fd_in(), &readfds);
    FD_SET(connectedfd, &readfds);
    /* ITEM-3 : Only add connectedfd to write fdset if we have data waiting to be written */
    FD_SET(connectedfd, &writefds);
    FD_SET(myself.get_fd_out(), &writefds);
    FD_SET(connectedfd, &exceptfds);
    FD_SET(myself.get_fd_in(), &exceptfds);

    nfds = (connectedfd > myself.get_fd_in() ? connectedfd : myself.get_fd_in()) + 1;

    /* Wait for data to arrive on the socket or stdin or a new connection */
    int selection = select(nfds, &readfds, &writefds, &exceptfds, NULL);

    if(selection == -1) {
      myself.stop();
      errexit(7);
    }
    else if(selection) {

      // printf("select returned : %d", selection);

      /* Exceptions with the connected peer */
      if(FD_ISSET(connectedfd, &exceptfds)) {
        myself.stop();
        return 18;
      }

      /* Got a message over peer's input fd */
      if(FD_ISSET(myself.get_fd_in(), &readfds)) {
        int bytes_read = read(myself.get_fd_in(), buffer, sizeof(buffer) - 1);

        printf("Bytes read : %d\n", bytes_read);

        if(bytes_read < 0) {
          myself.stop();
          return 8;
        }

        if(bytes_read == 0) {
          myself.stop();
          return 0;
        }

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        if(bytes_read > 1) {
          /* Mask the newline character we get in from stdin */
          buffer[bytes_read - 1] = '\0';

          int oldmsgsize = (msg_for_friend == NULL ? 0 : strlen(msg_for_friend));
          int newmsgsize = oldmsgsize + bytes_read + 1;
          printf("[GOT STDIN] old : %d, new %d\n", oldmsgsize, newmsgsize);
          msg_for_friend = (char *) realloc(msg_for_friend, newmsgsize);
          strcpy(msg_for_friend + oldmsgsize, buffer);
          msg_for_friend[newmsgsize - 1] = '\0';
          printf("msg for friend: %s\n", msg_for_friend);
        }
      }

      /* Send a message over TCP */
      if(FD_ISSET(connectedfd, &writefds)) {
        if(msg_for_friend != NULL) {
          int retval = write(connectedfd, msg_for_friend, strlen(msg_for_friend));

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
        int retval = read(connectedfd, buffer, sizeof(buffer) - 1);

        if(retval == -1) {
          myself.stop();
          return 10;
        }

        if(retval == 0) {
          myself.stop();
          return 0;
        }

        int oldmsgsize = (msg_for_us == NULL ? 0 : strlen(msg_for_us));
        int newmsgsize = oldmsgsize + retval + 1;
        printf("[GOT TCP] old : %d, new %d\n", oldmsgsize, newmsgsize);
        msg_for_us = (char *) realloc(msg_for_us, newmsgsize);
        strcpy(msg_for_us + oldmsgsize, buffer);
        msg_for_us[newmsgsize - 1] = '\0';
        printf("msg for us: %s\n", msg_for_us);
      }

      /* Write a message to peer's output fd */
      if(FD_ISSET(myself.get_fd_out(), &writefds)) {
        if(msg_for_us != NULL) {
          int retval = write(myself.get_fd_out(), msg_for_us, strlen(msg_for_us));

          if(retval == -1) {
            myself.stop();
            return 11;
          }
          else if(retval == strlen(msg_for_us)) {
            free(msg_for_us);
            msg_for_us = NULL;
          }
          else
            msg_for_us+=retval;
        }
      }
    }
  }

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  myself.stop();

  return 0;
}
