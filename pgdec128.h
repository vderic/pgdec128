#ifndef _DEC128_H_
#define _DEC128_H_

#include "decimal/basic_decimal.h"

typedef struct dec128_t
{
	int32 vl_len_;     /* varlena header (do not touch directly!) */
	int16 precision;
	int16 scale;
	decimal128_t x;
} dec128_t;

#endif
