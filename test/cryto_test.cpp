#include <stdio.h>
#include "crypto.h"

int main(int argc, char *argv[]) {
  Crypto c;
  c.set_public_key(NULL);
  
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
  unsigned char *decrypted = NULL;
  
  elen -=2;
  int dlen = c.decrypt(encrypted, &elen, decrypted);
  printf("Len remaining %d\n", elen);
  decrypted[dlen]='\0';
  printf("Decrypted data [%d]: %s\n", dlen, decrypted);
  dlen = c.decrypt(encrypted, &elen, decrypted);
  decrypted[dlen]='\0';
  printf("Decrypted data [%d]: %s\n", dlen, decrypted);
  
  return 0;
}
