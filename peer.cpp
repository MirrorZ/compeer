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
#include "crypto.h"

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

    fprintf(stderr,"Input file: %s\n", infile == NULL ? "NULL": infile);
    fprintf(stderr,"Output file: %s\n", outfile == NULL ? "NULL": outfile);

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

  unsigned char buffer[1025];
  buffer[1024] = '\0';
  
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;

  int nfds;
  int connectedfd;
  bool hasfriend = true;
  unsigned char *msg_for_friend = NULL;
  int msg_for_friend_length = 0;
  unsigned char *msg_for_us = NULL;
  int msg_for_us_length = 0;

  bool encrypt = false;
  int unencrypted_length, decrypt_from, total_decrypt_msg_length;
  unsigned char *decrypt_msg = NULL;
  total_decrypt_msg_length = unencrypted_length = decrypt_from = 0;

  /* Redirect stderr to logfile */

  nfds = 0;
  int opt;
  char *ip = NULL;
  int port = -1;
  char *file_in=NULL, *file_out=NULL;
  
  if(argc<6 || argc>11) {
    fprintf(stdout, "Usage: %s [-f | -l] -h IP -p PORT [-i infile] [-o outfile] -e\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  while((opt = getopt(argc, argv, "lfei:p:h:o:")) != -1) {
    switch(opt) {

    case 'f':
      hasfriend = true;
      break;

    case 'l':
      hasfriend = false;
      break;

    case 'h':
      ip = strdup(optarg);
      break;

    case 'p':
      port = atoi(optarg);
      break;

    case 'i':
      file_in = (char *) malloc(strlen(optarg) + 1);
      assert(file_in != NULL);
      strcpy(file_in, optarg);
      break;

    case 'o':
      file_out = (char *) malloc(strlen(optarg) + 1);
      assert(file_out != NULL);
      strcpy(file_out, optarg);
      break;

    case 'e':
      encrypt = true;
      break;

   default:
     fprintf(stdout, "Usage: %s [-f | -l] -h IP -p PORT [-i infile] [-o outfile] -e\n", argv[0]);
     exit(EXIT_FAILURE);
    }
  }

  assert(ip!=NULL);
  assert(port!=-1);

  /* Redirect stdout to logfile */
  char logfile[20];
  if(hasfriend){
    sprintf(logfile, "listener.log");
  }
  else {
    sprintf(logfile, "friend.log");
  }
  freopen(logfile, "a+", stderr);

  peer myself;
  Crypto crypto;
  crypto.set_public_key(NULL);
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
        unsigned char* message = NULL;
        int bytes_send;
        int bytes_read = read(myself.get_fd_in(), buffer, sizeof(buffer) - 1);

        assert(bytes_read >= 0);

        if(bytes_read == 0) {
          fdin_read_ok = false;
          continue;
        }

        total_bytes_in += bytes_read;

        if(encrypt){
          bytes_send = crypto.encrypt(buffer, bytes_read, message);
        }
        else {
          message = buffer;
          bytes_send = bytes_read;
        }

        /* We don't care about user pressing Enter directly (bytes_read = 1)*/
        /* ITEM-n Mask the newline character we get in if it came from stdin ? */
        msg_for_friend = (unsigned char *) realloc(msg_for_friend, msg_for_friend_length + bytes_send);
        assert(msg_for_friend != NULL);
        memcpy(msg_for_friend + msg_for_friend_length, message, bytes_send);
        msg_for_friend_length += bytes_send;

      }

      /* Write a message to peer's output fd */
      if(FD_ISSET(myself.get_fd_out(), &writefds) && msg_for_us_length > 0) {

	if(myself.get_fd_out()==1)
	  msg_for_us[msg_for_us_length] = '\0';
	
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
	unsigned char *message = NULL;
	int bytes_to_write;
        int bytes_read_tcp = read(connectedfd, buffer, sizeof(buffer) - 1);

        assert(bytes_read_tcp != -1);
        total_bytes_tcp_in += bytes_read_tcp;

        if(bytes_read_tcp == 0) {
          tcp_read_ok = false;
          continue;
        }

	if(encrypt){
	  /* Append received message to decrypt_msg buffer*/
	  decrypt_msg = (unsigned char*)realloc(decrypt_msg, total_decrypt_msg_length+bytes_read_tcp);
	  memcpy(decrypt_msg + total_decrypt_msg_length, buffer, bytes_read_tcp);
	  total_decrypt_msg_length += bytes_read_tcp;
	  int data_len = unencrypted_length + bytes_read_tcp;

	  fprintf(stderr, "Total decrypt msg_length: %d\n", total_decrypt_msg_length);
	  fprintf(stderr, "Before decryption decrypt_from:%d, data_len:%d, unencryptd_length:%d\n", decrypt_from, data_len, unencrypted_length);

	  fprintf(stderr,"Sending to decrypt\n");
	  print_block(decrypt_msg+decrypt_from, data_len);

	  bytes_to_write = crypto.decrypt(decrypt_msg+decrypt_from, data_len, message, &unencrypted_length); 
	  if(unencrypted_length==0) {
	    free(decrypt_msg);
	    decrypt_msg = NULL;
	    total_decrypt_msg_length = 0;
	    decrypt_from = 0;
	  }
	  else {
	    decrypt_from = data_len - unencrypted_length;
	  }

	  fprintf(stderr,"\nAfter decryption decrypt_from:%d, data_len:%d, unencryptd_length:%d\n", decrypt_from, data_len, unencrypted_length);

	}
	else{
	  message = buffer;
	  bytes_to_write = bytes_read_tcp;
	}

        msg_for_us = (unsigned char *)realloc(msg_for_us, msg_for_us_length + bytes_to_write);
        assert(msg_for_us != NULL);

        memcpy(msg_for_us + msg_for_us_length, message, bytes_to_write);
        msg_for_us_length+=bytes_to_write;
      }
    }
  }
  return 0;
}
