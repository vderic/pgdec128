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

