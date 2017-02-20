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
  int padding = RSA_PKCS1_PADDING;
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, unsigned int data_len, unsigned char *encrypted);
  int decrypt(unsigned char *data, unsigned int data_len, unsigned char *decrypted);
};

RSA * Crypto::createRSA(char* filename, bool pub){
  FILE *fp = fopen(filename, "rb");
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  ERR_load_crypto_strings();
  if(fp==NULL) {
    perror(strerror(errno));
    perror("key file does not exist\n");
    exit(1);
  }

  RSA *rsa = RSA_new();
  if(pub) {
    rsa = PEM_read_RSA_PUBKEY(fp, &rsa, NULL, NULL);
    this->peer_pub = rsa;
    return this->peer_pub;
  }
  else {
    rsa = PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);
    if (rsa == NULL) {
      std::cout<<"privateKey could not be read"<<std::endl;
      exit(1);
    }
    this->private_key = rsa;
    return this->private_key;
  }
  return NULL;
}

int Crypto::encrypt(unsigned char *data, unsigned int data_len, unsigned char *encrypted){
  int rval;
  rval = RSA_public_encrypt(data_len, data, encrypted, this->peer_pub, padding);
  if(rval==-1){
    std::cout<<"Encryption failure: "<<ERR_get_error()<<std::endl;
    exit(1);
  }
}

int Crypto::decrypt(unsigned char *data, unsigned int data_len, unsigned char *decrypted){
  int rval;
  rval = RSA_private_decrypt(data_len,  (const unsigned char *)data, decrypted, private_key, padding);
   if(rval==-1){
     std::cout<<"Decryption failure"<<ERR_get_error()<<std::endl;
     exit(1);
   }
}

int main(int argc, char *argv[]) {
  if(argc<3) {
    std::cout<<"Usage: /path/to/private/pem /path/to/peer/public/pem message"<<std::endl;
    exit(1);
  }
  Crypto c;
  
  RSA *pr = c.createRSA(argv[1], false);
  RSA *pb = c.createRSA(argv[2], true);
  
  unsigned char *msg = (unsigned char*)strdup("secret message");
  int pb_size = RSA_size(pb);
  unsigned char *encrypted = (unsigned char*)malloc(pb_size);
  if(encrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
   }
  c.encrypt(msg, strlen((const char *)msg), encrypted);
  std::cout<<"Encrypted message:"<<encrypted<<std::endl;
  
  int pr_size = RSA_size(pr);
  unsigned char *decrypted = (unsigned char*)malloc(pr_size);
  if(decrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
   }
  c.decrypt(encrypted, pr_size, decrypted);
  std::cout<<"Decrypted message: "<<decrypted<<std::endl;
  
  return 0;
}
