#include <stdio.h>
#include <string.h>
#include <iostream>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>
#include <openssl/err.h>

class Crypto {

public:

  Crypto() {
   struct passwd *pw = getpwuid(getuid());
   char *home_dir = pw->pw_dir;
   int sz = strlen(home_dir);

   /* Get rid of hard coded path names */
   char *peer_key_path = (char *)malloc(sz+16);
   char *private_key_path = (char *)malloc(sz+17);

   strcpy(peer_key_path, home_dir);
   strcat(peer_key_path, "/.ssh/public.pem");
   peer_key_path[sz+16] = '\0';
   printf("\nLoading friend's public key from: %s\n", peer_key_path);
   createRSA(peer_key_path, true);

   strcpy(private_key_path, home_dir);
   strcat(private_key_path, "/.ssh/private.pem");
   private_key_path[sz+17] = '\0';
   printf("\nLoading private key from: %s\n", private_key_path);
   createRSA(private_key_path, false);
  }

  RSA *peer_pub;
  RSA *private_key;
  unsigned int peer_key_size;
  unsigned int private_key_size;
  int padding = RSA_PKCS1_PADDING;
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, int data_len, unsigned char *encrypted);
  int decrypt(unsigned char *data, unsigned char *decrypted);
};
