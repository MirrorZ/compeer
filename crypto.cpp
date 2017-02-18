#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>
#include <openssl/err.h>

class Crypto {
  RSA * peer_pub;
  RSA * private_key;

public:
  int padding = RSA_PKCS1_PADDING;
  RSA * createRSA(char* filename, bool pub);   // Creates RSA structure from key file
  int encrypt(unsigned char *data, int data_len, unsigned char *encrypted );
  int decrypt(unsigned char *data, int data_len, unsigned char *decrypted);
};

RSA * Crypto::createRSA(char* filename, bool pub){
  FILE *fp = fopen(filename, "rb");
  if(fp==NULL) {
    perror(strerror(errno));
    std::cout<<"Reading "<<filename<<std::endl;
    perror("key file does not exist\n");
    exit(1);
  }
  
  if(pub) {
    RSA *peer_pub = RSA_new();
    peer_pub = PEM_read_RSA_PUBKEY(fp, &peer_pub, NULL, NULL);
    std::cout<<RSA_size(peer_pub)<<std::endl;
    return peer_pub;
  }
  else {
    RSA *private_key = RSA_new();
    private_key = PEM_read_RSAPrivateKey(fp, &private_key, NULL, NULL);
    return private_key;
  }
  return NULL;
}

int Crypto::encrypt(unsigned char *data, int data_len, unsigned char *encrypted){
  int rval;
  data_len = sizeof(* data);
  std::cout<<"got here"<<std::endl;
  rval = RSA_public_encrypt(data_len, data, encrypted, peer_pub, padding);
  if(rval==-1){
    std::cout<<"Encryption failure"<<ERR_get_error()<<std::endl;
    std::cout<<"Encryption failure"<<std::endl;
    exit(1);
  }
 
  // std::cout<<"Encrytped message"<<encrypted<<std::endl;
}

int Crypto::decrypt(unsigned char *data, int data_len, unsigned char *decrypted){
   int rval;
   decrypted = (unsigned char*)malloc(RSA_size(private_key));
   if(decrypted == NULL) {
     std::cout<<"Failed to allocate memory"<<std::endl;
     exit(1);
   }

   rval = RSA_public_encrypt(data_len,  (const unsigned char *)data, decrypted, private_key, padding);
   if(rval==-1){
     // std::cout<<"Encryption failure"<<err_get_error()<<std::endl;
     std::cout<<"Encryption failure"<<std::endl;
     exit(1);
   }
    std::cout<<"Decrytped message"<<decrypted<<std::endl;
}

int main() {
  Crypto c;
  char *msg= "hello";
  RSA *pr = c.createRSA("/home/mirrorz/.ssh/private.pem", false);
  RSA *pb = c.createRSA("/home/mirrorz/.ssh/public.pem", true);

  std::cout<<RSA_size(pb)<<std::endl;
  unsigned char *encrypted = (unsigned char*)malloc(256);
  if(encrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
    }
  std::cout<<sizeof(encrypted)<<std::endl;
  c.encrypt((unsigned char *)&msg, 5, encrypted);//(unsigned char *)&dmsg);
  return 0;
}
