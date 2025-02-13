#include "sds/sds.h"
#include <stdint.h>

uint64_t sdshash(sds data) {
  // algorithm: djb2
  uint64_t hash = 5381;
  for (int i = 0; i < sdslen(data); i++) {
    hash = ((hash << 5) + hash) + data[i];
  }
  return hash;
}
