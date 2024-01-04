#pragma once

struct draw_base
{
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;

    int32_t width;
    uint32_t rgba;

    gs_vertbuffer_t *buf;
};

struct draw_line {
    draw_base base;
    int32_t start_x;
    int32_t start_y;
    int32_t end_x;
    int32_t end_y;
};

typedef struct draw_base draw_base_t;
typedef struct draw_line draw_line_t;

#define mis_get_rgba_r(rgba) ((uint32_t)(rgba)&(uint32_t)0xff)
#define mis_get_rgba_g(rgba) (((uint32_t)(rgba)&(uint32_t)0xff00) >> 8)
#define mis_get_rgba_b(rgba) (((uint32_t)(rgba)&(uint32_t)0xff0000) >> 16)
#define mis_get_rgba_a(rgba) (((uint32_t)(rgba)&(uint32_t)0xff000000) >> 24)

static void mis_swapf(float * a, float * b){
    float temp;
    temp = *a;
    *a = *b;
    *b = temp;
}
