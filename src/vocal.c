#include "vocal.h"
#include "render.h"
#include "wav.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define BLOCK 4096u
char vocal_error[160];
static int16_t source[BLOCK*2], stem[BLOCK*2], processed[BLOCK*2];
static int fail(const char *s) {
  snprintf(vocal_error,sizeof(vocal_error),"%s",s);
  return -1;
}
static int16_t clip(float v) {
  return (int16_t)lrintf(fmaxf(-32768,fminf(32767,v)));
}
int vocal_reduce_wav(const char *input,const char *output,
                     const char *instrumental,const char *vocals,
                     unsigned strength,VocalProgress progress) {
  vocal_error[0]=0;
  if (!input || !output || !strcmp(input,output) || strength>100)
    return fail("Invalid vocal reduction settings.");
  uint32_t frames=0,offset=0,stem_frames=0,stem_offset=0;
  if (inspect_wav(input,&frames,&offset)) return fail(render_error);
  const char *sidecar=instrumental && *instrumental ? instrumental :
                       vocals && *vocals ? vocals : NULL;
  if (sidecar && (inspect_wav(sidecar,&stem_frames,&stem_offset) ||
                  stem_frames!=frames))
    return fail("Stem must match song format and exact sample length.");
  FILE *in=fopen(input,"rb"),*sf=NULL,*out=NULL;
  int created=0,rc=-1;
  if (!in) return fail("Cannot open source for vocal reduction.");
  if (sidecar) sf=fopen(sidecar,"rb");
  if ((sidecar && (!sf || fseek(sf,stem_offset,SEEK_SET))) ||
      fseek(in,offset,SEEK_SET)) {fail("Cannot read vocal stem or song.");goto done;}
  out=fopen(output,"rb");
  if (out) {fclose(out);out=NULL;fail("Vocal cache already exists.");goto done;}
  out=fopen(output,"wb");
  if (!out) {fail("Cannot create vocal cache.");goto done;}
  created=1;
  if (wav_begin(out,44100)) {fail("Cannot write vocal cache.");goto done;}
  float low=0,s=strength/100.f;
  for (uint32_t at=0;at<frames;) {
    uint32_t n=frames-at<BLOCK?frames-at:BLOCK;
    if (fread(source,4,n,in)!=n || (sf && fread(stem,4,n,sf)!=n)) {
      fail("Vocal input read failed.");goto done;
    }
    for (uint32_t i=0;i<n;i++) {
      float l=source[i*2],r=source[i*2+1];
      if (sf) {
        for (int ch=0;ch<2;ch++) {
          float original=source[i*2+ch];
          float value=instrumental && *instrumental ?
                      original*(1-s)+stem[i*2+ch]*s :
                      original-stem[i*2+ch]*s;
          processed[i*2+ch]=clip(value);
        }
      } else {
        float mid=(l+r)*.5f;
        low+=.02114f*(mid-low); /* preserve most content below ~150 Hz */
        float removed=s*(mid-low);
        processed[i*2]=clip(l-removed);
        processed[i*2+1]=clip(r-removed);
      }
    }
    if (wav_append(out,processed,n)) {fail("Vocal cache write failed.");goto done;}
    at+=n;
    if (progress && progress(at,frames)) {fail("Vocal reduction cancelled.");goto done;}
  }
  if (wav_finish(out)) {fail("Cannot finish vocal cache.");goto done;}
  rc=0;
done:
  if (out && fclose(out)) {rc=-1;fail("Cannot close vocal cache.");}
  if (sf) fclose(sf);
  fclose(in);
  if (rc && created) remove(output);
  return rc;
}
