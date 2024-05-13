--
-- COPY
--

-- CLASS POPULATION
--	(any resemblance to real life is purely coincidental)
--
COPY aggtest FROM '/postgresql-12.14/src/test/regress/data/agg.data';

COPY onek FROM '/postgresql-12.14/src/test/regress/data/onek.data';

COPY onek TO '/postgresql-12.14/src/test/regress/results/onek.data';

DELETE FROM onek;

COPY onek FROM '/postgresql-12.14/src/test/regress/results/onek.data';

COPY tenk1 FROM '/postgresql-12.14/src/test/regress/data/tenk.data';

COPY slow_emp4000 FROM '/postgresql-12.14/src/test/regress/data/rect.data';

COPY person FROM '/postgresql-12.14/src/test/regress/data/person.data';

COPY emp FROM '/postgresql-12.14/src/test/regress/data/emp.data';

COPY student FROM '/postgresql-12.14/src/test/regress/data/student.data';

COPY stud_emp FROM '/postgresql-12.14/src/test/regress/data/stud_emp.data';

COPY road FROM '/postgresql-12.14/src/test/regress/data/streets.data';

COPY real_city FROM '/postgresql-12.14/src/test/regress/data/real_city.data';

COPY hash_i4_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY hash_name_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY hash_txt_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY hash_f8_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY test_tsvector FROM '/postgresql-12.14/src/test/regress/data/tsearch.data';

COPY testjsonb FROM '/postgresql-12.14/src/test/regress/data/jsonb.data';

-- the data in this file has a lot of duplicates in the index key
-- fields, leading to long bucket chains and lots of table expansion.
-- this is therefore a stress test of the bucket overflow code (unlike
-- the data in hash.data, which has unique index keys).
--
-- COPY hash_ovfl_heap FROM '/postgresql-12.14/src/test/regress/data/hashovfl.data';

COPY bt_i4_heap FROM '/postgresql-12.14/src/test/regress/data/desc.data';

COPY bt_name_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY bt_txt_heap FROM '/postgresql-12.14/src/test/regress/data/desc.data';

COPY bt_f8_heap FROM '/postgresql-12.14/src/test/regress/data/hash.data';

COPY array_op_test FROM '/postgresql-12.14/src/test/regress/data/array.data';

COPY array_index_op_test FROM '/postgresql-12.14/src/test/regress/data/array.data';

-- analyze all the data we just loaded, to ensure plan consistency
-- in later tests

ANALYZE aggtest;
ANALYZE onek;
ANALYZE tenk1;
ANALYZE slow_emp4000;
ANALYZE person;
ANALYZE emp;
ANALYZE student;
ANALYZE stud_emp;
ANALYZE road;
ANALYZE real_city;
ANALYZE hash_i4_heap;
ANALYZE hash_name_heap;
ANALYZE hash_txt_heap;
ANALYZE hash_f8_heap;
ANALYZE test_tsvector;
ANALYZE bt_i4_heap;
ANALYZE bt_name_heap;
ANALYZE bt_txt_heap;
ANALYZE bt_f8_heap;
ANALYZE array_op_test;
ANALYZE array_index_op_test;

--- test copying in CSV mode with various styles
--- of embedded line ending characters

create temp table copytest (
	style	text,
	test 	text,
	filler	int);

insert into copytest values('DOS',E'abc\r\ndef',1);
insert into copytest values('Unix',E'abc\ndef',2);
insert into copytest values('Mac',E'abc\rdef',3);
insert into copytest values(E'esc\\ape',E'a\\r\\\r\\\n\\nb',4);

copy copytest to '/postgresql-12.14/src/test/regress/results/copytest.csv' csv;

create temp table copytest2 (like copytest);

copy copytest2 from '/postgresql-12.14/src/test/regress/results/copytest.csv' csv;

select * from copytest except select * from copytest2;

truncate copytest2;

--- same test but with an escape char different from quote char

copy copytest to '/postgresql-12.14/src/test/regress/results/copytest.csv' csv quote '''' escape E'\\';

copy copytest2 from '/postgresql-12.14/src/test/regress/results/copytest.csv' csv quote '''' escape E'\\';

select * from copytest except select * from copytest2;


-- test header line feature

create temp table copytest3 (
	c1 int,
	"col with , comma" text,
	"col with "" quote"  int);

copy copytest3 from stdin csv header;
this is just a line full of junk that would error out if parsed
1,a,1
2,b,2
\.

copy copytest3 to stdout csv header;

-- test copy from with a partitioned table
create table parted_copytest (
	a int,
	b int,
	c text
) partition by list (b);

create table parted_copytest_a1 (c text, b int, a int);
create table parted_copytest_a2 (a int, c text, b int);

alter table parted_copytest attach partition parted_copytest_a1 for values in(1);
alter table parted_copytest attach partition parted_copytest_a2 for values in(2);

-- We must insert enough rows to trigger multi-inserts.  These are only
-- enabled adaptively when there are few enough partition changes.
insert into parted_copytest select x,1,'One' from generate_series(1,1000) x;
insert into parted_copytest select x,2,'Two' from generate_series(1001,1010) x;
insert into parted_copytest select x,1,'One' from generate_series(1011,1020) x;

copy (select * from parted_copytest order by a) to '/postgresql-12.14/src/test/regress/results/parted_copytest.csv';

truncate parted_copytest;

copy parted_copytest from '/postgresql-12.14/src/test/regress/results/parted_copytest.csv';

-- Ensure COPY FREEZE errors for partitioned tables.
begin;
truncate parted_copytest;
copy parted_copytest from '/postgresql-12.14/src/test/regress/results/parted_copytest.csv' (freeze);
rollback;

select tableoid::regclass,count(*),sum(a) from parted_copytest
group by tableoid order by tableoid::regclass::name;

truncate parted_copytest;

-- create before insert row trigger on parted_copytest_a2
create function part_ins_func() returns trigger language plpgsql as $$
begin
  return new;
end;
$$;

create trigger part_ins_trig
	before insert on parted_copytest_a2
	for each row
	execute procedure part_ins_func();

copy parted_copytest from '/postgresql-12.14/src/test/regress/results/parted_copytest.csv';

select tableoid::regclass,count(*),sum(a) from parted_copytest
group by tableoid order by tableoid::regclass::name;

truncate table parted_copytest;
create index on parted_copytest (b);
drop trigger part_ins_trig on parted_copytest_a2;

copy parted_copytest from stdin;
1	1	str1
2	2	str2
\.

-- Ensure index entries were properly added during the copy.
select * from parted_copytest where b = 1;
select * from parted_copytest where b = 2;

drop table parted_copytest;
