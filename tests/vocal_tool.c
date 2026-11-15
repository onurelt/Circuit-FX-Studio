#include "vocal.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int cancel(uint32_t at,uint32_t total) {
  (void)total;
  return getenv("CANCEL_VOCAL") && at>4096;
}
int main(int argc,char **argv) {
  if (argc==7 && !strcmp(argv[1],"reduce")) {
    int rc=vocal_reduce_wav(argv[2],argv[3],
            strcmp(argv[4],"-")?argv[4]:NULL,
            strcmp(argv[5],"-")?argv[5]:NULL,
            (unsigned)atoi(argv[6]),cancel);
    if (rc) fprintf(stderr,"%s\n",vocal_error);
    return rc?1:0;
  }
  if (argc==11 && !strcmp(argv[1],"render")) {
    MixSettings cfg={.start_a=atof(argv[7]),.start_b=atof(argv[8]),
                     .duration=atof(argv[9]),.effects=FX_CROSSFADE};
    int rc=render_mix_layers(argv[2],argv[3],
            strcmp(argv[4],"-")?argv[4]:NULL,
            strcmp(argv[5],"-")?argv[5]:NULL,
            argv[6],&cfg,atoi(argv[10]),NULL);
    if (rc) fprintf(stderr,"%s\n",render_error);
    return rc?1:0;
  }
  return 2;
}
