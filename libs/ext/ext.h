#ifndef EXT_H
#define EXT_H
#include "sds/sds.h"
#include <stdint.h>
uint64_t sdshash(sds data);
#endif