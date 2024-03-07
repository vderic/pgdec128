#include "postgres.h"


#include <math.h>

#include "catalog/pg_type.h"
#include "common/shortest_dec.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "port.h"				/* for strtof() */
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "pgdec128.h"

#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

#if PG_VERSION_NUM < 130000
#define TYPALIGN_DOUBLE 'd'
#define TYPALIGN_INT 'i'
#endif

PG_MODULE_MAGIC;

/*
 * Initialize index options and variables
 */
PGDLLEXPORT void _PG_init(void);
void
_PG_init(void)
{
}

static inline int32
make_dec128_typmod(int precision, int scale)
{
	return ((precision << 16) | (scale & 0x7ff)) + VARHDRSZ;
}

/*
 * Because of the offset, valid numeric typmods are at least VARHDRSZ
 */
static inline bool
is_valid_dec128_typmod(int32 typmod)
{
	return typmod >= (int32) VARHDRSZ;
}

/*
 * numeric_typmod_precision() -
 *
 *	Extract the precision from a numeric typmod --- see make_numeric_typmod().
 */
static inline int
dec128_typmod_precision(int32 typmod)
{
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
dec128_typmod_scale(int32 typmod)
{
	return (((typmod - VARHDRSZ) & 0x7ff) ^ 1024) - 1024;
}

/*
 * Convert textual representation to internal representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_in);
Datum
dec128_in(PG_FUNCTION_ARGS)
{
	char	*lit = PG_GETARG_CSTRING(0);
	int32	typmod = PG_GETARG_INT32(2);
	int precision, scale;
	int tgt_precision, tgt_scale;
	decimal_status_t s;

	dec128_t *dec = (dec128_t *) palloc(sizeof(dec128_t));;
	s = dec128_from_string(lit, &dec->x, &precision, &scale);
	if (s != DEC128_STATUS_SUCCESS) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("dec128 conversion failure")));
	}

	dec->precision = precision;
	dec->scale = scale;

	if (precision > 38) {
		ereport(ERROR,
			(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
			 errmsg("value is larger precision than max precision 38")));
	}

	if (! is_valid_dec128_typmod(typmod)) {
		PG_RETURN_POINTER(dec);
	}

	tgt_precision = dec128_typmod_precision(typmod);
	tgt_scale = dec128_typmod_scale(typmod);

	if (tgt_precision < precision) {
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("value has larger precision than target precision %d", tgt_precision)));
	}

	if (tgt_scale < scale) {
		int delta = scale - tgt_scale;
		dec->x = dec128_reduce_scale_by(dec->x, delta, true);
		dec->scale -= delta;
		dec->precision -= delta;
	} else {
		dec->scale = (int16) tgt_scale;
		dec->precision = (int16) tgt_precision;
	}

	PG_RETURN_POINTER(dec);
}

/*
 * Convert internal representation to textual representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_out);
Datum
dec128_out(PG_FUNCTION_ARGS)
{
	dec128_t   *dec = (dec128_t *) PG_GETARG_POINTER(0);
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
Datum
dec128_typmod_in(PG_FUNCTION_ARGS)
{
	ArrayType  *ta = PG_GETARG_ARRAYTYPE_P(0);
	int32	   *tl;
	int			n;
	int precision, scale, typmod;

	tl = ArrayGetIntegerTypmods(ta, &n);

	if (n != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid type modifier")));

	precision = tl[0];
	scale = tl[1];

	if (precision < 0 || precision > 38) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("precision must be between 0 and 38")));
	}

	if (scale < 0 || scale > precision) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("scale must be smaller than precision and bigger than 0")));
	}

	typmod = make_dec128_typmod(precision, scale);

	PG_RETURN_INT32(typmod);
}

/*
 * Convert external binary representation to internal representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_recv);
Datum
dec128_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	int32		typmod = PG_GETARG_INT32(2);
	dec128_t	   *result;
	int32		precision;
	int32		scale;
	uint64_t        lo;
	int64_t         hi;

	precision = pq_getmsgint(buf, sizeof(int32));
	scale = pq_getmsgint(buf, sizeof(int32));

	result = (dec128_t *) palloc(sizeof(dec128_t));
	lo = (uint64_t) pq_getmsgint64(buf);

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
Datum
dec128_send(PG_FUNCTION_ARGS)
{
	dec128_t	   *dec = (dec128_t *) PG_GETARG_POINTER(0);
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


PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128pl);
Datum
dec128pl(PG_FUNCTION_ARGS)
{
	dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
	dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
	dec128_t *res = (dec128_t *) palloc(sizeof(dec128_t));
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	dec128_ADD_SUB_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
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
Datum
dec128mi(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        dec128_t *res = (dec128_t *) palloc(sizeof(dec128_t));
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        dec128_ADD_SUB_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
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
Datum
dec128mul(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        dec128_t *res = (dec128_t *) palloc(sizeof(dec128_t));
        dec128_MUL_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
        res->x = dec128_multiply(a->x, b->x);
        PG_RETURN_POINTER(res);
}


PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128div);
Datum
dec128div(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        dec128_t *res = (dec128_t *) palloc(sizeof(dec128_t));
        dec128_DIV_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
        res->x = dec128_divide_exact(a->x, a->scale, b->x, b->scale, res->precision, res->scale);
        PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128mod);
Datum
dec128mod(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        dec128_t *res = (dec128_t *) palloc(sizeof(dec128_t));
        dec128_MOD_precision_scale(a->precision, a->scale, b->precision, b->scale, &res->precision, &res->scale);
        res->x = dec128_mod(a->x, a->scale, b->x, b->scale);
        PG_RETURN_POINTER(res);
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128eq);
Datum
dec128eq(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
	decimal128_t aa = a->x;
	decimal128_t bb = b->x;

	if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
	PG_RETURN_BOOL(dec128_cmpeq(aa, bb));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128ne);
Datum
dec128ne(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
        PG_RETURN_BOOL(dec128_cmpne(aa, bb));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128lt);
Datum
dec128lt(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
        PG_RETURN_BOOL(dec128_cmplt(aa, bb));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128gt);
Datum
dec128gt(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
        PG_RETURN_BOOL(dec128_cmpgt(aa, bb));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128ge);
Datum
dec128ge(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
        PG_RETURN_BOOL(dec128_cmpge(aa, bb));
}

PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128le);
Datum
dec128le(PG_FUNCTION_ARGS)
{
        dec128_t           *a = (dec128_t *) PG_GETARG_POINTER(0);
        dec128_t           *b = (dec128_t *) PG_GETARG_POINTER(1);
        decimal128_t aa = a->x;
        decimal128_t bb = b->x;

        if (a->scale > b->scale) {
                bb = dec128_increase_scale_by(b->x, a->scale - b->scale);
        } else if (b->scale > a->scale) {
                aa = dec128_increase_scale_by(a->x, b->scale - a->scale);
        }
        PG_RETURN_BOOL(dec128_cmple(aa, bb));
}
