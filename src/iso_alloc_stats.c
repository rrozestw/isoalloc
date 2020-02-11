/* iso_alloc_stats.c - A secure memory allocator
 * Copyright 2020 - chris.rohlf@gmail.com */

#include "iso_alloc_internal.h"

INTERNAL_HIDDEN int32_t _iso_alloc_detect_leaks() {
    int32_t total_leaks = 0;
    for(size_t i = 0; i < _root->zones_used; i++) {
        iso_alloc_zone *zone = &_root->zones[i];

        if(zone == NULL) {
            break;
        }

        LOCK_ZONE_MUTEX(zone);
        UNMASK_ZONE_PTRS(zone);

        total_leaks += _iso_alloc_zone_leak_detector(zone);

        MASK_ZONE_PTRS(zone);
        UNLOCK_ZONE_MUTEX(zone);
    }

    return total_leaks;
}
/* This is the built-in leak detector. It works by scanning
 * the bitmap for every allocated zone and looking for
 * uncleared bits. All user allocations should have been
 * free'd by the time this destructor runs! */
INTERNAL_HIDDEN int32_t _iso_alloc_zone_leak_detector(iso_alloc_zone *zone) {
    int32_t total_leaks = 0;

#if LEAK_DETECTOR
    if(zone == NULL) {
        return ERR;
    }

    int32_t *bm = (int32_t *) zone->bitmap_start;
    int64_t bit_position;
    int32_t was_used = 0;

    for(int32_t i = 0; i < zone->bitmap_size / sizeof(int32_t); i++) {
        for(size_t j = 0; j < BITS_PER_DWORD; j += BITS_PER_CHUNK) {
            int32_t bit = GET_BIT(bm[i], j);

            if((GET_BIT(bm[i], (j + 1))) == 1) {
                was_used++;
            }

            if(bit != 0) {
                total_leaks++;
                bit_position = (i * BITS_PER_DWORD) + j;
                void *leak = (zone->user_pages_start + ((bit_position / BITS_PER_CHUNK) * zone->chunk_size));
                LOG("Leaked chunk of %zu bytes detected in zone[%d] at %p (bit position = %ld)", zone->chunk_size, i, leak, bit_position);
            }
        }
    }

    float percentage = (float) was_used / (GET_CHUNK_COUNT(zone)) * 100.0;

    LOG("Zone[%d] Total number of %zu byte chunks(%zu) used(%d) (%%%d)", zone->index, zone->chunk_size, GET_CHUNK_COUNT(zone), was_used, (int32_t) percentage);
#endif
    return total_leaks;
}

INTERNAL_HIDDEN int32_t _iso_alloc_zone_mem_usage(iso_alloc_zone *zone) {
    uint64_t mem_usage = 0;
    mem_usage += zone->bitmap_size;
    mem_usage += ZONE_USER_SIZE;
    LOG("Zone[%d] Total bytes(%ld) megabytes(%ld)", zone->index, mem_usage, (mem_usage / MEGABYTE_SIZE));
    return (mem_usage / MEGABYTE_SIZE);
}

INTERNAL_HIDDEN int32_t _iso_alloc_mem_usage() {
    uint64_t mem_usage = 0;
    for(size_t i = 0; i < _root->zones_used; i++) {
        iso_alloc_zone *zone = &_root->zones[i];
        mem_usage += zone->bitmap_size;
        mem_usage += ZONE_USER_SIZE;
        LOG("Zone[%d] Total bytes(%ld) megabytes(%ld)", zone->index, mem_usage, (mem_usage / MEGABYTE_SIZE));
    }

    return (mem_usage / MEGABYTE_SIZE);
}