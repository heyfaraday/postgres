/* src/test/modules/test_frequency_sketch/test_frequency_sketch--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION test_frequency_sketch" to load this file. \quit

CREATE FUNCTION test_frequency_sketch(nelements bigint,
    bits_per_elem integer,
    work_mem integer DEFAULT 1073741824,
    seed integer DEFAULT -1,
    mod integer DEFAULT 100000,
    tests integer DEFAULT 1)
RETURNS pg_catalog.void STRICT
AS 'MODULE_PATHNAME' LANGUAGE C;
