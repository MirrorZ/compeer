#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IN_FILE "/tmp/peerin"
#define OUT_FILE "/tmp/peerout"

int total_bytes_in = 0;
int total_bytes_tcp_in = 0;
int total_bytes_out = 0;
int total_bytes_tcp_out = 0;

/*
   Stick to TCP for now.
   ITEM-2: Overload this to handle other types of connections.
*/

class peer {
  bool hasfriend;
  struct sockaddr_in peersock_addr;
  socklen_t peersock_addr_size;
  int fd_this_side;
  int fd_other_side;
  int fd_input_file;
  int fd_output_file;

public:
  peer() {
    fd_this_side = -1;
    fd_other_side = -1;

    /* STDIN and STDOUT by default */
    fd_input_file = 0;
    fd_output_file = 1;

  }

  int get_fd_in(void) {
    return fd_input_file;
  }

  int get_fd_out(void) {
    return fd_output_file;
  }

  void set_up(bool friendexists, char *IP, int port, char *infile, char *outfile) {

    hasfriend = friendexists;

    printf("Input file: %s\n", infile == NULL ? "NULL": infile);
    printf("Output file: %s\n", outfile == NULL ? "NULL": outfile);

    fd_this_side = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd_this_side != -1);

    memset(&peersock_addr, 0, sizeof(struct sockaddr_in));

    peersock_addr.sin_family = AF_INET;
    peersock_addr.sin_port = htons(port);

    inet_pton(AF_INET, IP, &peersock_addr.sin_addr);

    if(hasfriend == false) {
      assert(bind(fd_this_side, (struct sockaddr *)&peersock_addr, sizeof(struct sockaddr_in)) != -1);
      assert(listen(fd_this_side, 1) != -1);
    }

    if(infile != NULL) {
      fd_input_file = open(infile, O_RDONLY);
      assert(fd_input_file != -1);
    }

    if(outfile != NULL) {
      fd_output_file = open(outfile, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG);
      assert(fd_output_file != -1);
    }
  }

  int start(void) {

    printf("[START] Starting up peer [Has a friend?: %s]\n", hasfriend == true ? "Yes" : "No");
    peersock_addr_size = sizeof(struct sockaddr_in);

    /* modus operandi */
    if(hasfriend == false) {
      fd_other_side = accept(fd_this_side, (struct sockaddr *) &peersock_addr, &peersock_addr_size);
      assert(fd_other_side >= 0);
      return fd_other_side;
    }
    else {
      assert(connect(fd_this_side, (struct sockaddr *) &peersock_addr, peersock_addr_size) == 0);
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
  int port = -1;
  char *file_in=NULL, *file_out=NULL;

  if(argc!=6 && argc!=8 && argc!=10) {
    fprintf(stderr, "Usage: %s [-f | -l] -h IP -p PORT [-i infile] [-o outfile]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  while((opt = getopt(argc, argv, "lfi:p:h:o:")) != -1) {
    switch(opt) {

    case 'f':
      hasfriend = true;
      break;

    case 'l':
      hasfriend = false;
      break;

    case 'h':
      ip = strdup(optarg);
      printf("ip is : %s\n", ip);
      break;

    case 'p':
      port = atoi(optarg);
      break;

    case 'i':
      file_in = (char *) malloc(strlen(optarg) + 1);
      assert(file_in!=NULL);
      strcpy(file_in, optarg);
      break;

    case 'o':
      file_out = (char *) malloc(strlen(optarg) + 1);
      assert(file_out!=NULL);
      strcpy(file_out, optarg);
      break;

   default:
     fprintf(stderr, "Usage: %s [-f | -l] -h IP -p PORT [-i infile] [-o outfile]\n", argv[0]);
     exit(EXIT_FAILURE);
    }
  }

  assert(ip!=NULL);
  assert(port!=-1);


  peer myself;
  myself.set_up(hasfriend, ip, port, file_in, file_out);
  assert((connectedfd = myself.start()) >= 0);

  printf("[MAIN] Connection established with connectedfd = %d.\n", connectedfd);
  bool fdin_read_ok = true;
  bool tcp_read_ok = true;

  /* Main conversation */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    /* If nothing to send or receive, tear down and break */
    if((!fdin_read_ok && msg_for_friend_length <= 0) || (!tcp_read_ok && msg_for_us_length <= 0)) {
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
    assert(selection != -1);

    if(selection) {

      /* Exceptions with the connected peer */
      /* ITEM-n Replace with a list of peers ? Or atleast an || condition for other fd */
      if(FD_ISSET(connectedfd, &exceptfds) ||
         FD_ISSET(myself.get_fd_in(), &exceptfds) ||
         FD_ISSET(myself.get_fd_out(), &exceptfds)) {

        myself.stop();
        return 2;
      }

      /* Got a message over peer's input fd */
      if(FD_ISSET(myself.get_fd_in(), &readfds)) {
        int bytes_read = read(myself.get_fd_in(), buffer, sizeof(buffer) - 1);

        assert(bytes_read >= 0);

        if(bytes_read == 0) {
          fdin_read_ok = false;
          continue;
        }

        total_bytes_in += bytes_read;

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        /* ITEM-n Mask the newline character we get in if it came from stdin ? */
        msg_for_friend = (char *) realloc(msg_for_friend, msg_for_friend_length + bytes_read);
        assert(msg_for_friend != NULL);

        memcpy(msg_for_friend + msg_for_friend_length, buffer, bytes_read);
        msg_for_friend_length+=bytes_read;
      }

      /* Write a message to peer's output fd */
      if(FD_ISSET(myself.get_fd_out(), &writefds) && msg_for_us_length > 0) {
        int bytes_written = write(myself.get_fd_out(), msg_for_us, msg_for_us_length);

        assert(bytes_written != -1);

        total_bytes_out += bytes_written;

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
        int bytes_written_tcp = write(connectedfd, msg_for_friend, msg_for_friend_length);

        assert(bytes_written_tcp != -1);

        total_bytes_tcp_out += bytes_written_tcp;

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
        int bytes_read_tcp = read(connectedfd, buffer, sizeof(buffer) - 1);

        assert(bytes_read_tcp != -1);
        total_bytes_tcp_in += bytes_read_tcp;

        if(bytes_read_tcp == 0) {
          tcp_read_ok = false;
          continue;
        }

        msg_for_us = (char *) realloc(msg_for_us, msg_for_us_length + bytes_read_tcp);
        assert(msg_for_us != NULL);

        memcpy(msg_for_us + msg_for_us_length, buffer, bytes_read_tcp);
        msg_for_us_length+=bytes_read_tcp;
      }
    }
  }

  return 0;
}
