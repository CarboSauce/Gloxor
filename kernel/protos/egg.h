#pragma once
#include <stddef.h>

typedef struct memmap_t memmap_t;


typedef struct framebuffer_t 
{
    void* fb_start;
    void* fb_end;
    size_t pitch;
    size_t height;
    size_t width;  
} framebuffer_t;

typedef struct egg_t {
    framebuffer_t fb;
} egg_t;