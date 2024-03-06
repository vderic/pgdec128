/* contrib/postgres_fdw/postgres_fdw--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgdec128" to load this file. \quit

-- type

CREATE TYPE dec128;

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
	INPUT     = dec128_in,
	OUTPUT    = dec128_out,
	TYPMOD_IN = dec128_typmod_in,
	RECEIVE   = dec128_recv,
	SEND      = dec128_send,
	STORAGE   = external
);

