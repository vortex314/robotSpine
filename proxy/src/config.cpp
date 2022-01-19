#include "config.h"
#include "errno.h"
#include <string.h>

string loadFile(const char *name) {

  std::string str = "{}";

  FILE *file = fopen(name, "r");
  if (file != NULL) {
    str = "";
    char buffer[256];
    while (true) {
      int result = fread(buffer, 1, 256, file);
      if (result <= 0)
        break;
      str.append(buffer, result);
    }
    fclose(file);
  } else {
    WARN(" cannot open %s : %d = %s", name, errno, strerror(errno));
  }
  return str;
}

