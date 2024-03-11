/* contrib/postgres_fdw/postgres_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgdec128" to load this file. \quit

-- type

CREATE TYPE dec128;
CREATE TYPE dec128_accumstate;

CREATE FUNCTION dec128_in(cstring, oid, integer) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_out(dec128) RETURNS cstring
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_typmod_in(cstring[]) RETURNS integer
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_recv(internal, oid, integer) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_send(dec128) RETURNS bytea
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE dec128 (
	INTERNALLENGTH = 24,
	INPUT     = dec128_in,
	OUTPUT    = dec128_out,
	TYPMOD_IN = dec128_typmod_in,
	RECEIVE   = dec128_recv,
	SEND      = dec128_send,
	ALIGNMENT = int4
);

CREATE FUNCTION dec128_accumstate_in(cstring, oid, integer) RETURNS dec128_accumstate
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_accumstate_out(dec128_accumstate) RETURNS cstring
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE dec128_accumstate (
        INTERNALLENGTH = 32,
        INPUT     = dec128_accumstate_in,
        OUTPUT    = dec128_accumstate_out,
        ALIGNMENT = int4
);

CREATE FUNCTION dec128um(dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128up(dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128larger(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128smaller(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128pl(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128mi(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128mul(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128div(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128mod(dec128, dec128) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128eq(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128ne(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128lt(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128gt(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128le(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128ge(dec128, dec128) RETURNS bool
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128and(dec128, dec128) RETURNS dec128
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128or(dec128, dec128) RETURNS dec128
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128shl(dec128, int) RETURNS dec128
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128shr(dec128, int) RETURNS dec128
        AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_accum(dec128_accumstate, dec128) RETURNS dec128_accumstate
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_avg(dec128_accumstate) RETURNS dec128
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_combine(dec128_accumstate, dec128_accumstate) RETURNS dec128_accumstate
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- operators
CREATE OPERATOR + (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128pl,
	COMMUTATOR = +
);

CREATE OPERATOR + (
	RIGHTARG = dec128, PROCEDURE = dec128up
);

CREATE OPERATOR - (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128mi
);

CREATE OPERATOR - (
	RIGHTARG = dec128, PROCEDURE = dec128um
);

CREATE OPERATOR * (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128mul,
	COMMUTATOR = *
);

CREATE OPERATOR / (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128div
);

CREATE OPERATOR % (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128mod
);

CREATE OPERATOR = (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128eq,
        COMMUTATOR = = , NEGATOR = <> ,
	RESTRICT = eqsel, JOIN = eqjoinsel
);

CREATE OPERATOR < (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128lt,
        COMMUTATOR = > , NEGATOR = >= ,
        RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128le,
        COMMUTATOR = >= , NEGATOR = > ,
        RESTRICT = scalarlesel, JOIN = scalarlejoinsel
);

CREATE OPERATOR <> (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128ne,
        COMMUTATOR = <> , NEGATOR = = ,
        RESTRICT = eqsel, JOIN = eqjoinsel
);

CREATE OPERATOR >= (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128ge,
        COMMUTATOR = <= , NEGATOR = < ,
        RESTRICT = scalargesel, JOIN = scalargejoinsel
);

CREATE OPERATOR > (
        LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128gt,
        COMMUTATOR = < , NEGATOR = <= ,
        RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);

CREATE OPERATOR & (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128and,
	COMMUTATOR = &
);

CREATE OPERATOR | (
	LEFTARG = dec128, RIGHTARG = dec128, PROCEDURE = dec128or,
	COMMUTATOR = |
);

CREATE OPERATOR >> (
	LEFTARG = dec128, RIGHTARG = int, PROCEDURE = dec128shr
);

CREATE OPERATOR << (
	LEFTARG = dec128, RIGHTARG = int, PROCEDURE = dec128shl
);

-- aggregates
CREATE AGGREGATE min(dec128) (
	SFUNC = dec128smaller,
	STYPE = dec128,
	COMBINEFUNC = dec128smaller,
	PARALLEL = SAFE
);

CREATE AGGREGATE max(dec128) (
	SFUNC = dec128larger,
	STYPE = dec128,
	COMBINEFUNC = dec128larger,
	PARALLEL = SAFE
);

CREATE AGGREGATE sum(dec128) (
	SFUNC = dec128pl,
	STYPE = dec128,
	COMBINEFUNC = dec128pl,
	PARALLEL = SAFE
);

CREATE AGGREGATE avg(dec128) (
	SFUNC = dec128_accum,
	STYPE = dec128_accumstate,
	FINALFUNC = dec128_avg,
	COMBINEFUNC = dec128_combine,
	INITCOND= '{}',
	PARALLEL = SAFE
);


-- cast functions

CREATE FUNCTION dec128_cast_int64(dec128, integer, boolean) RETURNS bigint
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_float(dec128, integer, boolean) RETURNS real
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_double(dec128, integer, boolean) RETURNS double precision
	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_from_float(real, integer, boolean) RETURNS dec128
 	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_from_double(double precision, integer, boolean) RETURNS dec128
 	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_from_int32(integer, integer, boolean) RETURNS dec128
 	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_from_int64(bigint, integer, boolean) RETURNS dec128
 	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION dec128_cast_from_numeric(numeric, integer, boolean) RETURNS dec128
 	AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- casts

CREATE CAST (dec128 AS bigint)
	WITH FUNCTION dec128_cast_int64(dec128, integer, boolean) AS IMPLICIT;

CREATE CAST (dec128 AS real)
	WITH FUNCTION dec128_cast_float(dec128, integer, boolean) AS IMPLICIT;

CREATE CAST (dec128 AS double precision)
	WITH FUNCTION dec128_cast_double(dec128, integer, boolean) AS IMPLICIT;

CREATE CAST (real AS dec128)
	WITH FUNCTION dec128_cast_from_float(real, integer, boolean) AS ASSIGNMENT;

CREATE CAST (double precision AS dec128)
	WITH FUNCTION dec128_cast_from_double(double precision, integer, boolean) AS ASSIGNMENT;

CREATE CAST (int AS dec128)
	WITH FUNCTION dec128_cast_from_int32(int, integer, boolean) AS ASSIGNMENT;

CREATE CAST (bigint AS dec128)
	WITH FUNCTION dec128_cast_from_int64(bigint, integer, boolean) AS ASSIGNMENT;

CREATE CAST (numeric AS dec128)
	WITH FUNCTION dec128_cast_from_numeric(numeric, integer, boolean) AS ASSIGNMENT;
