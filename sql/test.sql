--
-- test.sql
--
-- Setup: create test objects before activating safesession
create table t(i int);

create function f(IN p int, OUT i int) returns int as $$
begin
  insert into t(i) values (f.p) returning t.i into f.i;
end$$
language plpgsql;

-- Activate safesession: from now on the session is read-only
LOAD 'safesession';

-- SELECTs still work
select * from t;

-- DML is blocked
insert into t values (1);

-- Volatile function that attempts a write is blocked
select f(1);

-- DDL is blocked
CREATE TABLE t2(i int);

-- DO block with DML is blocked
DO $$ BEGIN INSERT INTO t VALUES (99); END $$;

select * from t;

-- Modifying CTE is blocked
with inserted as (insert into t values(3) returning 3) select * from inserted;

-- Large object creation is blocked
SELECT lo_create(0);

-- Bypass attempts via set_config() must not disable read-only mode
SELECT pg_catalog.set_config('transaction_read_only', 'off', false);
SELECT pg_catalog.set_config('default_transaction_read_only', 'off', false);
insert into t values (1);

SHOW transaction_read_only;
SHOW default_transaction_read_only;

-- Bypass attempt via set_config inside an explicit transaction block
BEGIN;
SELECT pg_catalog.set_config('transaction_read_only', 'off', true);
ROLLBACK;

BEGIN;
SELECT pg_catalog.set_config('default_transaction_read_only', 'off', true);
insert into t values (1);
COMMIT;

SHOW transaction_read_only;
SHOW default_transaction_read_only;
