#include "crypto.h"

RSA * Crypto::createRSA(char* filename, bool pub){
  printf("Got filename:%s\n", filename);
  FILE *fp = fopen(filename, "rb");
  if(fp==NULL) {
    perror(strerror(errno));
    printf("%s - key file does not exist\n", filename);
    exit(1);
  }
  printf("Opened the file\n");
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  ERR_load_crypto_strings();

  RSA *rsa = RSA_new();
  if(pub) {
    rsa = PEM_read_RSA_PUBKEY(fp, &rsa, NULL, NULL);
    if(rsa == NULL) {
      perror("failure loading peer key\n");
      exit(1);
    }
    this->peer_pub = rsa;
    this->peer_key_size = RSA_size(rsa);
    printf("Public Key loaded\n");
    return this->peer_pub;
  }
  else {
    rsa = PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);
    if (rsa == NULL) {
      std::cout<<"privateKey could not be read"<<std::endl;
      exit(1);
    }
    this->private_key = rsa;
    this->private_key_size = RSA_size(rsa);
    printf("Private key loaded\n");
    return this->private_key;
  }
  return NULL;
}

int Crypto::encrypt(unsigned char *data, unsigned char *encrypted){
  int rval;
  rval = RSA_public_encrypt(strlen((const char *)data), data, encrypted, this->peer_pub, padding);
  if(rval==-1){
    std::cout<<"Encryption failure: "<<ERR_get_error()<<std::endl;
    exit(1);
  }
}

int Crypto::encrypt(unsigned char *data, int data_len, unsigned char *encrypted){
  int rval;
  rval = RSA_public_encrypt(data_len, data, encrypted, this->peer_pub, padding);
  if(rval==-1){
    std::cout<<"Encryption failure: "<<ERR_get_error()<<std::endl;
    exit(1);
  }
}

int Crypto::decrypt(unsigned char *data, unsigned char *decrypted){
  int rval;
  rval = RSA_private_decrypt(this->private_key_size,  (const unsigned char *)data, decrypted, this->private_key, padding);
   if(rval==-1){
     std::cout<<"Decryption failure"<<ERR_get_error()<<std::endl;
     exit(1);
   }
}

/*
int main(int argc, char *argv[]) {
  if(argc<3) {
    std::cout<<"Usage: /path/to/private/pem /path/to/peer/public/pem message"<<std::endl;
    exit(1);
  }
  Crypto c;
  
  RSA *pr = c.createRSA(argv[1], false);
  RSA *pb = c.createRSA(argv[2], true);
  
  unsigned char *msg = (unsigned char*)strdup("secret message");
  //int pb_size = RSA_size(pb);
  unsigned char *encrypted = (unsigned char*)malloc(c.peer_key_size);
  if(encrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
   }
  c.encrypt(msg, encrypted);
  std::cout<<"Encrypted message:"<<encrypted<<std::endl;
  
  //int pr_size = RSA_size(pr);
  unsigned char *decrypted = (unsigned char*)malloc(c.private_key_size);
  if(decrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
   }
  c.decrypt(encrypted, decrypted);
  std::cout<<"Decrypted message: "<<decrypted<<std::endl;
  
  return 0;
  }*/
