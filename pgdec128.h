#ifndef _DEC128_H_
#define _DEC128_H_

#include "decimal/basic_decimal.h"

typedef struct dec128_t {
	int32 precision;
	int32 scale;
	decimal128_t x;
} dec128_t;

typedef struct dec128_accumstate_t {
	dec128_t sumx;
	int64_t count;
} dec128_accumstate_t;

#endif
