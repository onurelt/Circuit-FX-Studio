/* Compact, touch-first dashboard. All curves are illustrative previews of
 * the five existing mixer effects; they do not invent a curve parameter. */
static void flat_button(int x,int y,int w,int h,const char *label,
                        uint32_t color,int selected) {
  roundbox(x,y,w,h,7,selected?blend(BG,color,87):RGB(27,39,56),
           selected?blend(BG,color,54):RGB(18,27,41));
  txt(x+10,y+(h-19)/2,&body,selected?WHITE:color,label,w-18);
}
static unsigned char curve_cache[6][2][40];
static int curves_ready;
static void prepare_curves(void) {
  if (curves_ready) return;
  for (int kind=0;kind<6;kind++) for (int ch=0;ch<2;ch++)
    for (int i=0;i<40;i++) {
      float t=i/39.f,gain=ch?t:1-t;
      if (kind==1) gain=ch?t:expf(-3*t)*(0.75f+0.25f*cosf(t*32));
      if (kind==2) gain=ch?t*t:1-t*t;
      if (kind==3) gain=ch? t<.5f?.25f:.9f : t<.5f?.9f:.25f;
      if (kind==4) gain=ch? t<.5f?.05f:1.f : t<.5f?1.f:.05f;
      if (kind==5) gain=ch?.3f:.9f;
      curve_cache[kind][ch][i]=(unsigned char)(fmaxf(0,fminf(1,gain))*255);
    }
  curves_ready=1;
}
static void curve_card(int x,int y,int w,int h,int kind,uint32_t a,
                       uint32_t b) {
  prepare_curves();
#ifndef HOST_UI
  vita2d_color_vertex *vertices=ui_vertex_batch(2*2*40);
  int n=0;
#endif
  for (int i=1;i<40;i++) for (int ch=0;ch<2;ch++) {
    float t0=(i-1)/39.f,t1=i/39.f;
    int x0=x+(int)(t0*w),x1=x+(int)(t1*w);
    int y0=y+(255-curve_cache[kind][ch][i-1])*(h-3)/255;
    int y1=y+(255-curve_cache[kind][ch][i])*(h-3)/255;
#ifdef HOST_UI
    rect(x0,y0,2,2,ch?b:a);
    rect(x1,y1,2,2,ch?b:a);
#else
    if (vertices) {
      vertices[n++]=(vita2d_color_vertex){x0,y0,UI_VERTEX_Z,ch?b:a};
      vertices[n++]=(vita2d_color_vertex){x1,y1,UI_VERTEX_Z,ch?b:a};
    } else vita2d_draw_line(x0,y0,x1,y1,ch?b:a);
#endif
  }
#ifndef HOST_UI
  if (vertices) vita2d_draw_array(SCE_GXM_PRIMITIVE_LINES,vertices,n);
#endif
}
void ui_draw(uint32_t *pixels, const View *v) {
  fb=pixels;
  rect(0,0,960,544,BG);
  char s[180],a[32];
  /* Compact file selectors: art is intentionally omitted until real album
   * artwork loading exists. The entire bar opens the existing file browser. */
  for (int d=0;d<2;d++) {
    int x=10+d*480;
    uint32_t c=d?PINK:CYAN;
    roundbox(x,5,460,39,6,RGB(24,35,52),RGB(15,25,39));
    roundbox(x+4,9,34,31,5,c,blend(c,BG,45));
    txt(x+13,12,&title,BG,d?"B":"A",22);
    txt(x+49,8,&small,c,d?"TRACK B":"TRACK A",86);
    txt(x+49,23,&body,WHITE,v->songs[d]?v->songs[d]:"Select song",285);
    tc(a,sizeof(a),v->lengths[d],0);
    txt(x+363,8,&small,MUTED,v->songs[d]?a:"OPEN",85);
    if (v->songs[d]) {
      if (v->track_bpm[d]>0)
        snprintf(s,sizeof(s),"%.1f BPM%s",v->track_bpm[d],v->bpm_manual[d]?"*":"");
      else snprintf(s,sizeof(s),"BPM ...");
      txt(x+354,25,&small,c,s,101);
    }
  }
  /* 266px timeline area: two vertically generous, aligned waveforms. */
  rect(10,50,940,265,RGB(14,25,39));
  double span=v->wave_window>0?v->wave_window:45;
  double pan=v->wave_window>0?v->wave_pan:-22.5;
  double tempo_ratio=v->auto_beatmatch && v->track_bpm[0]>0 &&
                     v->track_bpm[1]>0 ?
                     v->track_bpm[0]/v->track_bpm[1] : 1;
  int cue_x[2],end_x[2];
  for (int d=0;d<2;d++) {
    double cue=d?v->settings.start_b:v->settings.start_a;
    cue_x[d]=54+(int)((cue-v->wave_origin[d]-pan)*880/span);
    end_x[d]=cue_x[d]+(int)(v->settings.duration*(d?tempo_ratio:1)*880/span);
    if (cue_x[d]<934 && end_x[d]>54) {
      int left=cue_x[d]<54?54:cue_x[d],right=end_x[d]>934?934:end_x[d];
      if (right>left) rect(left,d?195:79,right-left,115,RGB(42,30,64));
    }
  }
  for (int k=0;k<=8;k++) {
    int xx=54+k*110;
    rect(xx,80,1,230,RGB(30,48,62));
    double at=v->wave_origin[0]+pan+span*k/8.0;
    tc(a,sizeof(a),at,0);
    txt(xx+2,56,&small,MUTED,a,54);
  }
  rect(54,193,880,1,EDGE);
  for (int d=0;d<2;d++) {
    int y=d?253:135;
    uint32_t c=d?PINK:CYAN;
    txt(16,y-14,&large,c,d?"B":"A",34);
    if (v->lengths[d]>0) draw_wave(v,d,pan,span,y,c);
    else txt(350,y-8,&body,MUTED,d?"Select Track B above":"Select Track A above",270);
    if (cue_x[d]>=54 && cue_x[d]<934 && v->songs[d]) {
      rect(cue_x[d],y-51,2,103,WHITE);
      roundbox(cue_x[d]-4,y+48,10,7,3,c,c);
    }
  }
  for (int d=0;d<2;d++)
    if (end_x[d]>=54 && end_x[d]<934)
      rect(end_x[d],d?195:79,2,115,RGB(203,177,235));
  if (cue_x[0]>=54 && cue_x[0]<934) {
    tc(a,sizeof(a),v->settings.duration,1);
    snprintf(s,sizeof(s),"OVERLAP %s",a);
    txt(cue_x[0]+8,79,&small,WHITE,s,185);
  }
  if (v->playing) {
    double at=v->settings.start_a-ui_timeline_pre(v)+v->play_position;
    int xx=54+(int)((at-v->wave_origin[0]-pan)*880/span);
    if (xx>=54 && xx<934) rect(xx,80,2,229,RGB(230,247,255));
  }
  /* Live actions and precise time editors immediately below the timeline. */
  flat_button(10,318,102,46,v->playing?"STOP":"PREVIEW",CYAN,v->row==10);
  flat_button(118,318,60,46,"STOP",WHITE,0);
  flat_button(184,318,74,46,"REPLAY",WHITE,0);
  const char *labels[]={"A CUE","B CUE","DURATION"};
  double values[]={v->settings.start_a,v->settings.start_b,v->settings.duration};
  for (int i=0;i<3;i++) {
    int x=264+126*i;
    roundbox(x,318,120,46,6,RGB(28,44,61),RGB(18,30,45));
    txt(x+8,320,&small,i==1?PINK:CYAN,labels[i],100);
    tc(a,sizeof(a),values[i],1);
    txt(x+8,339,&small,WHITE,a,104);
  }
  flat_button(642,318,58,46,"-",CYAN,0);
  flat_button(706,318,58,46,"+",CYAN,0);
  flat_button(770,318,54,46,"FIT",CYAN,0);
  flat_button(830,318,120,46,"EXPORT",PINK,v->row==11);
  const char *tabs[]={"TRANSITIONS","EFFECTS","PRESETS"};
  for (int i=0;i<3;i++)
    flat_button(10+316*i,371,310,39,tabs[i],i==0?CYAN:i==1?PINK:WHITE,v->tab==i);
  const char *fx[]={"Crossfade","Echo Out","Filter Sweep","Bass Swap","Quick Cut","Vocal Lite"};
  const char *desc[]={"Blend A out, B in","Echo from A fades out","Filter sweeps the mix",
                     "Swap low frequencies","Fast 20ms handoff"};
  if (v->tab==0 || v->tab==1) {
    int selected=v->tab==0?v->effect_index:v->effect_index;
    if (selected<0 || selected>(v->tab==0?4:5)) selected=0;
    int cards=v->tab==0?5:6;
    for (int i=0;i<cards;i++) {
      int width=v->tab==0?180:148,x=14+(v->tab==0?188:155)*i;
      int active=i==5?v->vocal_enabled:(v->settings.effects&(1u<<i))!=0;
      roundbox(x,420,width,v->tab==0?84:70,7,
               i==selected || (i<5 && v->row==i+5)?
               RGB(31,75,85):RGB(25,38,56),RGB(14,24,39));
      curve_card(x+10,428,width-21,v->tab==0?43:34,i,CYAN,PINK);
      txt(x+8,v->tab==0?477:468,&body,WHITE,fx[i],width-26);
      if (v->tab==1) txt(x+width-25,468,&small,active?CYAN:MUTED,active?"ON":"OFF",26);
    }
    if (v->tab==0) {
      txt(18,513,&body,WHITE,fx[selected],155);
      txt(172,515,&small,MUTED,desc[selected],421);
      flat_button(620,507,320,35,
                  v->auto_beatmatch?"AUTO BEATMATCH  ON":"AUTO BEATMATCH  OFF",
                  v->auto_beatmatch?CYAN:MUTED,v->auto_beatmatch);
    } else {
      int active=selected==5?v->vocal_enabled:
                 (v->settings.effects&(1u<<selected))!=0;
      flat_button(14,497,136,42,active?"ENABLED":"DISABLED",active?CYAN:MUTED,active);
      txt(165,507,&small,MUTED,selected==5?"VOCAL STRENGTH":"GLOBAL INTENSITY",168);
      flat_button(340,497,52,42,"-",CYAN,0);
      if (selected==5) snprintf(s,sizeof(s),"%d%%",v->vocal_strength);
      else snprintf(s,sizeof(s),"%.0f%%",v->settings.intensity*100);
      txt(405,509,&body,WHITE,s,65);
      flat_button(466,497,52,42,"+",CYAN,0);
      if (selected==5) {
        const char *targets[]={"TRACK A","TRACK B","TRANSITION","BOTH TRACKS"};
        snprintf(s,sizeof(s),"TARGET: %s",targets[v->vocal_target%4]);
        flat_button(550,497,390,42,s,PINK,0);
      } else if (v->auto_beatmatch)
        txt(560,509,&small,MUTED,"FX BPM follows Track A",355);
      else if (v->settings.preset_id) {
        txt(547,507,&small,MUTED,"FX BPM",125);
        flat_button(710,497,52,42,"-",PINK,0);
        snprintf(s,sizeof(s),"%.0f",v->settings.bpm);
        txt(784,509,&body,WHITE,s,83);
        flat_button(872,497,52,42,"+",PINK,0);
      } else txt(560,509,&small,MUTED,"BPM applies to presets",355);
    }
  } else {
    for (int i=0;i<8;i++) {
      int x=14+i*116;
      roundbox(x,416,109,27,5,i==v->preset_bank?RGB(64,40,81):RGB(27,39,56),
               RGB(17,27,43));
      txt(x+5,421,&small,i==v->preset_bank?PINK:WHITE,preset_banks[i],98);
    }
    for (int i=0;i<5;i++) {
      int id=v->preset_bank*16+v->preset_page*5+i+1;
      if (id>(v->preset_bank+1)*16) break;
      TransitionPreset p;
      if (preset_get(id,&p)) continue;
      int x=14+i*188;
      roundbox(x,448,180,58,6,id==v->selected_preset || v->row==i+5?RGB(67,39,84):RGB(27,39,56),
               RGB(17,27,42));
      snprintf(s,sizeof(s),"%02d  %s",id,p.sample_id?"WAV SOUND":"DJ RECIPE");
      txt(x+8,451,&small,p.sample_id?CYAN:MUTED,s,162);
      txt(x+8,471,&body,WHITE,p.name,163);
    }
    flat_button(14,510,55,31,"<",PINK,0);
    if (v->selected_preset>0 && v->selected_preset<=PRESET_COUNT) {
      TransitionPreset p;
      preset_get(v->selected_preset,&p);
      snprintf(s,sizeof(s),"%s  /  %d beats%s",p.name,p.beats,
               p.sample_id?"  /  WAV sound":"");
      txt(83,519,&small,WHITE,s,380);
    }
    if (v->auto_beatmatch)
      txt(485,519,&small,CYAN,"FX BPM = TRACK A",220);
    else {
      txt(472,519,&small,MUTED,"BPM",57);
      flat_button(563,510,42,31,"-",PINK,0);
      snprintf(s,sizeof(s),"%.0f",v->settings.bpm);
      txt(613,519,&small,WHITE,s,43);
      flat_button(665,510,42,31,"+",PINK,0);
    }
    flat_button(722,510,157,31,"DETAIL / APPLY",CYAN,0);
    flat_button(884,510,55,31,">",PINK,0);
  }
  if (v->status[0] && v->progress<0 && !v->confirm && !v->browser &&
      !v->preset_browser && !v->editing) {
    rect(54,293,880,18,RGB(15,25,39));
    txt(60,295,&small,MUTED,v->status,868);
  }
