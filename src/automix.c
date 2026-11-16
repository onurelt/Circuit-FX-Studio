#include "automix.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static double limit(double x,double min,double max) {
  return x<min?min:x>max?max:x;
}

/* Mean energy proxy of the existing waveform envelope. Peaks cannot detect
 * vocals, key or downbeats; do not claim those capabilities. */
static double energy(const uint16_t *peaks,unsigned bins,double length,
                     double at,double width) {
  if (!peaks || bins<2 || length<=0 || width<=0) return 0;
  double end=limit(at+width,0,length);
  at=limit(at,0,length);
  if (end<=at) return 0;
  unsigned lo=(unsigned)(at*bins/length);
  unsigned hi=(unsigned)ceil(end*bins/length);
  if (hi>bins) hi=bins;
  if (lo>=bins) lo=bins-1;
  if (hi<=lo) hi=lo+1;
  double sum=0;
  for (unsigned i=lo;i<hi;i++) sum+=sqrt((double)peaks[i]);
  return sum/(hi-lo);
}

static double grid_at(double time,double phase,double step,double max) {
  if (!(step>0) || !isfinite(step)) return limit(time,0,max);
  double candidate=phase+round((time-phase)/step)*step;
  if (candidate<0) candidate+=ceil(-candidate/step)*step;
  if (candidate>max) candidate-=ceil((candidate-max)/step)*step;
  return limit(candidate,0,max);
}

static double choose_a(const uint16_t *peaks,unsigned bins,double length,
                       double overlap,double bpm,double phase,int on_grid) {
  double max=length-overlap;
  /* Prefer the last 4-8 bars. Don't throw away the entire outgoing song. */
  double desired=limit(max-240./bpm,0,max);
  if (!on_grid) return desired;
  double phrase=960./bpm; /* sixteen-beat phrase proxy */
  double best=-1e30, chosen=grid_at(desired,phase,phrase,max);
  double lower=limit(max-8*phrase,0,max);
  for (double t=grid_at(lower,phase,phrase,max);t<=max+.00001;t+=phrase) {
    if (t<lower-.00001) continue;
    double pre=energy(peaks,bins,length,t-phrase,phrase);
    double during=energy(peaks,bins,length,t,fmin(phrase,overlap));
    double contrast=fabs(during-pre)/(1+pre+during);
    double closeness=1-fabs(t-desired)/(1+max-lower);
    double score=0.55*closeness+0.45*contrast;
    if (score>best) {best=score;chosen=t;}
  }
  return chosen;
}

static double choose_b(const uint16_t *peaks,unsigned bins,double length,
                       double consumed,double bpm,double phase,int on_grid) {
  double max=length-consumed;
  double window=fmin(max,2400./bpm); /* inspect opening ~40 beats */
  if (window<=0) return 0;
  if (!on_grid) return 0;
  double phrase=960./bpm, best=-1e30,chosen=0;
  for (double t=grid_at(0,phase,phrase,max);t<=window+.00001;t+=phrase) {
    double before=energy(peaks,bins,length,t-phrase,phrase);
    double after=energy(peaks,bins,length,t,fmin(phrase,consumed));
    double rise=(after-before)/(1+after+before);
    double early=1-t/(window+1);
    double score=.7*rise+.3*early;
    if (score>best) {best=score;chosen=t;}
  }
  return chosen;
}

int automix_plan(const uint16_t *a,const uint16_t *b,unsigned bins,
                 double seconds_a,double seconds_b,
                 const BeatInfo *beat_a,const BeatInfo *beat_b,
                 AutoMixPlan *out) {
  if (!out || !a || !b || bins<256 || bins>8192 ||
      !isfinite(seconds_a) || !isfinite(seconds_b) ||
      seconds_a<8 || seconds_b<8 || seconds_a>48000 || seconds_b>48000)
    return -1;
  memset(out,0,sizeof(*out));
  double bpm_a=beat_a && isfinite(beat_a->bpm) &&
               beat_a->bpm>=70 && beat_a->bpm<=180?beat_a->bpm:0;
  double bpm_b=beat_b && isfinite(beat_b->bpm) &&
               beat_b->bpm>=70 && beat_b->bpm<=180?beat_b->bpm:0;
  double bpm=bpm_a?bpm_a:125;
  double ratio=(bpm_a && bpm_b)?bpm_a/bpm_b:1;
  int sync=bpm_a>0 && bpm_b>0 && ratio>=.75 && ratio<=1.3334;
  if (!sync) ratio=1; /* never reject an incompatible pair */
  double phase_a=beat_a && isfinite(beat_a->phase_seconds)?
                 beat_a->phase_seconds:0;
  double phase_b=beat_b && isfinite(beat_b->phase_seconds)?
                 beat_b->phase_seconds:0;
  phase_a=limit(phase_a,0,60/bpm);
  phase_b=limit(phase_b,0,60/(bpm_b?bpm_b:bpm));
  double overlap=1920/bpm; /* eight bars */
  if (overlap>fmin(seconds_a,seconds_b/ratio)*.6)
    overlap=960/bpm;  /* shorten gracefully for shorter songs */
  overlap=limit(overlap,2.,fmin(120,fmin(seconds_a,seconds_b/ratio)*.8));
  if (overlap<2 || !isfinite(overlap)) return -1;
  double consumed=overlap*ratio;
  double start_a=choose_a(a,bins,seconds_a,overlap,bpm,phase_a,sync);
  double start_b=choose_b(b,bins,seconds_b,consumed,bpm_b?bpm_b:bpm,
                          phase_b,sync);
  start_a=limit(start_a,0,seconds_a-overlap);
  start_b=limit(start_b,0,seconds_b-consumed);
  out->mix.start_a=floor(start_a*1000+.5)/1000;
  out->mix.start_b=floor(start_b*1000+.5)/1000;
  out->mix.duration=overlap;
  out->mix.bpm=(float)bpm;
  out->mix.effects=FX_CROSSFADE; /* prioritize safe, clean playback */
  out->mix.intensity=.75f;
  out->mix.sample_speed=1;
  out->tempo_ratio=ratio;
  out->use_beatmatch=sync;
  out->matched_phrases=sync;
  return 0;
}
