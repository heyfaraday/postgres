CREATE EXTENSION test_frequency_sketch;

-- See README for explanation of arguments:
SELECT test_frequency_sketch(nelements => 1000000,
                      bits_per_elem => 4,
                      work_mem => 1073741824,
                      seed => 11111,
                      mod => 100000,
                      tests => 1);

SELECT test_frequency_sketch(nelements => 100000,
                             bits_per_elem => 4,
                             work_mem => 1073741824,
                             seed => 11111,
                             mod => 100000,
                             tests => 1);

SELECT test_frequency_sketch(nelements => 1000000,
                             bits_per_elem => 8,
                             work_mem => 1073741824,
                             seed => 11111,
                             mod => 100000,
                             tests => 1);

SELECT test_frequency_sketch(nelements => 1000000,
                             bits_per_elem => 16,
                             work_mem => 1073741824,
                             seed => 11111,
                             mod => 100,
                             tests => 1);
