#include "crypto.h"

Crypto::Crypto(){
  struct passwd *pw = getpwuid(getuid());
  char *home_dir = pw->pw_dir;
  int sz = strlen(home_dir);
  
  char *private_key_path = (char *)malloc(sz+17);
  strcpy(private_key_path, home_dir);
  strcat(private_key_path, "/.ssh/private.pem");
  private_key_path[sz+17] = '\0';
  fprintf(stderr, "\nPrivate key path: %s\n", private_key_path);
  createRSA(private_key_path, false);
}

void Crypto::set_public_key(char *public_key_path){

  if(public_key_path == NULL) {
    struct passwd *pw = getpwuid(getuid());
    printf("Public key path: %s/.vault/public.pem\n", pw->pw_dir);
    char path[50] = ".vault/public.pem";
    public_key_path = path;
  }
  createRSA(public_key_path, true);
  
  /* For RSA_PKCS1_PADDING maximum message length can be the key size - 11 */
  this->max_message_length = this->public_key_size - 11;
}

RSA * Crypto::createRSA(char* filename, bool pub){
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_ciphers();
  ERR_load_crypto_strings();

  FILE *fp = fopen(filename, "rb");
  if(fp==NULL) {
    perror(strerror(errno));
    printf("%s - key file does not exist\n", filename);
    exit(1);
  }
  
  RSA *rsa = RSA_new();
  if(pub) {
    rsa = PEM_read_RSA_PUBKEY(fp, &rsa, NULL, NULL);
    if(rsa == NULL) {
      perror("failure loading public key\n");
      exit(1);
    }
    this->public_key = rsa;
    this->public_key_size = RSA_size(rsa);
    return this->public_key;
  }
  else {
    rsa = PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);
    if (rsa == NULL) {
      printf("privateKey could not be read\n");
      exit(1);
    }
    this->private_key = rsa;
    this->private_key_size = RSA_size(rsa);
    return this->private_key;
  }
  return NULL;
}

int Crypto::encrypt(unsigned char *data, int data_len, unsigned char*& encrypted_data){
  int rval;
  int len, encrypted_length=0;
  unsigned char *encrypted = NULL;

  while(data_len!=0){
    len = data_len > this->max_message_length ? this->max_message_length : data_len;
    unsigned char *block_encrypted = (unsigned char*)malloc(this->public_key_size);
    if(block_encrypted == NULL) {
      printf("Failed to allocate memory\n");
      exit(1);
    }
    rval = RSA_public_encrypt(len, data, block_encrypted, this->public_key, padding);
    if(rval==-1){
      printf("Encryption failure: %ld\n",ERR_get_error());
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

  encrypted_data = encrypted;
  return encrypted_length;
}

int Crypto::decrypt(unsigned char *data, int data_len, unsigned char*& decrypted_data, int *unencrypted_length){
  
  int rval;
  int decrypted_length=0;
  unsigned char *block_decrypted = NULL;
  unsigned char *decrypted = NULL;
  unsigned int size = this->private_key_size;
  unsigned int data_length = data_len;
  int data_ptr = 0;
  
  while(data_length>=size){
    block_decrypted = (unsigned char*)malloc(size);
    if(block_decrypted == NULL) {
      std::cout<<"Failed to allocate memory"<<std::endl;
      exit(1);
    }
 
    rval = RSA_private_decrypt(size, (const unsigned char *)data+data_ptr, block_decrypted, this->private_key, padding);

    if(rval==-1){
      printf("Decryption failure: %ld\n",ERR_get_error());
      exit(1);
    }
    data_ptr += size;
    data_length -= size;
    
    decrypted = (unsigned char *)realloc(decrypted, decrypted_length+rval);
    for(int i=0; i<rval; i++)
      decrypted[i+decrypted_length] = block_decrypted[i];
    decrypted_length += rval;
  }

  decrypted_data = decrypted;
  
  *unencrypted_length = data_length;
  return decrypted_length;
  
}
