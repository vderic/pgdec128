#include "postgres.h"

#include <math.h>

#include "catalog/pg_type.h"
#include "common/shortest_dec.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "port.h" /* for strtof() */
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "pgdec128.h"

#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

PG_MODULE_MAGIC;

static inline void CHECK_DEC128_STATUS(decimal_status_t s) {
	if (s == DEC128_STATUS_SUCCESS) {
		return;
	} else if (s == DEC128_STATUS_DIVIDEDBYZERO) {
		ereport(ERROR,
			(errcode(ERRCODE_DIVISION_BY_ZERO),
				errmsg("division by zero")));

	} else if (s == DEC128_STATUS_OVERFLOW) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows dec128 format")));
	} else if (s == DEC128_STATUS_RESCALEDATALOSS) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("dec128 rescale data loss")));

	} else {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("dec128 internal error")));
	}
}

/*
 * Initialize index options and variables
 */
PGDLLEXPORT void _PG_init(void);
void _PG_init(void) {
}

static inline int32
make_dec128_typmod(int precision, int scale) {
	return ((precision << 16) | (scale & 0x7ff)) + VARHDRSZ;
}

/*
 * Because of the offset, valid numeric typmods are at least VARHDRSZ
 */
static inline bool
is_valid_dec128_typmod(int32 typmod) {
	return typmod >= (int32)VARHDRSZ;
}

/*
 * numeric_typmod_precision() -
 *
 *	Extract the precision from a numeric typmod --- see make_numeric_typmod().
 */
static inline int
dec128_typmod_precision(int32 typmod) {
	return ((typmod - VARHDRSZ) >> 16) & 0xffff;
}

/*
 * numeric_typmod_scale() -
 *
 *	Extract the scale from a numeric typmod --- see make_numeric_typmod().
 *
 *	Note that the scale may be negative, so we must do sign extension when
 *	unpacking it.  We do this using the bit hack (x^1024)-1024, which sign
 *	extends an 11-bit two's complement number x.
 */
static inline int
dec128_typmod_scale(int32 typmod) {
	return (((typmod - VARHDRSZ) & 0x7ff) ^ 1024) - 1024;
}

/*
 * dec128_accumstate dummy functions
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_accumstate_in);
Datum dec128_accumstate_in(PG_FUNCTION_ARGS) {
	char *lit = PG_GETARG_CSTRING(0);
	dec128_accumstate_t *accum = (dec128_accumstate_t *)palloc0(sizeof(dec128_accumstate_t));
	PG_RETURN_POINTER(accum);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_accumstate_out);
Datum dec128_accumstate_out(PG_FUNCTION_ARGS) {
	PG_RETURN_POINTER(NULL);
}

/*
 * Convert textual representation to internal representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_in);
Datum dec128_in(PG_FUNCTION_ARGS) {
	char *lit = PG_GETARG_CSTRING(0);
	int32 typmod = PG_GETARG_INT32(2);
	int precision, scale;
	int tgt_precision, tgt_scale;
	decimal_status_t s;

	dec128_t *dec = (dec128_t *)palloc(sizeof(dec128_t));
	;
	s = dec128_from_string(lit, &dec->x, &precision, &scale);
	if (s != DEC128_STATUS_SUCCESS) {
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("dec128 conversion failure")));
	}

	dec->precision = precision;
	dec->scale = scale;

	if (precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value is larger precision %d than max precision %d", precision, DEC128_MAX_PRECISION)));
	}

	if (!is_valid_dec128_typmod(typmod)) {
		PG_RETURN_POINTER(dec);
	}

	tgt_precision = dec128_typmod_precision(typmod);
	tgt_scale = dec128_typmod_scale(typmod);

	if (tgt_precision < precision) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value has larger precision %d than target precision %d", precision, tgt_precision)));
	}

	if (tgt_scale < scale) {
		int delta = scale - tgt_scale;
		dec->x = dec128_reduce_scale_by(dec->x, delta, true);
		dec->scale -= delta;
		dec->precision -= delta;
	} else {
		dec->scale = (int16)tgt_scale;
		dec->precision = (int16)tgt_precision;
	}

	PG_RETURN_POINTER(dec);
}

/*
 * Convert internal representation to textual representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_out);
Datum dec128_out(PG_FUNCTION_ARGS) {
	dec128_t *dec = (dec128_t *)PG_GETARG_POINTER(0);
	char output[DEC128_MAX_STRLEN];
	char *res = 0;
	*output = 0;
	dec128_to_string(dec->x, output, dec->scale);
	res = pstrdup(output);
	PG_RETURN_CSTRING(res);
}

/*
 * Convert type modifier
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_typmod_in);
Datum dec128_typmod_in(PG_FUNCTION_ARGS) {
	ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);
	int32 *tl;
	int n;
	int precision, scale, typmod;

	tl = ArrayGetIntegerTypmods(ta, &n);

	if (n == 2) {

		precision = tl[0];
		scale = tl[1];

		if (precision < 1 || precision > DEC128_MAX_PRECISION) {
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("precision must be between 1 and %d", DEC128_MAX_PRECISION)));
		}

		if (scale < 0 || scale > precision) {
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("scale must be smaller than precision and bigger than 0")));
		}

		typmod = make_dec128_typmod(precision, scale);

	} else if (n == 1) {
		precision = tl[0];
		if (precision < 1 || precision > DEC128_MAX_PRECISION) {
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("precision must be between 1 and %d", DEC128_MAX_PRECISION)));
		}

		typmod = make_dec128_typmod(precision, 0);

	} else {
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid type modifier")));
	}

	PG_RETURN_INT32(typmod);
}

/*
 * Convert external binary representation to internal representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_recv);
Datum dec128_recv(PG_FUNCTION_ARGS) {
	StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
	int32 typmod = PG_GETARG_INT32(2);
	dec128_t *result;
	int32 precision;
	int32 scale;
	uint64_t lo;
	int64_t hi;

	precision = pq_getmsgint(buf, sizeof(int32));
	scale = pq_getmsgint(buf, sizeof(int32));

	result = (dec128_t *)palloc(sizeof(dec128_t));
	lo = (uint64_t)pq_getmsgint64(buf);

	if (precision <= 18) {
		result->x = dec128_from_int64(lo);
	} else {
		hi = pq_getmsgint64(buf);
		result->x = dec128_from_hilo(hi, lo);
	}
	result->precision = precision;
	result->scale = scale;

	if (is_valid_dec128_typmod(typmod)) {
		int tgt_precision = dec128_typmod_precision(typmod);
		int tgt_scale = dec128_typmod_scale(typmod);
		if (precision > tgt_precision) {
			ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("value has larger precision than target precision.")));
		}

		if (scale > tgt_scale) {
			ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("value has larger scale than target scale.")));
		}
	}

	PG_RETURN_POINTER(result);
}

/*
 * Convert internal representation to the external binary representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_send);
Datum dec128_send(PG_FUNCTION_ARGS) {
	dec128_t *dec = (dec128_t *)PG_GETARG_POINTER(0);
	StringInfoData buf;
	uint64_t lo;
	int64_t hi;

	pq_begintypsend(&buf);
	pq_sendint(&buf, dec->precision, sizeof(int32));
	pq_sendint(&buf, dec->scale, sizeof(int32));

	lo = dec128_low_bits(dec->x);
	pq_sendint64(&buf, lo);

	if (dec->precision > 18) {
		hi = dec128_high_bits(dec->x);
		pq_sendint64(&buf, hi);
	}

	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128larger);
Datum dec128larger(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	if (a->scale > b->scale) {
		bb = dec128_increase_scale_by(bb, a->scale - b->scale);
	} else if (b->scale > a->scale) {
		aa = dec128_increase_scale_by(aa, b->scale - a->scale);
	}
	if (dec128_cmpgt(aa, bb)) {
		PG_RETURN_POINTER(a);
	}

	PG_RETURN_POINTER(b);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128smaller);
Datum dec128smaller(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	if (a->scale > b->scale) {
		bb = dec128_increase_scale_by(bb, a->scale - b->scale);
	} else if (b->scale > a->scale) {
		aa = dec128_increase_scale_by(aa, b->scale - a->scale);
	}
	if (dec128_cmplt(aa, bb)) {
		PG_RETURN_POINTER(a);
	}

	PG_RETURN_POINTER(b);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128pl);
Datum dec128pl(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	dec128_ADD_SUB_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
	if (res->precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", res->precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}
	if (res->scale > a->scale) {
		aa = dec128_increase_scale_by(a->x, res->scale - a->scale);
	}
	if (res->scale > b->scale) {
		bb = dec128_increase_scale_by(b->x, res->scale - b->scale);
	}
	res->x = dec128_sum(aa, bb);
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128mi);
Datum dec128mi(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	dec128_ADD_SUB_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
	if (res->precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", res->precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}
	if (res->scale > a->scale) {
		aa = dec128_increase_scale_by(a->x, res->scale - a->scale);
	}
	if (res->scale > b->scale) {
		bb = dec128_increase_scale_by(b->x, res->scale - b->scale);
	}
	res->x = dec128_subtract(aa, bb);
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128mul);
Datum dec128mul(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	dec128_MUL_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
	if (res->precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", res->precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}
	res->x = dec128_multiply(a->x, b->x);
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128div);
Datum dec128div(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	decimal128_t zero = {0};
	if (dec128_cmpeq(b->x, zero)) {
		ereport(ERROR,
			(errcode(ERRCODE_DIVISION_BY_ZERO),
				errmsg("division by zero")));

		PG_RETURN_POINTER(NULL);
	}

	dec128_DIV_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
	if (res->precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", res->precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}

	res->x = dec128_divide_exact(a->x, a->scale, b->x, b->scale, res->precision, res->scale);
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128mod);
Datum dec128mod(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	decimal128_t zero = {0};
	if (dec128_cmpeq(b->x, zero)) {
		ereport(ERROR,
			(errcode(ERRCODE_DIVISION_BY_ZERO),
				errmsg("division by zero")));
		PG_RETURN_POINTER(NULL);
	}

	dec128_MOD_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
	if (res->precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", res->precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}

	res->x = dec128_mod(a->x, a->scale, b->x, b->scale);
	PG_RETURN_POINTER(res);
}

#define DEC128CMP(OP)                                                 \
	{                                                                 \
		dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);               \
		dec128_t *b = (dec128_t *)PG_GETARG_POINTER(1);               \
		decimal128_t aa = a->x;                                       \
		decimal128_t bb = b->x;                                       \
		if (a->scale > b->scale) {                                    \
			bb = dec128_increase_scale_by(b->x, a->scale - b->scale); \
		} else if (b->scale > a->scale) {                             \
			aa = dec128_increase_scale_by(a->x, b->scale - a->scale); \
		}                                                             \
		PG_RETURN_BOOL(OP(aa, bb));                                   \
	}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128eq);
Datum dec128eq(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmpeq);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128ne);
Datum dec128ne(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmpne);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128lt);
Datum dec128lt(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmplt);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128gt);
Datum dec128gt(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmpgt);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128ge);
Datum dec128ge(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmpge);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128le);
Datum dec128le(PG_FUNCTION_ARGS) {
	DEC128CMP(dec128_cmple);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_accum);
Datum dec128_accum(PG_FUNCTION_ARGS) {
	dec128_accumstate_t *accum = (dec128_accumstate_t *)PG_GETARG_POINTER(0);
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(1);
	int precision, scale;
	decimal128_t aa, sumx;

	if (accum == NULL) {
		accum = (dec128_accumstate_t *)palloc0(sizeof(dec128_accumstate_t));
	}

	if (accum->count == 0) {
		accum->count = 1;
		accum->sumx = *a;
		PG_RETURN_POINTER(accum);
	}

	aa = a->x;
	sumx = accum->sumx.x;

	dec128_ADD_SUB_precision_scale(accum->sumx.precision, accum->sumx.scale,
		a->precision, a->scale, &precision, &scale);
	if (scale > a->scale) {
		aa = dec128_increase_scale_by(a->x, scale - a->scale);
	}
	if (scale > accum->sumx.scale) {
		sumx = dec128_increase_scale_by(sumx, scale - accum->sumx.scale);
	}
	accum->sumx.x = dec128_sum(sumx, aa);
	accum->sumx.precision = precision;
	accum->sumx.scale = scale;
	accum->count++;

#if 0
	{
		char output[DEC128_MAX_STRLEN];
		dec128_to_string(accum->sumx.x, output, accum->sumx.scale);
		elog(LOG, "accum: count: %ld, sumx=%s", accum->count, output);
	}
#endif
	PG_RETURN_POINTER(accum);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_combine);
Datum dec128_combine(PG_FUNCTION_ARGS) {
	dec128_accumstate_t *accum1 = (dec128_accumstate_t *)PG_GETARG_POINTER(0);
	dec128_accumstate_t *accum2 = (dec128_accumstate_t *)PG_GETARG_POINTER(1);
	dec128_accumstate_t *accum = (dec128_accumstate_t *)palloc(sizeof(dec128_accumstate_t));
	decimal128_t sumx1, sumx2;
	int precision, scale;

	sumx1 = accum1->sumx.x;
	sumx2 = accum2->sumx.x;

	dec128_ADD_SUB_precision_scale(accum1->sumx.precision, accum1->sumx.scale,
		accum2->sumx.precision, accum2->sumx.scale, &precision, &scale);
	if (scale > accum1->sumx.scale) {
		sumx1 = dec128_increase_scale_by(sumx1, scale - accum1->sumx.scale);
	}
	if (scale > accum2->sumx.scale) {
		sumx2 = dec128_increase_scale_by(sumx2, scale - accum2->sumx.scale);
	}
	accum->sumx.x = dec128_sum(sumx1, sumx2);
	accum->sumx.precision = precision;
	accum->sumx.scale = scale;
	accum->count = accum1->count + accum2->count;

	PG_RETURN_POINTER(accum);
}

static int calc_precision(decimal128_t value) {
	char output[DEC128_MAX_STRLEN];
	if (dec128_cmpeq(value, dec128_from_int64(0))) {
		return 0;
	}

	dec128_to_integer_string(value, output);
	return strlen(output);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_avg);
Datum dec128_avg(PG_FUNCTION_ARGS) {
	dec128_accumstate_t *accum = (dec128_accumstate_t *)PG_GETARG_POINTER(0);
	int precision, scale;
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));
	dec128_t N;

	N.x = dec128_from_int64(accum->count);
	N.scale = 0;
	N.precision = calc_precision(N.x);

	dec128_DIV_precision_scale(accum->sumx.precision, accum->sumx.scale, N.precision, N.scale, &precision, &scale);
	if (precision > DEC128_MAX_PRECISION) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				errmsg("value overflows in dec128 format. precision %d > %d", precision, DEC128_MAX_PRECISION)));
		PG_RETURN_POINTER(NULL);
	}
	res->x = dec128_divide_exact(accum->sumx.x, accum->sumx.scale, N.x, N.scale, precision, scale);
	res->precision = precision;
	res->scale = scale;
	PG_RETURN_POINTER(res);
}

/* 
 * Cast functions
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_int64);
Datum dec128_cast_int64(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	//int32 typmod = PG_GETARG_INT32(1);
	int64 res = 0;
	res = dec128_to_int64(a->x);
	PG_RETURN_INT64(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_float);
Datum dec128_cast_float(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	int32 typmod = PG_GETARG_INT32(1);
	float res = 0;
	res = dec128_to_float(a->x, a->scale);
	PG_RETURN_FLOAT4(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_double);
Datum dec128_cast_double(PG_FUNCTION_ARGS) {
	dec128_t *a = (dec128_t *)PG_GETARG_POINTER(0);
	int32 typmod = PG_GETARG_INT32(1);
	double res = dec128_to_double(a->x, a->scale);
	PG_RETURN_FLOAT8(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_from_float);
Datum dec128_cast_from_float(PG_FUNCTION_ARGS) {
	float a = PG_GETARG_FLOAT4(0);
	int32 typmod = PG_GETARG_INT32(1);
	dec128_t *res = (dec128_t *)palloc0(sizeof(dec128_t));
	int precision, scale;
	decimal_status_t s;

	if (!is_valid_dec128_typmod(typmod)) {
		/* max precision for float is 7 and scale 3 */
		s = dec128_from_float(a, &res->x, 7, 3);
		CHECK_DEC128_STATUS(s);
		res->precision = 7;
		res->scale = 3;
		PG_RETURN_POINTER(res);
	}

	precision = dec128_typmod_precision(typmod);
	scale = dec128_typmod_scale(typmod);
	s = dec128_from_double(a, &res->x, precision, scale);
	CHECK_DEC128_STATUS(s);

	res->precision = precision;
	res->scale = scale;
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_from_double);
Datum dec128_cast_from_double(PG_FUNCTION_ARGS) {
	double a = PG_GETARG_FLOAT8(0);
	int32 typmod = PG_GETARG_INT32(1);
	dec128_t *res = (dec128_t *)palloc0(sizeof(dec128_t));
	int precision, scale;
	decimal_status_t s;

	if (!is_valid_dec128_typmod(typmod)) {
		/* max precision for float is 16 and scale 5 */
		s = dec128_from_double(a, &res->x, 16, 5);
		CHECK_DEC128_STATUS(s);
		res->precision = 16;
		res->scale = 5;
		PG_RETURN_POINTER(res);
	}

	precision = dec128_typmod_precision(typmod);
	scale = dec128_typmod_scale(typmod);
	s = dec128_from_double(a, &res->x, precision, scale);
	CHECK_DEC128_STATUS(s);
	res->precision = precision;
	res->scale = scale;
	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_from_int32);
Datum dec128_cast_from_int32(PG_FUNCTION_ARGS) {
	int32 a = PG_GETARG_INT32(0);
	int32 typmod = PG_GETARG_INT32(1);
	dec128_t *res = (dec128_t *)palloc0(sizeof(dec128_t));
	res->x = dec128_from_int64(a);
	res->precision = calc_precision(res->x);
	res->scale = 0;
	if (is_valid_dec128_typmod(typmod)) {
		int scale = dec128_typmod_scale(typmod);
		if (scale < 0) {
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("scale must be positive integer")));
			PG_RETURN_POINTER(NULL);
		}

		if (res->precision + scale > DEC128_MAX_PRECISION) {
			ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("value overflows dec128 format")));
			PG_RETURN_POINTER(NULL);
		}

		res->x = dec128_increase_scale_by(res->x, scale);
		res->scale = scale;
		res->precision += scale;
	}

	PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_from_int64);
Datum dec128_cast_from_int64(PG_FUNCTION_ARGS) {
	int64 a = PG_GETARG_INT64(0);
	int32 typmod = PG_GETARG_INT32(1);
	dec128_t *res = (dec128_t *)palloc0(sizeof(dec128_t));
	res->x = dec128_from_int64(a);
	res->precision = calc_precision(res->x);
	res->scale = 0;
	if (is_valid_dec128_typmod(typmod)) {
		int scale = dec128_typmod_scale(typmod);
		if (scale < 0) {
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("scale must be positive integer")));
			PG_RETURN_POINTER(NULL);
		}

		if (res->precision + scale > DEC128_MAX_PRECISION) {
			ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("value overflows dec128 format")));
			PG_RETURN_POINTER(NULL);
		}

		res->x = dec128_increase_scale_by(res->x, scale);
		res->scale = scale;
		res->precision += scale;
	}
	PG_RETURN_POINTER(res);
}


PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_cast_from_numeric);
Datum dec128_cast_from_numeric(PG_FUNCTION_ARGS) {
	Numeric num = PG_GETARG_NUMERIC(0);
	int32 typmod = PG_GETARG_INT32(1);
	char *str;
	int precision, scale;
	decimal_status_t s;
	dec128_t *res = (dec128_t *)palloc(sizeof(dec128_t));

	str = numeric_normalize(num);
	s = dec128_from_string(str, &res->x, &res->precision, &res->scale);
	CHECK_DEC128_STATUS(s);
	PG_RETURN_POINTER(res);
}

