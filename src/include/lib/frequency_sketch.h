#ifndef FREQUENCY_SKETCH_H
#define FREQUENCY_SKETCH_H

typedef struct frequency_sketch frequency_sketch;

extern frequency_sketch *sketch_create(int64 total_elems, int sketch_work_mem,
        uint64 seed, int bits_per_elem);
extern void sketch_free(frequency_sketch *sketch);
extern uint64 sketch_used_mem(frequency_sketch *sketch);
		extern void sketch_add_element(frequency_sketch *sketch, unsigned char *elem,
        size_t len);
extern uint64 sketch_get_frequency(frequency_sketch *sketch,
        unsigned char *elem, size_t len);
extern void sketch_reset(frequency_sketch *sketch);


#endif                      /* FREQUENCY_SKETCH_H */
