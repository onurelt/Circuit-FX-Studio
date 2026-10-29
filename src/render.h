#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
typedef int (*Progress)(uint32_t done, uint32_t total);
extern char render_error[160];
int render_mix(const char *a,const char *b,const char *out,double seconds,int curve,int preview,Progress progress);
#endif
