Decimal 128  for PostgreSQL
=========================================

This PostgreSQL extension implements deimcal 128 in C.  The deicimal library is ported from arrow Decimal library to C.

Installation
------------

To compile the pgdec128 extension,

1. To build on POSIX-compliant systems you need to ensure the
   `pg_config` executable is in your path when you run `make`. This
   executable is typically in your PostgreSQL installation's `bin`
   directory. For example:

    ```
    $ export PATH=/usr/local/pgsql/bin/:$PATH
    ```

2. Compile the code using make.

    ```
    $ make USE_PGXS=1
    ```

3.  Finally install the extension

    ```
    $ make USE_PGXS=1 install
    ```

Usage
=====

```sql
create extension if not exists pgdec128;

create temp table t (a dec128(20, 2), b dec128(10,4));

insert into t values ('12.30','345.6789'), ('2.55','2.1234'), ('123456789012345678.59','3.45'), ('-12345678901234567.33', '1.3'), 
('10.03', '10.0300'), (NULL, '3.45'), (NULL, NULL), ('2.34', NULL);

select sum(a) as sum, avg(a) as avg, min(a) as min, max(a) as max from t;

select a, b, a+b as plus, a-b as minus, a*b as mul, a/b as div, a%b as mod from t;

select a, b, a=b as eq, a>b as gt, a<b as lt, a>=b as ge, a<=b as le from t;

select cast('12'::int as dec128(10,2));
select cast('12'::bigint as dec128(10,2));
select cast('12.23'::real as dec128(10,4));
select cast('12.2334'::double precision as dec128(10,7));

drop table t;

create temp table err (a dec128(20, 2), b dec128(10,4));

insert into err values ('12.30','0');
select a, b, a/b as div from err;
select a, b, a%b as mod from err;

drop table err;

drop extension pgdec128 cascade;
```

