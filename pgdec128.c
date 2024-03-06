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
#include "decimal/basic_decimal.h"

#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

#if PG_VERSION_NUM < 130000
#define TYPALIGN_DOUBLE 'd'
#define TYPALIGN_INT 'i'
#endif

#define STATE_DIMS(x) (ARR_DIMS(x)[0] - 1)
#define CreateStateDatums(dim) palloc(sizeof(Datum) * (dim + 1))

PG_MODULE_MAGIC;

/*
 * Initialize index options and variables
 */
PGDLLEXPORT void _PG_init(void);
void
_PG_init(void)
{
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

	PG_RETURN_POINTER(NULL);
}

/*
 * Convert internal representation to textual representation
 */
PGDLLEXPORT PG_FUNCTION_INFO_V1(dec128_out);
Datum
dec128_out(PG_FUNCTION_ARGS)
{
	decimal128_t   *dec = (decimal128_t *) PG_GETARG_POINTER(0);

	PG_RETURN_CSTRING("1.23");
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

	tl = ArrayGetIntegerTypmods(ta, &n);

	if (n != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid type modifier")));

	if (*tl < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("dimensions for type vector must be at least 1")));

#if 0
	if (*tl > VECTOR_MAX_DIM)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("dimensions for type vector cannot exceed %d", VECTOR_MAX_DIM)));
#endif

	PG_RETURN_INT32(*tl);
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
	decimal128_t	   *result;
	int16		dim;
	int16		unused;

	dim = pq_getmsgint(buf, sizeof(int16));
	unused = pq_getmsgint(buf, sizeof(int16));

	//CheckDim(dim);
	//CheckExpectedDim(typmod, dim);

	if (unused != 0)
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("expected unused to be 0, not %d", unused)));

	result = 0;
	//result = InitVector(dim);
	for (int i = 0; i < dim; i++)
	{
		//result->x[i] = pq_getmsgfloat4(buf);
		//CheckElement(result->x[i]);
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
	decimal128_t	   *dec = (decimal128_t *) PG_GETARG_POINTER(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	/*
	pq_sendint(&buf, vec->dim, sizeof(int16));
	pq_sendint(&buf, vec->unused, sizeof(int16));
	for (int i = 0; i < vec->dim; i++)
		pq_sendfloat4(&buf, vec->x[i]);
	*/

	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

