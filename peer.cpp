#include   <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include "crypto.h"

#define CONNECT_TO_FRIEND 999
#define WAIT_FOR_FRIEND   998
#define IN_FILE "/tmp/peerin"
#define OUT_FILE "/tmp/peerout"
#define BUFF_SIZE 1024

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

  int set_up(int mode_to_set, char *IP, int port, char *infile, char *outfile) {

    printf("[SETUP] MODE : %s\n", mode_to_set == WAIT_FOR_FRIEND ? "WAIT_FOR_FRIEND" :
           mode_to_set == CONNECT_TO_FRIEND ? "CONNECT_TO_FRIEND" : "UNKNOWN");
    printf("[SETUP] IP:Port : %s:%d\n", IP, port);
    //    printf("[SETUP] input file : %s\n", infile == NULL ? "stdin" : infile);
    //    printf("[SETUP] output file : %s\n", outfile == NULL ? "stdout" : outfile);

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
        printf("Failure accepting connections\n");
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


/*
 *
 */

int main(int argc, char *argv[]) {

  char USAGE[200];
  char buffer[BUFF_SIZE];
  char infile[BUFF_SIZE], outfile[BUFF_SIZE];
  bool encrypt = false;

  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;

  int nfds;
  int connectedfd;

  unsigned char *msg_for_friend = NULL;
  unsigned char *send_msg_for_friend = NULL;
  int msg_for_friend_length = 0;
  int total_msg_for_friend_length = 0;
  unsigned char *msg_for_us = NULL;
  int msg_for_us_length = 0;
  unsigned char *encrypted_msg_for_us = NULL;
  unsigned char *recv_encrypted_msg_for_us = NULL;
  int encrypted_msg_for_us_length = 0;
  int total_encrypted_msg_for_us_length = 0;

  int peermode;
  Crypto c;

  nfds = 0;

  /*
   * Handling commandline args
   *
   */
  sprintf(USAGE, "Usage: \n\t%s -listen listenIP listenPORT [ -infile ] [ -outfile] [ -inoutfile ][-encrypt]\n\tOR\n\t%s -friend friendIP friendPort [ -infile ] [-encrypt]\n", argv[0], argv[0]);
  //sprintf(USAGE, "Usage: \n\t%s -listen listenIP listenPORT [ -infile ] [ -outfile] [ -inoutfile ]\n\tOR\n\t%s -friend friendIP friendPort [ -infile [infile] ] [ -outfile [outfile] ] [ -inoutfile ]\n", argv[0], argv[0]);
  
  /* Handle command line arguments for flexibility */
  if(argc < 4 || argc > 7) {
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
      if(argc > 5) {
        strcpy(infile, argv[5]);
        if (argc > 6) {
          if(!strcmp(argv[6], "-encrypt"))
            encrypt = true;
        }
      }
      else
        strcpy(infile, IN_FILE);
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), infile, NULL) < 0)
        errexit(-2);
    }
    else if(!strcmp(argv[4], "-outfile")) {
      if(argc > 5){
        strcpy(outfile, argv[5]);
        if (argc > 6) {
          if(!strcmp(argv[6], "-encrypt"))
            encrypt = true;
        }
      }
      else
        strcpy(outfile, OUT_FILE);
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), NULL, outfile) < 0)
        errexit(-2);
    }
    /* not really using this yet */
    else if(!strcmp(argv[4], "-inoutfile")) {
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), IN_FILE, OUT_FILE) < 0)
        errexit(-2);
    }
    else if(!strcmp(argv[4], "-encrypt")) {
      encrypt = true;
      if (myself.set_up(peermode, argv[2], atoi(argv[3]), NULL, NULL) < 0)
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
   * If encryption is enabled, set up crypto object 
   */
  if(encrypt) {
    struct passwd *pw = getpwuid(getuid());
    char *home_dir = pw->pw_dir;
    int sz = strlen(home_dir);

    char *peer_key_path = (char *)malloc(sz+16);
    strcpy(peer_key_path, home_dir);
    strcat(peer_key_path, "/.ssh/public.pem");
    peer_key_path[sz+16] = '\0';
    printf("\nLoading peer key from: %s\n", peer_key_path);
    c.createRSA(peer_key_path, true);
      
    char *private_key_path = (char *)malloc(sz+17);
    strcpy(private_key_path, home_dir);
    strcat(private_key_path, "/.ssh/private.pem");
    private_key_path[sz+17] = '\0';
    printf("\nLoading private key from: %s\n", private_key_path);      
    c.createRSA(private_key_path, false);
  }

  connectedfd = myself.start();
  if(connectedfd < 0) {
    printf("Failed with connectedfd:%d\n", connectedfd);
    errexit(-2);
  }

  printf("[MAIN] Connection established with connectedfd = %d.\n", connectedfd);

  bool fdin_read_ok = true;
  bool tcp_read_ok = true;
  
  /* 
   * Main conversation 
   */
  while(1) {

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    /* If nothing to send or receive, tear down and break */
    if((!fdin_read_ok &&  msg_for_friend_length<=0) || (!tcp_read_ok && msg_for_us_length<=0 && encrypted_msg_for_us_length<=0)) {
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
        int bytes_stdin_in = read(myself.get_fd_in(), buffer, sizeof(buffer));

        if(bytes_stdin_in < 0) {
          myself.stop();
          return 8;
        }

        if(bytes_stdin_in == 0) {
          fdin_read_ok = false;
          continue;
        }

        total_bytes_in += bytes_stdin_in;

        /* We don't care about user pressing Enter directly (bytes_stdin_in = 1)*/
        /* ITEM-n Mask the newline character we get in if it came from stdin ? */
        msg_for_friend = (unsigned char *) realloc(msg_for_friend, total_msg_for_friend_length + bytes_stdin_in);
        if(msg_for_friend == NULL) {
          myself.stop();
          errexit(9);
        }

        int i = 0;
        for(unsigned char *d = msg_for_friend + total_msg_for_friend_length;
            i < bytes_stdin_in;
            d++, i++) {
          *d = *(buffer+i);
        }

        msg_for_friend_length+=bytes_stdin_in; // todo: this should be used by sender block
        total_msg_for_friend_length+=bytes_stdin_in;
      }

      /* Write a message to peer's output fd */
      if(FD_ISSET(myself.get_fd_out(), &writefds) && msg_for_us_length > 0) {
        // printf("\nWriting to output fd\n");
        write(myself.get_fd_out(), "> ", 2);
        int bytes_stdout_out = write(myself.get_fd_out(), msg_for_us, msg_for_us_length);
      
        if(bytes_stdout_out == -1) {
          printf("Could not write for output fd\n");
          myself.stop();
          return 11;
        }

        total_bytes_out += bytes_stdout_out;
        //        printf("total_bytes_out: %d\n", total_bytes_out);

        if(bytes_stdout_out == msg_for_us_length) {
          free(msg_for_us);
          msg_for_us = NULL;
          msg_for_us_length = 0;
        }
        else {
          msg_for_us += bytes_stdout_out;
          msg_for_us_length -= bytes_stdout_out;
        }
      }

      /* Send a message over TCP */
      if(FD_ISSET(connectedfd, &writefds) && msg_for_friend_length > 0) {
        if(send_msg_for_friend == NULL)
          send_msg_for_friend = msg_for_friend;

        printf("\nSending message over TCP\n");
        int bytes_tcp_out;
        int msg_length;

        printf("Plain message length: %d\n", msg_for_friend_length);
        if(encrypt) {
          msg_length = msg_for_friend_length > c.peer_key_size - 11 ? c.peer_key_size - 11 : msg_for_friend_length;
          unsigned char *encrypted = (unsigned char *) malloc(c.peer_key_size);
          if(encrypted == NULL) {
            printf("Failed to allocate memory for encrypted message\n");
            exit(1);
          }

          //printf("Msg for friend to encrypt: %.*s\n", msg_length, send_msg_for_friend);
          int l = c.encrypt(send_msg_for_friend, msg_length, encrypted);

          printf("Sending encrypted msg tcp: ");
          for(int i = 0; i< l; i++)
            printf("%x,", encrypted[i]);
          printf("\n");

          bytes_tcp_out = write(connectedfd, encrypted, c.peer_key_size);
        }
        else 
          bytes_tcp_out = write(connectedfd, send_msg_for_friend, msg_for_friend_length);

        printf("bytes_tcp_out: %d\n", bytes_tcp_out);

        if(bytes_tcp_out == -1) {
          myself.stop();
          return 11;
        }

        total_bytes_tcp_out += bytes_tcp_out;

        printf("total_bytes_tcp_out: %d\n", total_bytes_tcp_out);

        if((encrypt && msg_length == msg_for_friend_length) || bytes_tcp_out == msg_for_friend_length) {
          free(msg_for_friend);
          send_msg_for_friend = NULL;
          msg_for_friend = NULL;
          msg_for_friend_length = 0;
          total_msg_for_friend_length = 0;
        }
        else if(encrypt) {
          send_msg_for_friend += msg_length;
          msg_for_friend_length -= msg_length;
        }
        else {
          send_msg_for_friend += bytes_tcp_out;
          msg_for_friend_length -= bytes_tcp_out;
        }
      }

      /* Got a message over TCP */
      if(FD_ISSET(connectedfd, &readfds)) {
        printf("buffer size %ld\n", sizeof(buffer));
        int bytes_read_tcp = read(connectedfd, buffer, sizeof(buffer));

        printf("bytes_read_tcp: %d\n", bytes_read_tcp);

        if(bytes_read_tcp == -1) {
          myself.stop();
          return 10;
        }

        total_bytes_tcp_in += bytes_read_tcp;
        printf("total_bytes_tcp_in: %d\n", total_bytes_tcp_in);
	
        if(bytes_read_tcp == 0 && encrypted_msg_for_us_length == 0) {
          tcp_read_ok = false;
          continue;
        }

        if(encrypt) {
          encrypted_msg_for_us = (unsigned char *)realloc(encrypted_msg_for_us, total_encrypted_msg_for_us_length + bytes_read_tcp);
          if(recv_encrypted_msg_for_us == NULL)
            recv_encrypted_msg_for_us = encrypted_msg_for_us;

          int i=0;
          /* copy message buffer to encrypted_msg_for_us */
          for(unsigned char *d = encrypted_msg_for_us + total_encrypted_msg_for_us_length; i < bytes_read_tcp; d++, i++) {
            *d = *(buffer+i);
          }

          printf("Encrypted_msg_for_us: ");
          for(int i = 0; i< bytes_read_tcp; i++)
            printf("%x,", encrypted_msg_for_us[i]);
          printf("\n");

          encrypted_msg_for_us_length += bytes_read_tcp;
          total_encrypted_msg_for_us_length += bytes_read_tcp;
	  
          if(encrypted_msg_for_us_length >= c.private_key_size){
            unsigned char *decrypted = (unsigned char*)malloc(c.private_key_size);
            if(decrypted == NULL) {
              printf("Failed to allocate memory for decrypted message\n");
              exit(1);
            }

            int decryptedlen = c.decrypt((unsigned char *)recv_encrypted_msg_for_us, decrypted);
            decrypted[decryptedlen]='\0';
            printf("Decrypted msg length: %d\n", decryptedlen);
            msg_for_us = (unsigned char *) realloc(msg_for_us, msg_for_us_length + decryptedlen);
            int i=0;
            for(unsigned char *d = msg_for_us + msg_for_us_length; i < decryptedlen; d++, i++) {
              *d = *(decrypted+i);
            }

            msg_for_us_length += decryptedlen;	     
            if(encrypted_msg_for_us_length == c.private_key_size) {
              //              printf("\nClearing up\n");
              free(encrypted_msg_for_us);
              encrypted_msg_for_us = NULL;
              recv_encrypted_msg_for_us = NULL;
              encrypted_msg_for_us_length = 0;
              total_encrypted_msg_for_us_length = 0;
            }
            else {
              recv_encrypted_msg_for_us += c.private_key_size;
              encrypted_msg_for_us_length -= c.private_key_size;
            }
          }
          //          printf("Encrypted_msg_for_us_length: %d\n", encrypted_msg_for_us_length);
        }
        else {
          msg_for_us = (unsigned char *) realloc(msg_for_us, msg_for_us_length + bytes_read_tcp);

          if(msg_for_us == NULL) {
            myself.stop();
            errexit(9);
          }

          int i = 0;
          for(unsigned char *d = msg_for_us + msg_for_us_length; i < bytes_read_tcp; d++, i++)
            *d = *(buffer+i);

          msg_for_us_length+=bytes_read_tcp;
        }
        //printf("msg_for_us_length: %d\n", msg_for_us_length);
      }
    }
  }

  return 0;
}
