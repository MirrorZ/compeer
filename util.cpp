#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"


logger::logger(const char *path){
  this->logfile = path;
  this->fp = fopen(path, "a+");
}

logger::~logger(){
  fclose(this->fp);
}

void logger::log(const char *line) {
  fwrite(line, sizeof(char), strlen(line), this->fp);
}

void print_block(unsigned char* block, int len) {
  fprintf(stderr, "Block [ len = %d]\n", len);

  --len;
  for(int i = 0; i < len; i++)
    fprintf(stderr,"%02x,", block[i]);
  fprintf(stderr, "%02x\n", block[len]);
}
