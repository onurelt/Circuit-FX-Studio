#ifndef TEST_VITA2D_H
#define TEST_VITA2D_H
#include <stddef.h>
#include <stdint.h>
typedef int SceGxmPrimitiveType;
#define SCE_GXM_PRIMITIVE_LINES 1
#define SCE_GXM_PRIMITIVE_TRIANGLES 2
#define SCE_GXM_TEXTURE_FORMAT_P8_ABGR 3
#define SCE_GXM_TEXTURE_FILTER_POINT 4
typedef struct { float x,y,z; unsigned color; } vita2d_color_vertex;
typedef struct {
  unsigned width,height,stride;
  unsigned char *data;
  uint32_t *palette;
} vita2d_texture;
void *vita2d_pool_memalign(unsigned size,unsigned alignment);
void vita2d_draw_array(SceGxmPrimitiveType type,
                       const vita2d_color_vertex *vertices,size_t count);
void vita2d_draw_rectangle(float x,float y,float w,float h,unsigned color);
void vita2d_draw_line(float x0,float y0,float x1,float y1,unsigned color);
void vita2d_draw_fill_circle(float x,float y,float radius,unsigned color);
vita2d_texture *vita2d_create_empty_texture_format(unsigned w,unsigned h,int format);
void vita2d_free_texture(vita2d_texture *t);
unsigned char *vita2d_texture_get_datap(vita2d_texture *t);
uint32_t *vita2d_texture_get_palette(vita2d_texture *t);
unsigned vita2d_texture_get_stride(vita2d_texture *t);
void vita2d_texture_set_filters(vita2d_texture *t,int min,int mag);
void vita2d_draw_texture_tint_part(vita2d_texture *t,float x,float y,
                                    float tx,float ty,float w,float h,unsigned color);
#endif
