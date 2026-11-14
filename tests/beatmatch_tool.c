#include "beatmatch.h"
#include "render.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cancel(uint32_t at,uint32_t total) {
  (void)total;
  return getenv("CANCEL_BEATMATCH") && at>4096;
}
int main(int argc,char **argv) {
  if (argc<3) return 2;
  if (!strcmp(argv[1],"analyze")) {
    uint32_t frames,offset;
    if (inspect_wav(argv[2],&frames,&offset)) return 1;
    uint16_t peaks[8192]={0};
    FILE *f=fopen(argv[2],"rb");
    if (!f || fseek(f,offset,SEEK_SET)) return 1;
    int16_t pcm[2048*2];
    for (uint32_t at=0;at<frames;) {
      uint32_t n=frames-at<2048?frames-at:2048;
      if (fread(pcm,4,n,f)!=n) return 1;
      for (uint32_t i=0;i<n;i+=8) {
        unsigned bin=(uint64_t)(at+i)*8192/frames;
        int value=abs(pcm[2*i]);
        if (abs(pcm[2*i+1])>value) value=abs(pcm[2*i+1]);
        if (bin<8192 && peaks[bin]<value) peaks[bin]=value;
      }
      at+=n;
    }
    fclose(f);
    BeatInfo info=beat_analyze_peaks(peaks,8192,frames/44100.);
    printf("%.3f %.5f %.3f\n",info.bpm,info.phase_seconds,info.confidence);
    return info.bpm?0:1;
  }
  if (argc==5 && !strcmp(argv[1],"stretch")) {
    if (beat_stretch_wav(argv[2],argv[3],atof(argv[4]),cancel)) {
      fprintf(stderr,"%s\n",beatmatch_error);
      return 1;
    }
    return 0;
  }
  return 2;
}
