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
  
  RSA *peer_pub;
  RSA *private_key;
  unsigned int peer_key_size;
  unsigned int private_key_size;
  int padding = RSA_PKCS1_PADDING;
  int max_message_length;

  Crypto();
  void set_peer_key(char *peer_key_path);
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, int data_len, unsigned char *encrypted);
  int decrypt(unsigned char *data, unsigned char *decrypted);
};
