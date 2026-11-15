#include "beatmatch.h"
#include "render.h"
#include "wav.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100u
#define WINDOW 2048
#define HOP 512
#define SEARCH 256
#define RING 4096
char beatmatch_error[160];

static float onset[8192];
static int16_t input_window[(WINDOW+2*SEARCH)*2];
static int16_t previous[WINDOW*2];
static float accumulated[RING*2], weights[RING], taper[WINDOW];
static int16_t output_block[HOP*2];

static int fail(const char *s) {
  snprintf(beatmatch_error,sizeof(beatmatch_error),"%s",s);
  return -1;
}

BeatInfo beat_analyze_peaks(const uint16_t *peaks,unsigned bins,
                            double seconds) {
  BeatInfo result={0};
  if (!peaks || bins<256 || bins>8192 || !isfinite(seconds) || seconds<8)
    return result;
  double rate=bins/seconds, mean=0, power=0;
  float smooth=0;
  for (unsigned i=0;i<bins;i++) {
    float level=log1pf((float)peaks[i]);
    float attack=level-smooth;
    smooth=smooth*.82f+level*.18f;
    onset[i]=attack>0?attack:0;
    mean+=onset[i];
  }
  mean/=bins;
  for (unsigned i=0;i<bins;i++) {
    onset[i]-=(float)mean;
    power+=onset[i]*onset[i];
  }
  if (power<.01) return result;
  double best=-1;
  for (int tenths=700;tenths<=1800;tenths+=2) {
    double bpm=tenths*.1, lag=rate*60/bpm;
    int whole=(int)lag;
    double fraction=lag-whole, sum=0, local=0;
    for (unsigned i=0;i+whole+1<bins;i+=2) {
      double b=onset[i+whole]*(1-fraction)+onset[i+whole+1]*fraction;
      sum+=onset[i]*b;
      local+=onset[i]*onset[i];
    }
    if (local<=0) continue;
    double correlation=sum/local;
    /* Slightly prefer ordinary dance tempos when half/double time ties. */
    double preference=1-.08*fabs(log(bpm/125.)/log(2.));
    double score=correlation*preference;
    if (score>best) {best=score;result.bpm=(float)bpm;}
  }
  if (best<.11) {result.bpm=0;return result;}
  double period=rate*60/result.bpm;
  double buckets[64]={0};
  for (unsigned i=0;i<bins;i++) {
    double positive=onset[i]+mean;
    if (positive<=0) continue;
    double phase=fmod((double)i,period);
    unsigned bucket=(unsigned)(phase*64/period)&63;
    buckets[bucket]+=positive;
  }
  double highest=-1;
  int winner=0;
  for (int i=0;i<64;i++) {
    double score=buckets[(i+63)&63]+2*buckets[i]+buckets[(i+1)&63];
    if (score>highest) {highest=score;winner=i;}
  }
  result.phase_seconds=(float)((winner+.5)*period/(64*rate));
  result.confidence=(float)best;
  return result;
}

double beat_nearest(double time,const BeatInfo *beat) {
  if (!beat || beat->bpm<=0 || !isfinite(time)) return time;
  double period=60./beat->bpm;
  return fmax(0,beat->phase_seconds+
                round((time-beat->phase_seconds)/period)*period);
}

static int16_t clamp16(float v) {
  if (v>32767) return 32767;
  if (v< -32768) return -32768;
  return (int16_t)lrintf(v);
}

int beat_stretch_wav(const char *input,const char *output,
                     double tempo_ratio,BeatProgress progress) {
  beatmatch_error[0]=0;
  if (!input || !output || !strcmp(input,output) ||
      !isfinite(tempo_ratio) || tempo_ratio<.75 || tempo_ratio>1.3334)
    return fail("Tempo match supports changes up to 25 percent.");
  uint32_t frames=0,offset=0;
  if (inspect_wav(input,&frames,&offset)) return fail(render_error);
  uint64_t out64=(uint64_t)llround(frames/tempo_ratio);
  if (!out64 || out64>(INT32_MAX-44u)/4)
    return fail("Matched song exceeds the WAV size limit.");
  uint32_t target=(uint32_t)out64;
  FILE *in=fopen(input,"rb"),*out=NULL;
  int rc=-1,created=0;
  if (!in) return fail("Cannot open song for beatmatching.");
  out=fopen(output,"rb");
  if (out) {fclose(out);out=NULL;fail("Match output already exists.");goto done;}
  out=fopen(output,"wb");
  if (!out) {fail("Cannot create beatmatch cache.");goto done;}
  created=1;
  if (wav_begin(out,RATE)) {fail("Cannot write beatmatch cache.");goto done;}
  memset(accumulated,0,sizeof(accumulated));
  memset(weights,0,sizeof(weights));
  for (int i=0;i<WINDOW;i++) taper[i]=.5f-.5f*cosf(6.28318530718f*i/(WINDOW-1));
  for (uint32_t at=0;at<target;at+=HOP) {
    uint32_t hop=target-at<HOP?target-at:HOP;
    int expected=(int)llround((double)at*tempo_ratio);
    int base=at ? expected-SEARCH : 0;
    if (base<0) base=0;
    memset(input_window,0,sizeof(input_window));
    uint32_t available=base<(int)frames?frames-base:0;
    uint32_t count=available<WINDOW+2*SEARCH?available:WINDOW+2*SEARCH;
    if (count && (fseek(in,(long)offset+(long)base*4,SEEK_SET) ||
                  fread(input_window,4,count,in)!=count)) {
      fail("Beatmatch input read failed.");goto done;
    }
    int chosen=at?expected:0;
    if (at) {
      double best=-1e300;
      int low=expected-SEARCH;
      if (low<0) low=0;
      int high=expected+SEARCH;
      if (high>(int)frames-WINDOW) high=(int)frames-WINDOW;
      if (high<low) high=low;
      for (int candidate=low;candidate<=high;candidate+=8) {
        int shift=candidate-base;
        if (shift<0 || shift+WINDOW>WINDOW+2*SEARCH) continue;
        double dot=0,energy=1;
        for (int i=0;i<512;i+=8) {
          for (int ch=0;ch<2;ch++) {
            int a=previous[(HOP+i)*2+ch];
            int b=input_window[(shift+i)*2+ch];
            dot+=(double)a*b;
            energy+=(double)b*b;
          }
        }
        double score=dot/sqrt(energy)-abs(candidate-expected)*.01;
        if (score>best) {best=score;chosen=candidate;}
      }
    }
    int shift=chosen-base;
    if (shift<0 || shift+WINDOW>WINDOW+2*SEARCH) {
      fail("Beatmatch source window failed.");goto done;
    }
    memcpy(previous,input_window+shift*2,sizeof(previous));
    for (int i=0;i<WINDOW;i++) {
      int pos=(at+i)&(RING-1);
      float weight=taper[i];
      weights[pos]+=weight;
      accumulated[pos*2]+=previous[i*2]*weight;
      accumulated[pos*2+1]+=previous[i*2+1]*weight;
    }
    for (uint32_t i=0;i<hop;i++) {
      int pos=(at+i)&(RING-1);
      float inverse=weights[pos]>.00001f?1/weights[pos]:0;
      output_block[i*2]=clamp16(accumulated[pos*2]*inverse);
      output_block[i*2+1]=clamp16(accumulated[pos*2+1]*inverse);
      weights[pos]=0;
      accumulated[pos*2]=accumulated[pos*2+1]=0;
    }
    if (wav_append(out,output_block,hop)) {fail("Beatmatch cache write failed.");goto done;}
    if (progress && progress(at+hop,target)) {fail("Beatmatch cancelled.");goto done;}
  }
  if (wav_finish(out)) {fail("Cannot finish beatmatch cache.");goto done;}
  rc=0;
done:
  if (out && fclose(out)) {rc=-1;fail("Cannot close beatmatch cache.");}
  fclose(in);
  if (rc && created) remove(output);
  return rc;
}
