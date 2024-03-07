#ifndef _DEC128_H_
#define _DEC128_H_

#include "decimal/basic_decimal.h"

typedef struct dec128_t
{
	int16 precision;
	int16 scale;
	int32 unused;
	decimal128_t x;
} dec128_t;

#endif
