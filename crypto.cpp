#include "crypto.h"
#include "util.h"
Crypto::Crypto(){
  struct passwd *pw = getpwuid(getuid());
  char *home_dir = pw->pw_dir;
  int sz = strlen(home_dir);
  
  char *private_key_path = (char *)malloc(sz+17);
  strcpy(private_key_path, home_dir);
  strcat(private_key_path, "/.ssh/private.pem");
  private_key_path[sz+17] = '\0';
  printf("\nprivate key: %s\n", private_key_path);
  createRSA(private_key_path, false);
}

void Crypto::set_public_key(char *public_key_path){
  
  if(public_key_path == NULL) {
    printf("Using default public key path (.vault/public.pem");
    char path[50] = ".vault/public.pem";
    public_key_path = path;
  }
  printf("\npublic key: %s\n", public_key_path);
  createRSA(public_key_path, true);
  
  /* For RSA_PKCS1_PADDING maximum message length can be the key size - 11 */
  this->max_message_length = this->public_key_size - 11;
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
      perror("failure loading public key\n");
      exit(1);
    }
    this->public_key = rsa;
    this->public_key_size = RSA_size(rsa);
    printf("Public Key loaded\n");
    return this->public_key;
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

int Crypto::encrypt(unsigned char *data, int data_len, unsigned char*& encrypted_data){
  int rval;
  int len, encrypted_length=0;
  unsigned char *encrypted = NULL;
  printf("=============>IN FUNC: %u\n", encrypted);

  while(data_len!=0){
    len = data_len > this->max_message_length ? this->max_message_length : data_len;
    printf("Encrypting %d data \n", len);
    unsigned char *block_encrypted = (unsigned char*)malloc(this->public_key_size);
    if(block_encrypted == NULL) {
      std::cout<<"Failed to allocate memory"<<std::endl;
      exit(1);
    }
    rval = RSA_public_encrypt(len, data, block_encrypted, this->public_key, padding);
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
  // encrypted[encrypted_length] = '\0';
  printf("=============>IN FUNC (after realloc): %u\n", encrypted);

  printf("Returning ");
  print_block(encrypted, encrypted_length);

  encrypted_data = encrypted;
  return encrypted_length;
}

int Crypto::decrypt(unsigned char* data, int *data_len, unsigned char*& decrypted_data){
  
  int rval;
  int len, decrypted_length=0;
  unsigned char *block_decrypted = NULL;
  unsigned char *decrypted = NULL;
  unsigned int size = this->private_key_size;
  int data_length = *data_len;
  
  while(data_length>=size){
    unsigned char *block_decrypted = (unsigned char*)malloc(size);
    if(block_decrypted == NULL) {
      std::cout<<"Failed to allocate memory"<<std::endl;
      exit(1);
    }
     printf("Decrypting\n");
     //print_block(data, data_len);
 
    rval = RSA_private_decrypt(size, (const unsigned char *)data, block_decrypted, this->private_key, padding);

    if(rval==-1){
      std::cout<<"Decryption failure: "<<ERR_get_error()<<std::endl;
      exit(1);
    }
    data += size;
    data_length -= size;
    
    decrypted = (unsigned char *)realloc(decrypted, decrypted_length+rval);
    for(int i=0; i<rval; i++)
      decrypted[i+decrypted_length] = block_decrypted[i];
    decrypted_length += rval;
    printf("Decrypted_length: %d\n", decrypted_length);
  }

  decrypted_data = decrypted;
  printf("\nIn decrypt: %s\n", decrypted);

  *data_len = data_length;
  return decrypted_length;
  
  /*

  rval = RSA_private_decrypt(this->private_key_size, (const unsigned char *)data, decrypted, this->private_key, padding);

  if(rval==-1){
    std::cout<<"Decryption failure: "<<ERR_get_error()<<std::endl;
    exit(1);
  }

  return rval;*/
}


int main(int argc, char *argv[]) {
  /*if(argc<3) {
    std::cout<<"Usage: /path/to/private/pem /path/to/public/pem message"<<std::endl;
    exit(1);
    }*/
  Crypto c;
  c.set_public_key(NULL);
  
  //RSA *pr = c.createRSA(argv[1], false);
  //RSA *pb = c.createRSA(argv[2], true);
  
  unsigned char *msg = (unsigned char*)strdup("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  //unsigned char *msg = (unsigned char*)strdup("aaaaa");
  //int pb_size = RSA_size(pb);
  unsigned char* encrypted = NULL;  //(unsigned char *)malloc(strlen((const char*)msg));
  printf("======> IN MAIN: %u\n", encrypted);
  int elen = c.encrypt(msg, strlen((const char*)msg), encrypted);
  printf("======> IN MAIN: %u\n", encrypted);
  std::cout<<"Returned data: ";
  print_block(encrypted, elen);
  // int pr_size = RSA_size(pr);
  // return 1;

  unsigned char *decrypted = NULL;//(unsigned char*)malloc(c.private_key_size);
  /*if(decrypted == NULL) {
    std::cout<<"Failed to allocate memory"<<std::endl;
    exit(1);
    }*/

  elen -=2;
  int dlen = c.decrypt(encrypted, &elen, decrypted);
  printf("Len remaining %d\n", elen);
  decrypted[dlen]='\0';
  printf("Decrypted data [%d]: %s\n", dlen, decrypted);

  return 0;
  }

