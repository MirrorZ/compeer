#ifndef _UTIL_H_
#define _UTIL_H_
class logger {
  const char *logfile;
  FILE *fp = NULL;

  public:
  logger(const char *path);
  ~logger();
  void log(const char *line);
};
void print_block(unsigned char* block, int len);

#endif /* _UTIL_H_ */
