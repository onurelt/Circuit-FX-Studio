#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "render.h"
PSP_MODULE_INFO("Circuit FX Studio",0,0,60);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(128);
PSP_HEAP_SIZE_KB(8192);
#define print pspDebugScreenPrintf
#define MAX_FILES 512
static char songs[MAX_FILES][256];
static int count,track[2]={-1,-1},duration=16,curve=0,ready=0,approved=0;
static volatile int running=1;
static char base[768],a[1024],b[1024],preview[1024],out[1024],status[180]="Choose song A and song B.";
static const char *curves[]={"Linear crossfade","Sine blend (peak-safe)","Smooth crossfade","Dip to silence"};
static unsigned old;
static int exit_cb(int x,int y,void *p){(void)x;(void)y;(void)p;running=0;return 0;}
static int callbacks(SceSize n,void *p){(void)n;(void)p;int id=sceKernelCreateCallback("Exit",exit_cb,0);sceKernelRegisterExitCallback(id);sceKernelSleepThreadCB();return 0;}
static unsigned buttons(void){SceCtrlData p;sceCtrlReadBufferPositive(&p,1);unsigned hit=p.Buttons&~old;old=p.Buttons;return hit;}
static void release_buttons(void){SceCtrlData p;do{sceCtrlReadBufferPositive(&p,1);sceKernelDelayThread(10000);}while(running&&p.Buttons);old=0;}
static void invalidate(void){ready=approved=0;remove(preview);}
static int cmp(const void *x,const void *y){return strcasecmp(x,y);}
static void scan(void){
 count=0;char path[1024];snprintf(path,sizeof(path),"%s/music",base);int d=sceIoDopen(path);if(d<0){snprintf(status,sizeof(status),"Cannot open music folder.");return;}
 SceIoDirent e;memset(&e,0,sizeof(e));
 while(sceIoDread(d,&e)>0){size_t n=strlen(e.d_name);if(!FIO_S_ISDIR(e.d_stat.st_mode)&&n>4&&!strcasecmp(e.d_name+n-4,".wav")&&count<MAX_FILES){snprintf(songs[count++],256,"%s",e.d_name);}memset(&e,0,sizeof(e));}
 sceIoDclose(d);qsort(songs,count,sizeof(songs[0]),cmp);
}
static int choose(int which){
 int pos=track[which]>=0?track[which]:0;release_buttons();
 while(running){pspDebugScreenClear();print("CIRCUIT FX STUDIO  |  Select song %c\n\n",'A'+which);print("Put WAV files in CIRCUITFX/music/\n44.1kHz, stereo, 16-bit PCM\n\n");
  int first=pos/14*14;for(int i=first;i<count&&i<first+14;i++)print("%s %.57s\n",i==pos?">":" ",songs[i]);
  if(!count)print("No WAV songs found. Copy songs via USB first.\n");
  print("\nUP/DOWN select | X use song | O cancel\n%d files (maximum 512)\n",count);
  unsigned hit=buttons();if(hit&PSP_CTRL_CIRCLE)return -1;if(count){if(hit&PSP_CTRL_UP)pos=(pos+count-1)%count;if(hit&PSP_CTRL_DOWN)pos=(pos+1)%count;if(hit&PSP_CTRL_CROSS)return pos;}
  sceKernelDelayThread(35000);
 }return -1;
}
static int progress(uint32_t done,uint32_t total){
 static int last=-1;int pc=total?(int)((uint64_t)done*100/total):0;
 if(pc!=last){pspDebugScreenSetXY(0,5);print("Progress: %3d%%   O cancels\n",pc);last=pc;scePowerTick(0);}
 SceCtrlData p;sceCtrlPeekBufferPositive(&p,1);return !running||(p.Buttons&PSP_CTRL_CIRCLE);
}
static int play(void){
 FILE *f=fopen(preview,"rb");if(!f)return -1;
 /* Preview was produced by our writer: fixed 44-byte PCM header. */
 unsigned char h[44];if(fread(h,1,44,f)!=44||memcmp(h,"RIFF",4)||memcmp(h+36,"data",4)){fclose(f);return -1;}
 uint32_t bytes=h[40]|((uint32_t)h[41]<<8)|((uint32_t)h[42]<<16)|((uint32_t)h[43]<<24);
 int ch=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,1024,PSP_AUDIO_FORMAT_STEREO);if(ch<0){fclose(f);return -1;}
 static int16_t samples[2048] __attribute__((aligned(64)));int rc=0;
 pspDebugScreenClear();print("Playing actual transition\n\nSTART or O stops playback.\n");release_buttons();
 while(running&&bytes){uint32_t n=bytes>4096?4096:bytes;memset(samples,0,sizeof(samples));if(fread(samples,1,n,f)!=n){rc=-1;break;}
  if(sceAudioOutputBlocking(ch,PSP_AUDIO_VOLUME_MAX,samples)<0){rc=-1;break;}bytes-=n;scePowerTick(0);unsigned hit=buttons();if(hit&(PSP_CTRL_START|PSP_CTRL_CIRCLE))break;
 }
 while(sceAudioGetChannelRestLen(ch)>0)sceKernelDelayThread(1000);
 sceAudioChRelease(ch);fclose(f);release_buttons();return rc;
}
static void paths(void){snprintf(a,sizeof(a),"%s/music/%s",base,songs[track[0]]);snprintf(b,sizeof(b),"%s/music/%s",base,songs[track[1]]);}
static int unique_output(void){for(int i=1;i<=9999;i++){snprintf(out,sizeof(out),"%s/exports/MIX%04d.WAV",base,i);SceIoStat st;if(sceIoGetstat(out,&st)<0)return 0;}return -1;}
int main(int argc,char **argv){
 (void)argc;(void)argv;pspDebugScreenInit();sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
 int th=sceKernelCreateThread("Callbacks",callbacks,0x11,4096,0,0);if(th>=0)sceKernelStartThread(th,0,0);
 if(!getcwd(base,sizeof(base))){snprintf(base,sizeof(base),"ms0:/PSP/GAME/CIRCUITFX");}size_t len=strlen(base);if(len&&base[len-1]=='/')base[len-1]=0;
 char folder[1024];snprintf(folder,sizeof(folder),"%s/music",base);sceIoMkdir(folder,0777);snprintf(folder,sizeof(folder),"%s/exports",base);sceIoMkdir(folder,0777);
 snprintf(preview,sizeof(preview),"%s/preview.tmp.wav",base);remove(preview);scan();int row=0;
 while(running){
  pspDebugScreenClear();print("CIRCUIT FX STUDIO                  BETA 1 / 0.6\n\n");
  print("%s SONG A: %.49s\n",row==0?">":" ",track[0]<0?"Select WAV":songs[track[0]]);
  print("%s SONG B: %.49s\n",row==1?">":" ",track[1]<0?"Select WAV":songs[track[1]]);
  print("%s TRANSITION: %s\n",row==2?">":" ",curves[curve]);
  print("%s DURATION / OVERLAP: %d seconds\n",row==3?">":" ",duration);
  print("%s PREVIEW actual junction\n",row==4?">":" ");
  print("%s APPROVE AND EXPORT complete songs\n",row==5?">":" ");
  print("\nUP/DOWN: row   LEFT/RIGHT: change   X: select\nSQUARE: replay preview   SELECT: rescan music\nHOME: exit\n\n");
  print("4 transition curves | WAV input / WAV output\nPreview: %s\n\n%.65s\n",ready?"ready":"needed",status);
  unsigned hit=buttons();if(hit&PSP_CTRL_UP)row=(row+5)%6;if(hit&PSP_CTRL_DOWN)row=(row+1)%6;
  if(hit&PSP_CTRL_SELECT){invalidate();track[0]=track[1]=-1;scan();snprintf(status,sizeof(status),"Found %d WAV files.",count);}
  if(row==2&&(hit&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_CROSS))){curve=(curve+((hit&PSP_CTRL_LEFT)?3:1))%4;invalidate();}
  if(row==3&&(hit&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_CROSS))){duration+=(hit&PSP_CTRL_LEFT)?-1:1;if(duration<1)duration=1;if(duration>120)duration=120;invalidate();}
  if((hit&PSP_CTRL_CROSS)&&row<2){int selected=choose(row);if(selected>=0){track[row]=selected;invalidate();}release_buttons();}
  if((hit&PSP_CTRL_CROSS)&&row==4){
   if(track[0]<0||track[1]<0)snprintf(status,sizeof(status),"Choose both songs first.");
   else{invalidate();paths();release_buttons();pspDebugScreenClear();print("Rendering preview...\n");
    if(render_mix(a,b,preview,duration,curve,1,progress))snprintf(status,sizeof(status),"%s",render_error);
    else{ready=1;if(play())snprintf(status,sizeof(status),"Playback failed. Preview exists; try replay.");else snprintf(status,sizeof(status),"Preview ready. Select export when satisfied.");}release_buttons();}
  }
  if(hit&PSP_CTRL_SQUARE){if(ready){if(play())snprintf(status,sizeof(status),"Preview playback failed.");}else snprintf(status,sizeof(status),"Generate a preview first.");}
  if((hit&PSP_CTRL_CROSS)&&row==5){
   if(!ready)snprintf(status,sizeof(status),"Preview your transition before exporting.");
   else{release_buttons();pspDebugScreenClear();print("APPROVE EXPORT?\n\n%s\nOverlap: %d seconds\n\nComplete song A + transition + complete song B\nThe overlap is shared; originals are preserved.\n\nX: approve and export\nO: back\n",curves[curve],duration);
    approved=0;while(running){unsigned p=buttons();if(p&PSP_CTRL_CIRCLE)break;if(p&PSP_CTRL_CROSS){approved=1;break;}sceKernelDelayThread(20000);}release_buttons();
    if(approved&&running){paths();if(unique_output())snprintf(status,sizeof(status),"Export folder has 9999 mixes.");else{pspDebugScreenClear();print("Exporting full songs...\nKeep memory stick inserted.\n");
     if(render_mix(a,b,out,duration,curve,0,progress))snprintf(status,sizeof(status),"%s",render_error);else snprintf(status,sizeof(status),"Saved exports/%s",strrchr(out,'/')+1);release_buttons();}approved=0;}
   }
  }
  sceDisplayWaitVblankStart();sceKernelDelayThread(30000);
 }
 remove(preview);sceKernelExitGame();return 0;
}
