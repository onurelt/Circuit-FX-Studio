#include "wav.h"
static void u16(FILE *f,unsigned v){ fputc(v&255,f); fputc((v>>8)&255,f); }
static void u32(FILE *f,uint32_t v){ u16(f,v&65535); u16(f,v>>16); }
int wav_begin(FILE *f,uint32_t rate){
 if(!f||!rate)return -1;
 fwrite("RIFF",1,4,f);u32(f,0);fwrite("WAVEfmt ",1,8,f);u32(f,16);
 u16(f,1);u16(f,2);u32(f,rate);u32(f,rate*4);u16(f,4);u16(f,16);
 fwrite("data",1,4,f);u32(f,0);return ferror(f)?-1:0;
}
int wav_append(FILE *f,const int16_t *s,uint32_t frames){
 if(!f||!s)return -1;
 for(uint32_t i=0;i<frames*2;i++)u16(f,(uint16_t)s[i]);
 return ferror(f)?-1:0;
}
int wav_finish(FILE *f){
 if(!f)return -1;
 long end=ftell(f);if(end<44|| (unsigned long)end>0xFFFFFFFFUL)return -1;
 if(fseek(f,4,SEEK_SET))return -1;
 u32(f,(uint32_t)end-8);
 if(fseek(f,40,SEEK_SET))return -1;
 u32(f,(uint32_t)end-44);
 return fflush(f)==0?0:-1;
}
