void print_block(unsigned char* block, int len) {
  printf("Block [ len = %d]\n", len);

  --len;
  for(int i = 0; i < len; i++)
    printf("%02x,", block[i]);
  printf("%02x\n", block[len]);
}
