#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>
#include <openssl/err.h>

class Crypto {
public:
  RSA *peer_pub;
  RSA *private_key;
  unsigned int peer_key_size;
  unsigned int private_key_size;
  int padding = RSA_PKCS1_PADDING;
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, unsigned char *encrypted);
  int encrypt(unsigned char *data, int data_len, unsigned char *encrypted);
  int decrypt(unsigned char *data, unsigned char *decrypted);
};
