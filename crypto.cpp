#include "crypto.h"

Crypto::Crypto(){
  struct passwd *pw = getpwuid(getuid());
  char *home_dir = pw->pw_dir;
  int sz = strlen(home_dir);
  
  char *private_key_path = (char *)malloc(sz+17);
  strcpy(private_key_path, home_dir);
  strcat(private_key_path, "/.ssh/private.pem");
  private_key_path[sz+17] = '\0';
  printf("\nLoading private key from: %s\n", private_key_path);
  createRSA(private_key_path, false);
}

void Crypto::set_peer_key(char *peer_key_path){
  
  if(peer_key_path == NULL) {
    printf("Using default peer key path (~/compeer/.vault/peer.pem");
    char path[50] = ".vault/peer.pem";
    peer_key_path = path;
  }
  printf("\nLoading friend's public key from: %s\n", peer_key_path);
  createRSA(peer_key_path, true);
  
  /* For RSA_PKCS1_PADDING maximum message length can be the key size - 11 */
  this->max_message_length = this->peer_key_size - 11;
  printf("Setting max_message_length: %d\n", this->max_message_length);
}

RSA * Crypto::createRSA(char* filename, bool pub){
  printf("Got filename: %s\n", filename);
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  ERR_load_crypto_strings();

  FILE *fp = fopen(filename, "rb");
  if(fp==NULL) {
    perror(strerror(errno));
    printf("%s - key file does not exist\n", filename);
    exit(1);
  }
  printf("Opened the file\n");
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

int Crypto::encrypt(unsigned char *data, int data_len, unsigned char *encrypted){
  int rval;
  int len, encrypted_length=0;
  while(data_len!=0){
    len = data_len > this->max_message_length ? this->max_message_length : data_len;
    printf("Encrypting %d data \n", len);
    unsigned char *block_encrypted = (unsigned char*)malloc(this->peer_key_size);
    if(block_encrypted == NULL) {
      std::cout<<"Failed to allocate memory"<<std::endl;
      exit(1);
    }
    rval = RSA_public_encrypt(len, data, block_encrypted, this->peer_pub, padding);
    if(rval==-1){
      std::cout<<"Encryption failure: "<<ERR_get_error()<<std::endl;
      exit(1);
    }
    /* update the pointers */
    data += len;
    data_len -= len;
    encrypted = (unsigned char *)realloc(encrypted, encrypted_length+rval);
    for(int i=0; i<rval; i++)
      encrypted[i+encrypted_length] = block_encrypted[i];
    
    encrypted_length += rval;
  }
  encrypted[encrypted_length] = '\0';
  printf("Sending %s with length %d\n", encrypted, encrypted_length);
  return encrypted_length;
}

int Crypto::decrypt(unsigned char *data, unsigned char *decrypted){
  int rval;
  printf("Got data %s, using %d\n", data, this->private_key_size);
  rval = RSA_private_decrypt(this->private_key_size, (const unsigned char *)data, decrypted, this->private_key, padding);

  if(rval==-1){
    std::cout<<"Decryption failure: "<<ERR_get_error()<<std::endl;
    exit(1);
  }

  return rval;
}


int main(int argc, char *argv[]) {
  /*if(argc<3) {
    std::cout<<"Usage: /path/to/private/pem /path/to/peer/public/pem message"<<std::endl;
    exit(1);
    }*/
  Crypto c;
  c.set_peer_key(NULL);
  
  //RSA *pr = c.createRSA(argv[1], false);
  //RSA *pb = c.createRSA(argv[2], true);
  
  //unsigned char *msg = (unsigned char*)strdup("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  unsigned char *msg = (unsigned char*)strdup("aaaaa");
  //int pb_size = RSA_size(pb);
  unsigned char* encrypted = (unsigned char *)malloc(strlen((const char*)msg));
  c.encrypt(msg, strlen((const char*)msg), encrypted);
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
  }

