#ifndef _CRYPTO_H_
#define _CRYPTO_H_
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
#include "util.h"

class Crypto {

public:

  RSA *public_key;
  RSA *private_key;
  unsigned int public_key_size;
  unsigned int private_key_size;
  int padding = RSA_PKCS1_PADDING;
  int max_message_length;

  Crypto(bool);
  void set_public_key(char *public_key_path);
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, int data_len, unsigned char*& encrypted);
  int decrypt(unsigned char *data, int data_len, unsigned char*& decrypted, int *unencrypted_length);
};
#endif /* _CRYPTO_H_ */
