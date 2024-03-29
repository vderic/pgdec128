create extension if not exists pgdec128;

create temp table t (a dec128(20, 2), b dec128(10,4));

insert into t values ('12.30','345.6789'), ('2.55','2.1234'), ('123456789012345678.59','3.45'), ('-12345678901234567.33', '1.3'), 
('10.03', '10.0300'), (NULL, '3.45'), (NULL, NULL), ('2.34', NULL);

select sum(a) as sum, avg(a) as avg, min(a) as min, max(a) as max from t;

select a, b, -a as um, +a as up, a+b as plus, a-b as minus, a*b as mul, a/b as div, a%b as mod from t;

select a, b, a=b as eq, a<>b as ne, a>b as gt, a<b as lt, a>=b as ge, a<=b as le from t;

select cast('12'::int as dec128(10,2));
select cast('12'::bigint as dec128(10,2));
select cast('12.23'::real as dec128(10,4));
select cast('12.2334'::double precision as dec128(10,7));
select cast(12.23456 as dec128(10, 5));
select cast(12.23456 as dec128(6, 5));
select cast(12.23456 as dec128(10, 2));
select 128::dec128(5) & 18::dec128(5);
select 128::dec128(5) | 18::dec128(5);
select 128::dec128(5) >> 1;
select 128::dec128(5) << 2;
select ceil(128.1234::dec128(8,4));
select floor(128.1234::dec128(8,4));
select abs(-128.1234::dec128(8,4));
select round(-128.1234::dec128(8,4), 2);

copy t to '/tmp/test.bin' (format binary);

drop table t;

create temp table t (a dec128(20, 2), b dec128(10,4));

copy t from '/tmp/test.bin' (format binary);

select * from t;

drop table t;

create temp table err (a dec128(20, 2), b dec128(10,4));

insert into err values ('12.30','0');
select a, b, a/b as div from err;
select a, b, a%b as mod from err;

drop table err;

create temp table err (a dec128(20, 2), b dec128(20,4));
insert into err values ('123456789012345678.59','12345678901234567.45');
select a, b, a*b from err;
drop table err;

create temp table t (a dec128(20, 2), b dec128(10,4));

insert into t values (12.30,345.6789), (2.55,2.1234), (123456789012345678.59,3.45), (-12345678901234567.33, 1.3),
(10.03, 10.0300), (NULL, 3.45), (NULL, NULL), (2.34, NULL);

select sum(a) as sum, avg(a) as avg, min(a) as min, max(a) as max from t;

select a, b, -a as um, +a as up, a+b as plus, a-b as minus, a*b as mul, a/b as div, a%b as mod from t;

select a, b, a=b as eq, a<>b as ne, a>b as gt, a<b as lt, a>=b as ge, a<=b as le from t;

drop table t;


drop extension pgdec128 cascade;
