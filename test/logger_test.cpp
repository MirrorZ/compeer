#include <stdio.h>
#include "../util.h"

int main(){
  logger mylog("test.log");
  mylog.log("This is a test.\n");
  mylog.log("Test completed.\n");

  return 0;
}
