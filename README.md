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
CREATE EXTENSION
create temp table t (a dec128(20, 2), b dec128(10,4));
CREATE TABLE
insert into t values ('12.30','345.6789'), ('2.55','2.1234'), ('123456789012345678.59','3.45'), ('-12345678901234567.33', '1.3'), 
('10.03', '10.0300'), (NULL, '3.45'), (NULL, NULL), ('2.34', NULL);
INSERT 0 8
select sum(a) as sum, avg(a) as avg, min(a) as min, max(a) as max from t;
          sum          |          avg           |          min          |          max          
-----------------------+------------------------+-----------------------+-----------------------
 111111110111111138.48 | 18518518351851856.4133 | -12345678901234567.33 | 123456789012345678.59
(1 row)

select a, b, a+b as plus, a-b as minus, a*b as mul, a/b as div, a%b as mod from t;
           a           |    b     |         plus          |         minus         |           mul           |          div           |   mod   
-----------------------+----------+-----------------------+-----------------------+-------------------------+------------------------+---------
 12.30                 | 345.6789 | 357.9789              | -333.3789             | 4251.850470             | 0.035582               | 12.3000
 2.55                  | 2.1234   | 4.6734                | 0.4266                | 5.414670                | 1.2009                 | 0.4266
 123456789012345678.59 | 3.45     | 123456789012345682.04 | 123456789012345675.14 | 425925922092592591.1355 | 35784576525317587.9971 | 3.44
 -12345678901234567.33 | 1.3      | -12345678901234566.03 | -12345678901234568.63 | -16049382571604937.529  | -9496676077872744.1000 | -0.13
 10.03                 | 10.0300  | 20.0600               | 0.0000                | 100.600900              | 1.00000                | 0.0000
                       | 3.45     |                       |                       |                         |                        | 
                       |          |                       |                       |                         |                        | 
 2.34                  |          |                       |                       |                         |                        | 
(8 rows)

select a, b, a=b as eq, a<>b as ne, a>b as gt, a<b as lt, a>=b as ge, a<=b as le from t;
           a           |    b     | eq | ne | gt | lt | ge | le 
-----------------------+----------+----+----+----+----+----+----
 12.30                 | 345.6789 | f  | t  | f  | t  | f  | t
 2.55                  | 2.1234   | f  | t  | t  | f  | t  | f
 123456789012345678.59 | 3.45     | f  | t  | t  | f  | t  | f
 -12345678901234567.33 | 1.3      | f  | t  | f  | t  | f  | t
 10.03                 | 10.0300  | t  | f  | f  | f  | t  | t
                       | 3.45     |    |    |    |    |    | 
                       |          |    |    |    |    |    | 
 2.34                  |          |    |    |    |    |    | 
(8 rows)

select cast('12'::int as dec128(10,2));
 dec128 
--------
 12.00
(1 row)

select cast('12'::bigint as dec128(10,2));
 dec128 
--------
 12.00
(1 row)

select cast('12.23'::real as dec128(10,4));
 dec128  
---------
 12.2300
(1 row)

select cast('12.2334'::double precision as dec128(10,7));
   dec128   
------------
 12.2334000
(1 row)

copy t to '/tmp/test.bin' (format binary);
COPY 8
drop table t;
DROP TABLE
create temp table t (a dec128(20, 2), b dec128(10,4));
CREATE TABLE
copy t from '/tmp/test.bin' (format binary);
COPY 8
select * from t;
           a           |    b     
-----------------------+----------
 12.30                 | 345.6789
 2.55                  | 2.1234
 123456789012345678.59 | 3.45
 -12345678901234567.33 | 1.3
 10.03                 | 10.0300
                       | 3.45
                       | 
 2.34                  | 
(8 rows)

drop table t;
DROP TABLE
create temp table err (a dec128(20, 2), b dec128(10,4));
CREATE TABLE
insert into err values ('12.30','0');
INSERT 0 1
select a, b, a/b as div from err;
psql:test.sql:34: ERROR:  division by zero
select a, b, a%b as mod from err;
psql:test.sql:35: ERROR:  division by zero
drop table err;
DROP TABLE
drop extension pgdec128 cascade;
DROP EXTENSION
```

