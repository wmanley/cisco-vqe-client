/*------------------------------------------------------------------
 * Libevent Benchmarking Logic
 *
 * March 2009, Matt Caulfield
 *
 * Copyright (c) 2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdint.h>

#define MICROS_PER_SEC 1000000
#define MAX_SAMPLES 60
#define SAMPLE_TIME 5
#define PRECISION 1000

#define MIN(_x,_y) _x < _y ? _x : _y
#define MAX(_x,_y) _x > _y ? _x : _y

/* Benchmarking statics */
static uint64_t start_time, start_cycles, sample_cycles;
static uint32_t samples[MAX_SAMPLES];
static uint32_t sample_index;
static uint8_t benchmark_enabled;

static void benchmark_enable (void) 
{
    sample_index = 0;
    memset(samples, 0, sizeof(samples));
    benchmark_enabled = 1;
}

static void benchmark_disable (void) 
{
    benchmark_enabled = 0;
    start_time = 0;
}

static int32_t benchmark_get (uint32_t interval) 
{
    uint32_t start_index, i, num_samples, total;

    start_index = sample_index;
    num_samples = MAX(1, MIN(interval / SAMPLE_TIME, MAX_SAMPLES));
    total = 0;

    for (i = 0; i < num_samples; i++) {
        total += samples[(start_index - i) % MAX_SAMPLES];
    }

    /* Note: For intervals greater than the sample size, this
     *       function's return value is marginally inprecise. 
     *       We assume all samples to be of equal weight.
     */
    return total / num_samples;
}

static void benchmark_init (void) 
{
    start_time = 0;
    start_cycles = 0;
    sample_cycles = 0;
    memset(samples, 0, sizeof(samples));
    sample_index = 0;
    benchmark_enabled = 0;
}

static inline uint64_t get_time_in_micros (void) 
{
    uint64_t micros_time;
    struct timeval tv;
    if (gettimeofday(&tv, 0)) {
        micros_time = 0; /* Failure case */
    } else {
        micros_time = tv.tv_sec;
        micros_time *= MICROS_PER_SEC;
        micros_time += tv.tv_usec;
    }
    return micros_time;
}

static inline void benchmark_start_sample (void) 
{
    uint64_t curr_time, total_cycles;
    curr_time = get_time_in_micros();
    if (!benchmark_enabled) {
        return;
    }
    if (!start_time) {
        start_time = curr_time;
    } else if ((curr_time - start_time) >= (SAMPLE_TIME * MICROS_PER_SEC)) {
        total_cycles = curr_time - start_time;
        if (sample_cycles < total_cycles) {
            sample_index = (sample_index + 1) % MAX_SAMPLES;
            samples[sample_index] = sample_cycles / (total_cycles / PRECISION);
        }
        start_time = curr_time;
        sample_cycles = 0;
    }
    start_cycles = get_time_in_micros();
}

static inline void benchmark_stop_sample (void) 
{
    if (!benchmark_enabled) {
        return;
    }
    if (start_time) {
        sample_cycles += (get_time_in_micros() - start_cycles);
    }
}

