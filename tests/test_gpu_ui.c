#include "ui.h"
#include "vita2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Alignas(16) unsigned char pool[1024*1024];
static size_t used;
static int wave_batches,curve_batches,tri_batches,rectangles,textures;
static int deny_curve_alloc;
void *vita2d_pool_memalign(unsigned size,unsigned alignment) {
  assert(alignment>=16 && (alignment&(alignment-1))==0);
  if (deny_curve_alloc && size==2*39*6*sizeof(vita2d_color_vertex)) return NULL;
  used=(used+alignment-1)&~(size_t)(alignment-1);
  assert(used+size<=sizeof(pool));
  void *p=pool+used;
  used+=size;
  return p;
}
void vita2d_draw_array(int type,const vita2d_color_vertex *v,size_t count) {
  assert(type==SCE_GXM_PRIMITIVE_TRIANGLES && count%3==0);
  assert(((uintptr_t)v&15)==0);
  for (size_t i=0;i<count;i++) {
    assert(isfinite(v[i].x) && isfinite(v[i].y) && v[i].z==.5f);
  }
  for (size_t i=0;i<count;i+=3) {
    float area=(v[i+1].x-v[i].x)*(v[i+2].y-v[i].y)-
               (v[i+2].x-v[i].x)*(v[i+1].y-v[i].y);
    assert(fabsf(area)>.001f);
  }
  tri_batches++;
  if (count==880*6) wave_batches++;
  if (count==2*39*6) curve_batches++;
}
void vita2d_draw_rectangle(float x,float y,float w,float h,unsigned color) {
  (void)x;(void)y;(void)color;
  assert(w>0 && h>0);
  rectangles++;
}
void vita2d_draw_line(float x0,float y0,float x1,float y1,unsigned color) {
  (void)x0;(void)y0;(void)x1;(void)y1;(void)color;
  assert(!"curves must use triangles, not hardware line primitives");
}
void vita2d_draw_fill_circle(float x,float y,float radius,unsigned color) {
  (void)x;(void)y;(void)color;
  assert(radius>0);
}
vita2d_texture *vita2d_create_empty_texture_format(unsigned w,unsigned h,int fmt) {
  assert(fmt==SCE_GXM_TEXTURE_FORMAT_P8_ABGR);
  vita2d_texture *t=calloc(1,sizeof(*t));
  assert(t);
  t->width=t->stride=w;t->height=h;
  t->data=calloc(w*h,1);t->palette=calloc(256,sizeof(uint32_t));
  assert(t->data && t->palette);
  return t;
}
void vita2d_free_texture(vita2d_texture *t) {
  free(t->data);free(t->palette);free(t);
}
unsigned char *vita2d_texture_get_datap(vita2d_texture *t) {return t->data;}
uint32_t *vita2d_texture_get_palette(vita2d_texture *t) {return t->palette;}
unsigned vita2d_texture_get_stride(vita2d_texture *t) {return t->stride;}
void vita2d_texture_set_filters(vita2d_texture *t,int min,int mag) {
  (void)t;assert(min==SCE_GXM_TEXTURE_FILTER_POINT && mag==min);
}
void vita2d_draw_texture_tint_part(vita2d_texture *t,float x,float y,
                                    float tx,float ty,float w,float h,unsigned color) {
  (void)t;(void)x;(void)y;(void)tx;(void)ty;(void)color;
  assert(w>0 && h>0);textures++;
}
static void frame(View *v) {
  used=0;wave_batches=curve_batches=tri_batches=rectangles=textures=0;
  ui_draw(NULL,v);
  assert(wave_batches==2 && curve_batches==0);
  assert(tri_batches>=2 && rectangles>20 && textures>20);
  assert(ui_gpu_fallback_count()==0);
}
int main(void) {
  View v={0};
  assert(ui_gpu_init()==0);
  v.progress=-1;v.songs[0]="A.wav";v.songs[1]="B.wav";
  v.lengths[0]=360;v.lengths[1]=390;
  v.settings=(MixSettings){.start_a=150,.start_b=48,.duration=15,
                            .effects=FX_CROSSFADE,.bpm=128,.intensity=1};
  v.wave_origin[0]=150;v.wave_origin[1]=48;
  v.wave_window=45;v.wave_pan=-22.5;
  v.track_bpm[0]=128;v.track_bpm[1]=120;
  v.auto_beatmatch=1;v.vocal_strength=70;v.vocal_target=2;
  for (int d=0;d<2;d++) for (int i=0;i<WAVE_BINS;i++)
    v.peaks[d][i]=15000;
  v.candidates_ready=1;v.mix_variation=0;
  v.candidate_start_a[0]=150;v.candidate_start_b[0]=48;
  v.candidate_duration[0]=15;
  frame(&v);
  v.mix_variation=1;frame(&v);
  v.mix_variation=2;frame(&v);
  v.browser=1;v.entries[0]="A.wav";v.browser_count=1;frame(&v);
  v.browser=0;v.preset_browser=1;v.selected_preset=65;frame(&v);
  v.preset_browser=0;v.editing=1;strcpy(v.entry,"155.250");frame(&v);
  v.editing=0;v.confirm=1;frame(&v);
  v.confirm=0;v.progress=50;frame(&v);
  v.progress=-1;v.workflow=1;
  used=0;wave_batches=curve_batches=rectangles=0;
  ui_draw(NULL,&v);
  assert(wave_batches==1 && curve_batches==0 && rectangles>20);
  assert(ui_gpu_fallback_count()==0);
  v.workflow=0;
  ui_gpu_fini();
  puts("PASS: native Vita2D draw path, aligned 0.5-depth waveform triangles, "
       "candidate cards, progress and modal screens.");
}
