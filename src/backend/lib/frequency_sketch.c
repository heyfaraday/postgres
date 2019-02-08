#include "postgres.h"

#include <math.h>

#include "access/hash.h"
#include "lib/frequency_sketch.h"

#define MAX_HASH_FUNCS          10
#define BLOCK_SIZE              sizeof(uint64) * BITS_PER_BYTE
#define POWER_OF_BLOCK_SIZE     6

struct frequency_sketch
{
    int         k_hash_funcs;
    uint64      seed;

    int         bits_per_elem;
    uint64      elem_mask;
    uint64      reset_mask;
    uint64      size;

    uint64      bitset[FLEXIBLE_ARRAY_MEMBER];
};

static uint32 get_index(uint32 hash);
static uint32 get_offset(uint32 hash);
static int	my_sketch_power(uint64 target_bitset_bits);
static int	optimal_k(uint64 bitset_bits, int64 total_elems);
static void k_hashes(frequency_sketch *sketch, uint32 *hashes, unsigned char *elem,
                     size_t len);
static inline uint32 mod_m(uint32 a, uint64 m);

frequency_sketch *
sketch_create(int64 total_elems, int sketch_work_mem, uint64 seed, int bits_per_elem)
{
    frequency_sketch    *sketch;
    int         sketch_power;
    uint64      bitset_bytes;
    uint64      bitset_bits;
    uint64      bitset_elems;
    int         i;

    Assert((bits_per_elem - 1) & bits_per_elem == 0);

    bitset_bytes = Min(sketch_work_mem * UINT64CONST(1024), total_elems * bits_per_elem * 2);
    bitset_bytes = Max(1024 * 1024, bitset_bytes);

    sketch_power = my_sketch_power(bitset_bytes * BITS_PER_BYTE);
    bitset_bits = UINT64CONST(1) << sketch_power;
    bitset_bytes = bitset_bits / BITS_PER_BYTE;
    bitset_elems = bitset_bits / bits_per_elem;

    sketch = palloc0(offsetof(frequency_sketch, bitset) +
            sizeof(unsigned char) * bitset_bytes);
    sketch->k_hash_funcs = optimal_k(bitset_elems, total_elems);
    sketch->seed = seed;
    sketch->bits_per_elem = bits_per_elem;
    sketch->elem_mask = (UINT64CONST(1) << bits_per_elem) - 1;
    for (i = 0; i < BLOCK_SIZE / bits_per_elem; ++i)
    {
        sketch->reset_mask |= ((UINT64CONST(1) << (bits_per_elem - 1)) - 1) << bits_per_elem * i;
    }
    sketch->size = bitset_elems;

    return sketch;
}

void
sketch_free(frequency_sketch *sketch)
{
    pfree(sketch);
}

void
sketch_add_element(frequency_sketch *sketch, unsigned char *elem, size_t len)
{
    uint32  hashes[MAX_HASH_FUNCS];
    int     i;
    uint32  index;
    uint32  offset;
    uint64  min = UINT64CONST(1) << sketch->bits_per_elem;

    k_hashes(sketch, hashes, elem, len);

    for (i = 0; i < sketch->k_hash_funcs; ++i)
    {
        index = get_index(hashes[i]);
        offset = get_offset(hashes[i]);
        min = Min(min, sketch->bitset[index] >> offset & sketch->elem_mask);
    }

    if (min == sketch->elem_mask) return;

    for (i = 0; i < sketch->k_hash_funcs; ++i)
    {
        index = get_index(hashes[i]);
        offset = get_offset(hashes[i]);
        if ((sketch->bitset[index] >> offset & sketch->elem_mask) == min)
        {
            sketch->bitset[index] += UINT64CONST(1) << offset;
        }
    }
}

uint64
sketch_get_frequency(frequency_sketch *sketch, unsigned char *elem, size_t len)
{
    uint32  hashes[MAX_HASH_FUNCS];
    int     i;
    uint32  index;
    uint32  offset;
    uint64  min = UINT64CONST(1) << sketch->bits_per_elem;

    k_hashes(sketch, hashes, elem, len);

    for (i = 0; i < sketch->k_hash_funcs; ++i)
    {
        index = get_index(hashes[i]);
        offset = get_offset(hashes[i]);
        min = Min(min, sketch->bitset[index] >> offset & sketch->elem_mask);
    }

    return min;
}

void
sketch_reset(frequency_sketch *sketch)
{
    int i;

    for (i = 0; i < sketch->size; ++i) {
        sketch->bitset[i] >>= 1;
        sketch->bitset[i] &= sketch->reset_mask;
    }
}

static uint32
get_index(uint32 hash)
{
    return hash >> POWER_OF_BLOCK_SIZE;
}

static uint32
get_offset(uint32 hash)
{
    return hash & ((1 << POWER_OF_BLOCK_SIZE) - 1);
}

static int
my_sketch_power(uint64 target_bitset_bits)
{
    int			sketch_power = -1;

    while (target_bitset_bits > 0 && sketch_power < 32)
    {
        sketch_power++;
        target_bitset_bits >>= 1;
    }

    return sketch_power;
}

static int
optimal_k(uint64 bitset_bits, int64 total_elems)
{
    int			k = rint(log(2.0) * bitset_bits / total_elems);

    return Max(1, Min(k, MAX_HASH_FUNCS));
}

static void
k_hashes(frequency_sketch *filter, uint32 *hashes, unsigned char *elem, size_t len)
{
    uint64		hash;
    uint32		x;
    uint32      y;
    uint64		m;
    int			i;

    /* Use 64-bit hashing to get two independent 32-bit hashes */
    hash = DatumGetUInt64(hash_any_extended(elem, len, filter->seed));
    x = (uint32) hash;
    y = (uint32) (hash >> 32);
    m = filter->size;

    x = mod_m(x, m);
    y = mod_m(y, m);

    /* Accumulate hashes */
    hashes[0] = x * filter->bits_per_elem;
    for (i = 1; i < filter->k_hash_funcs; i++)
    {
        x = mod_m(x + y, m);
        y = mod_m(y + i, m);

        hashes[i] = x * filter->bits_per_elem;
    }
}

static inline uint32
mod_m(uint32 val, uint64 m)
{
    Assert(m <= PG_UINT32_MAX + UINT64CONST(1));
    Assert(((m - 1) & m) == 0);

    return val & (m - 1);
}

