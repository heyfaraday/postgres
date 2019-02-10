#include "postgres.h"

#include "access/hash.h"
#include "fmgr.h"
#include "lib/frequency_sketch.h"
#include "miscadmin.h"

PG_MODULE_MAGIC;

typedef struct for_data
{
	uint32 key;
	char status;
	uint64 value;
} for_data;

#define SH_PREFIX freq
#define SH_ELEMENT_TYPE for_data
#define SH_KEY_TYPE uint32
#define SH_KEY key
#define SH_HASH_KEY(tb, key) hash_uint32(key)
#define SH_EQUAL(tb, a, b) a == b
#define SH_SCOPE static inline
#define SH_DEFINE
#define SH_DECLARE

#include "lib/simplehash.h"

typedef uint64 (*rndptr_t)(int helper, int mod);

uint64 sum_rand(int helper, int mod)
{
	int i;
	uint64 rnd = 0;

	for (i = 0; i < helper; ++i)
	{
		rnd += random() % mod;
	}

	return rnd % mod;
}

uint64 mul_rand(int helper, int mod)
{
	int i;
	uint64 rnd = 1;

	for (i = 0; i < helper; ++i)
	{
		rnd *= random() % mod;
		rnd %= mod;
	}

	return rnd;
}

static void
create_and_test_sketch(int64 nelements, int bits_per_elem, int work_mem, int callerseed, int mod,
		rndptr_t rnd, int helper, const char *deviation)
{
	uint64		seed;
	uint64		tmp;
	uint64 		tmp2;
	uint64 		val1;
	uint64 		val2;
	uint64 		tries = 0;
	bool 		found;
	int64		errors = 0;
	frequency_sketch	*sketch;
	freq_hash	*hash_table;
	int 		i;
	for_data	*data;

	srandom(callerseed);

	seed = callerseed < 0 ? random() % PG_INT32_MAX : callerseed;

	hash_table = freq_create(CurrentMemoryContext, nelements, NULL);
	sketch = sketch_create(nelements, work_mem, seed, bits_per_elem);

	for (i = 0; i < 2 * nelements; ++i)
	{
		if (random() % 2)
		{
			tmp = rnd(helper, mod);
			sketch_add_element(sketch, (unsigned char *) &tmp, sizeof(tmp));
			++(freq_insert(hash_table, tmp, &found)->value);
		}
		else
		{
			tmp = rnd(helper, mod) % mod;
			tmp2 = rnd(helper, mod) % mod;
			data = freq_lookup(hash_table, tmp);
			val1 = (data ? data->value : 0);
			data = freq_lookup(hash_table, tmp2);
			val2 = (data ? data->value : 0);
			tmp = sketch_get_frequency(sketch, (unsigned char *) &tmp, sizeof(tmp));
			tmp2 = sketch_get_frequency(sketch, (unsigned char *) &tmp2, sizeof(tmp2));
			++tries;

			if ((val1 < val2 && tmp >= tmp2) || (val1 == val2 && tmp != tmp2) || (val1 > val2 && tmp <= tmp2))
			{
				++errors;
			}
		}
	}

	elog(DEBUG1, " FREQUENCY SKETCH:\nbits per element: %d\nused mem: %lu MB\nrandom distribution: %s\n"
			  "numbers bound: %d\nadded %ld elements\nhits: %ld\nmisses: %ld\n"
	          "hit ratio: %f\n  -----------------\n",
		   	  bits_per_elem, sketch_used_mem(sketch) >> 20, deviation, mod,
		   	  2 * nelements - tries, tries - errors, errors,
			  (double)(tries - errors) / tries);

	freq_destroy(hash_table);
	sketch_free(sketch);
}

PG_FUNCTION_INFO_V1(test_frequency_sketch);

/*
 * SQL-callable entry point to perform all tests.
 *
 * If a 1% false positive threshold is not met, emits WARNINGs.
 *
 * See README for details of arguments.
 */
Datum
test_frequency_sketch(PG_FUNCTION_ARGS)
{
	int64		nelements = PG_GETARG_INT64(0);
	int 		bits_per_elem = PG_GETARG_INT32(1);
	int 		work_mem = PG_GETARG_INT32(2);
	int			seed = PG_GETARG_INT32(3);
	int 		mod = PG_GETARG_INT32(4);
	int			tests = PG_GETARG_INT32(5);
	int			i;

	if (tests <= 0)
		elog(ERROR, "invalid number of tests: %d", tests);

	if (nelements < 0)
		elog(ERROR, "invalid number of elements: %d", nelements);

	if (((bits_per_elem - 1) & bits_per_elem) != 0 || bits_per_elem <= 0 || bits_per_elem > 64)
	{
		elog(ERROR, "invalid number of bits per element (should be a power of 2,"
			  "less or equal than 64: %d", bits_per_elem);
	}

	if (work_mem <= 0)
		elog(ERROR, "invalid number of work mem: %d", work_mem);

	if (mod <= 0)
		elog(ERROR, "invalid mod: %d", mod);

	for (i = 0; i < tests; i++)
	{
		elog(DEBUG1, "beginning test case #%d...", i + 1);

		create_and_test_sketch(nelements, bits_per_elem, work_mem, seed, mod, sum_rand, 1, "~uniform");
		create_and_test_sketch(nelements, bits_per_elem, work_mem, seed, mod, sum_rand, 10,
				"almost normal");
		create_and_test_sketch(nelements, bits_per_elem, work_mem, seed, mod, mul_rand, 2, "~uniform squared");
		create_and_test_sketch(nelements, bits_per_elem, work_mem, seed, mod, mul_rand, 3, "~uniform cubed");
	}

	PG_RETURN_VOID();
}
