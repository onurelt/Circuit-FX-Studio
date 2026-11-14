"""Exercise every shipped recipe with real renderer output, not just preset counts."""
import wave,struct,math,tempfile,pathlib,subprocess,hashlib,os
exe=os.environ.get('RENDER_EXE',str(pathlib.Path('tests/render').resolve()))
rate=44100
with tempfile.TemporaryDirectory() as directory:
 p=pathlib.Path(directory)
 for deck,seconds in [('a',4),('b',5)]:
  with wave.open(str(p/(deck+'.wav')),'wb') as w:
   w.setparams((2,2,rate,0,'NONE','not compressed'))
   for second in range(seconds):
    data=bytearray()
    for i in range(rate):
     t=second+i/rate;f=173 if deck=='a' else 287
     l=int(7000*math.sin(2*math.pi*f*t)+4000*math.sin(2*math.pi*59*t)+2000*math.sin(2*math.pi*4100*t))
     r=int(6000*math.cos(2*math.pi*(f+30)*t)+3000*math.sin(2*math.pi*83*t))
     data.extend(struct.pack('<hh',l,r))
    w.writeframesraw(data)
 def render(id,name,bpm=128,intensity=1,preview=0,ok=True,cancel=False):
  env=os.environ.copy();env.update(PRESET_TEST=str(id),BPM_TEST=str(bpm),INTENSITY_TEST=str(intensity))
  if cancel:env['CANCEL_TEST']='1'
  out=p/name;r=subprocess.run([exe,str(p/'a.wav'),str(p/'b.wav'),str(out),'1','.5','1.5','1',str(preview)],env=env,capture_output=True)
  assert (r.returncode==0)==ok,(id,r.stderr.decode())
  if not ok:assert not out.exists();return
  with wave.open(str(out),'rb') as w:
   assert w.getnframes()==int(5.5*rate);return w.readframes(w.getnframes())
 with wave.open(str(p/'a.wav'),'rb') as w:a=w.readframes(w.getnframes())
 with wave.open(str(p/'b.wav'),'rb') as w:b=w.readframes(w.getnframes())
 signatures=set()
 for id in range(1,65):
  data=render(id,f'{id}.wav')
  assert data[:rate*4]==a[:rate*4]
  assert data[int(2.5*rate)*4:]==b[2*rate*4:]
  assert data[rate*4:rate*4+4]==a[rate*4:rate*4+4],(id,'start boundary')
  assert data[int(2.5*rate-1)*4:int(2.5*rate)*4]==b[int(2*rate-1)*4:int(2*rate)*4],(id,'end boundary')
  signatures.add(hashlib.sha256(data).hexdigest())
  if id in [8,10,14,16,30,46,64]:
   assert render(id,f'{id}-again.wav',preview=1)==data,'preview must equal excerpt with deterministic FX'
   assert render(id,f'{id}-weak.wav',intensity=.2)!=data
 # Same duration and input: not merely duplicate names or duration variants.
 assert len(signatures)==64,len(signatures)
 for bpm in [60,200]:render(30,f'bpm{bpm}.wav',bpm=bpm)
 for id in [-1,65]:render(id,f'bad{id}.wav',ok=False)
 for bpm in [0,59,201,'nan','inf']:render(1,'badbpm.wav',bpm=bpm,ok=False)
 for intensity in [-.1,1.1,'nan']:render(1,'badintensity.wav',intensity=intensity,ok=False)
 render(64,'cancel.wav',ok=False,cancel=True)
 print('PASS: all 64 recipes produce distinct audio; exact endpoints; preview identity; intensity; BPM boundaries; invalid settings; cancellation.')
