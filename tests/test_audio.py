import pathlib, tempfile, wave, struct, subprocess, hashlib
EXE=str(pathlib.Path(__file__).with_name('render').resolve())
def wav(p,frames,value,rate=44100):
 with wave.open(str(p),'wb') as w:
  w.setparams((2,2,rate,0,'NONE','not compressed'))
  block=struct.pack('<hh',value,value)*44100
  while frames:
   n=min(frames,44100);w.writeframesraw(block[:n*4]);frames-=n

def run(a,b,o,seconds=1,curve=0,preview=0,ok=True):
 r=subprocess.run([EXE,str(a),str(b),str(o),str(seconds),str(curve),str(preview)],capture_output=True)
 assert (r.returncode==0)==ok,r.stderr

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);a=p/'a.wav';b=p/'b.wav';wav(a,44100*6,12000);wav(b,44100*7,-8000)
 hashes=digest(a),digest(b)
 for curve in range(4):
  o=p/f'curve{curve}.wav';run(a,b,o,2,curve)
  with wave.open(str(o),'rb') as w:
   assert w.getnframes()==44100*11
   raw=w.readframes(w.getnframes());assert raw[:44100*4*4]==struct.pack('<hh',12000,12000)*44100*4
   assert raw[-44100*5*4:]==struct.pack('<hh',-8000,-8000)*44100*5
   assert struct.unpack_from('<h',raw,44100*4*4)[0]==12000
   assert struct.unpack_from('<h',raw,(44100*6-1)*4)[0]==-8000
  saved=digest(o);run(a,b,o,ok=False);assert digest(o)==saved
 assert hashes==(digest(a),digest(b))
 o=p/'preview.wav';run(a,b,o,2,2,1)
 with wave.open(str(o),'rb') as w:assert w.getnframes()==44100*10
 bad=p/'bad.wav';wav(bad,44100,100,48000);run(bad,b,p/'invalid.wav',ok=False);assert not (p/'invalid.wav').exists()
 bad.write_bytes(a.read_bytes()[:-20]);run(bad,b,p/'truncated.wav',ok=False)
 run(a,b,p/'too_long.wav',120,ok=False)
 run(a,b,a,ok=False);assert hashes==(digest(a),digest(b))
 # Full 7+7 minute render using fixed processing buffers.
 wav(a,44100*420,1234);wav(b,44100*420,-2345);run(a,b,p/'long.wav',16,1)
 with wave.open(str(p/'long.wav'),'rb') as w:assert w.getnframes()==44100*824
 print('PASS: four curves, untouched prefixes/tails, endpoints, preview, overwrite refusal, invalid/truncated WAV rejection, excessive overlap, 13m44s full export.')
