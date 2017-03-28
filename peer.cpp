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
#include <algorithm>
#include <iostream>
#include <getopt.h>

#define CONNECT_TO_FRIEND 999
#define WAIT_FOR_FRIEND   998
#define IN_FILE "/tmp/peerin"
#define OUT_FILE "/tmp/peerout"

int total_bytes_in = 0;
int total_bytes_tcp_in = 0;
int total_bytes_out = 0;
int total_bytes_tcp_out = 0;

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

  int set_up(bool hasfriend, char *IP, int port, char *infile, char *outfile) {

    mode = (hasfriend == true ? CONNECT_TO_FRIEND : WAIT_FOR_FRIEND);

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
	printf("%s does not exist", file_input.c_str());
        stop();
        return -4;
      }
    }

    /* out_file */
    if(file_output.length() > 0) {
      fd_output_file = open(file_output.c_str(), O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG);
      if(fd_output_file == -1) {
        stop();
        return -5;
      }
    }

    //printf("[SETUP] input file descriptor : %d\n", fd_input_file);
    //printf("[SETUP] output file descriptor : %d\n", fd_output_file);

    return 0;
  }

  int start(void) {

    printf("[START] Starting up peer with mode = %d\n", mode);
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

    if(fd_input_file > 0)
      close(fd_input_file);

    if(fd_output_file > 0)
      close(fd_output_file);
  }
};

int main(int argc, char *argv[]) {

  char buffer[1025];
  buffer[1024] = '\0';

  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;

  int nfds;
  int connectedfd;
  bool hasfriend = true;
  char *msg_for_friend = NULL;
  int msg_for_friend_length = 0;
  char *msg_for_us = NULL;
  int msg_for_us_length = 0;

  nfds = 0;
  int opt;
  char *ip = NULL;
  int port;

  if(argc!=6) {
    fprintf(stderr, "Usage: %s [-f | -l] -i IP -p PORT\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  while((opt = getopt(argc, argv, "lfi:p:")) != -1) {
    switch(opt) {

    case 'f':
      hasfriend = true;
      break;

    case 'l':
      hasfriend = false;
      break;

    case 'i':
      ip = strdup(optarg);
      break;

    case 'p':
      port = atoi(optarg);
      break;

   default:
     fprintf(stderr, "Usage: %s [-f | -l] -i IP -p PORT\n", argv[0]);
     exit(EXIT_FAILURE);
    }
  }

  peer myself;
  int ret = myself.set_up(hasfriend, ip, port, NULL, NULL);

  if(ret < 0) {
    printf("We are fucked\n");
    exit(10);
  }

  connectedfd = myself.start();
  if(connectedfd < 0) {
    errexit(-2);
  }

  printf("[MAIN] Connection established with connectedfd = %d.\n", connectedfd);

  bool fdin_read_ok = true;
  bool tcp_read_ok = true;

  /* Main conversation */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    /* If nothing to send or receive, tear down and break */
    if((!fdin_read_ok && msg_for_friend_length<=0) || (!tcp_read_ok && msg_for_us_length<=0)) {
      myself.stop();
      break;
    }

    if(fdin_read_ok)
      FD_SET(myself.get_fd_in(), &readfds);

    if(tcp_read_ok)
      FD_SET(connectedfd, &readfds);

    if(msg_for_friend_length > 0)
      FD_SET(connectedfd, &writefds);

    if(msg_for_us_length > 0)
      FD_SET(myself.get_fd_out(), &writefds);

    FD_SET(connectedfd, &exceptfds);
    FD_SET(myself.get_fd_in(), &exceptfds);
    FD_SET(myself.get_fd_out(), &exceptfds);

    nfds = std::max(connectedfd, myself.get_fd_in());
    nfds = std::max(nfds, myself.get_fd_out());
    nfds = nfds + 1;

    /* Wait for data to arrive on the socket or stdin or a new connection */
    int selection = select(nfds, &readfds, &writefds, &exceptfds, NULL);

    if(selection == -1) {
      myself.stop();
      errexit(7);
    }
    else if(selection) {

      /* Exceptions with the connected peer */
      /* ITEM-n Replace with a list of peers ? Or atleast an || condition for other fd */
      if(FD_ISSET(connectedfd, &exceptfds) ||
         FD_ISSET(myself.get_fd_in(), &exceptfds) ||
         FD_ISSET(myself.get_fd_out(), &exceptfds)) {

        myself.stop();
        return 18;
      }

      /* Got a message over peer's input fd */
      if(FD_ISSET(myself.get_fd_in(), &readfds)) {
        //	printf("\nInput fd set\n");
        int bytes_read = read(myself.get_fd_in(), buffer, sizeof(buffer) - 1);
        //        printf("Bytes read in: %d\n", bytes_read);

        if(bytes_read < 0) {
          myself.stop();
          return 8;
        }

        if(bytes_read == 0) {
          fdin_read_ok = false;
          continue;
        }

        total_bytes_in += bytes_read;
        //printf("total_bytes_in : %d\n", total_bytes_in);

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        /* ITEM-n Mask the newline character we get in if it came from stdin ? */
        msg_for_friend = (char *) realloc(msg_for_friend, msg_for_friend_length + bytes_read);
        if(msg_for_friend == NULL) {
          myself.stop();
          errexit(9);
        }

        int i = 0;
        for(char *d = msg_for_friend + msg_for_friend_length;
            i < bytes_read;
            d++, i++) {
          *d = *(buffer+i);
        }

        msg_for_friend_length+=bytes_read;
        //printf("msg_for_friend_length: %d\n", msg_for_friend_length);
      }

      /* Write a message to peer's output fd */
      if(FD_ISSET(myself.get_fd_out(), &writefds) && msg_for_us_length > 0) {
        //	printf("\nWriting to output fd\n");
        int bytes_written = write(myself.get_fd_out(), msg_for_us, msg_for_us_length);

        if(bytes_written == -1) {
          myself.stop();
          return 11;
        }

        total_bytes_out += bytes_written;
        //        printf("total_bytes_out: %d\n", total_bytes_out);

        if(bytes_written == msg_for_us_length) {
          free(msg_for_us);
          msg_for_us = NULL;
          msg_for_us_length = 0;
        }
        else {
          msg_for_us += bytes_written;
          msg_for_us_length -= bytes_written;
        }
      }

      /* Send a message over TCP */
      if(FD_ISSET(connectedfd, &writefds) && msg_for_friend_length > 0) {
        //	printf("\nSending message over TCP\n");
        int bytes_written_tcp = write(connectedfd, msg_for_friend, msg_for_friend_length);

        if(bytes_written_tcp == -1) {
          myself.stop();
          return 11;
        }

        total_bytes_tcp_out += bytes_written_tcp;
        //        printf("total_bytes_tcp_sent: %d\n", total_bytes_tcp_out);

        if(bytes_written_tcp == msg_for_friend_length) {
          free(msg_for_friend);
          msg_for_friend = NULL;
          msg_for_friend_length = 0;
        }
        else {
          msg_for_friend += bytes_written_tcp;
          msg_for_friend_length -= bytes_written_tcp;
        }
      }

      /* Got a message over TCP */
      if(FD_ISSET(connectedfd, &readfds)) {
        //	printf("\nReceived message\n");
        int bytes_read_tcp = read(connectedfd, buffer, sizeof(buffer) - 1);

        if(bytes_read_tcp == -1) {
          myself.stop();
          return 10;
        }

        total_bytes_tcp_in += bytes_read_tcp;
        //        printf("total_bytes_tcp_received: %d\n", total_bytes_tcp_in);

        if(bytes_read_tcp == 0) {
          tcp_read_ok = false;
          continue;
        }

        msg_for_us = (char *) realloc(msg_for_us, msg_for_us_length + bytes_read_tcp);
        if(msg_for_us == NULL) {
          myself.stop();
          errexit(9);
        }

        int i = 0;
        for(char *d = msg_for_us + msg_for_us_length;
            i < bytes_read_tcp;
            d++, i++) {
          *d = *(buffer+i);
        }

        msg_for_us_length+=bytes_read_tcp;
        //printf("msg_for_us_length: %d\n", msg_for_us_length);
      }
    }
  }

  return 0;
}
