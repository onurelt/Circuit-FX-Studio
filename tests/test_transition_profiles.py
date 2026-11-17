"""Proof that Auto Mix options render different AUDIO at identical cues.

This does not establish subjective DJ quality or real PS Vita performance.
"""
import array
import math
import os
from pathlib import Path
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
RENDER = ROOT / "tests/render"
RATE = 44100

def song(path, bass, upper, is_vocal=False):
    samples = array.array("h")
    for i in range(RATE):
        t = i / RATE
        kick = 7000 * math.sin(2*math.pi*75*t) * math.exp(-25*(t % .5))
        low = 6800 * math.sin(2*math.pi*bass*t)
        bright = 4300 * math.sin(2*math.pi*upper*t)
        vocal = 1800 * math.sin(2*math.pi*(300+12*math.sin(2*math.pi*2*t))*t) if is_vocal else 0
        s = max(-32000, min(32000, int(kick+low+bright+vocal)))
        samples.append(s)
        samples.append(int(s*.93))
    with wave.open(str(path), "wb") as w:
        w.setparams((2,2,RATE,0,"NONE","not compressed"))
        w.writeframes(samples.tobytes()*26)

def render(path_a,path_b,out,style,fx,preview=False,stem_a=None,stem_b=None):
    env=dict(os.environ,MIX_STYLE_TEST=str(style))
    if stem_a and stem_b:
        env.update(A_STEM_TEST=str(stem_a),B_STEM_TEST=str(stem_b))
    cmd=[str(RENDER),str(path_a),str(path_b),str(out),"14","2","8",str(fx),str(int(preview))]
    result=subprocess.run(cmd,env=env,capture_output=True,text=True)
    assert result.returncode==0, (cmd,result.stderr)

def read(path):
    with wave.open(str(path),"rb") as w:
        assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(2,2,RATE)
        return w.readframes(w.getnframes())

def samples(raw):
    x=array.array("h")
    x.frombytes(raw)
    return x

def rms(x):
    return math.sqrt(sum(float(v)*v for v in x[::64])/max(1,len(x[::64])))

with tempfile.TemporaryDirectory() as td:
    p=Path(td)
    a,b=p/"a.wav",p/"b.wav"
    song(a,110,4000,True)
    song(b,142,6000,False)
    profiles=[("smooth",1,9),("energy",2,11),("echo-fallback",3,3)]
    clips={}
    for name,style,fx in profiles:
        full=p/(name+".wav")
        preview=p/(name+"-preview.wav")
        render(a,b,full,style,fx)
        render(a,b,preview,style,fx,preview=True)
        entire=read(full)
        excerpt=read(preview)
        # Pre is 14 seconds; preview begins 10 s earlier and includes
        # 10 s after the 8 s overlap. Export must be sample-identical.
        assert len(excerpt)==28*RATE*4, (name,len(excerpt))
        assert excerpt==entire[4*RATE*4:(4+28)*RATE*4],name
        clips[name]=samples(entire[14*RATE*4:22*RATE*4])

    for i,(name,_,_) in enumerate(profiles):
        sig=clips[name][::64]
        assert max(abs(x) for x in sig)>500,name
        for name2,_,_ in profiles[i+1:]:
            sig2=clips[name2][::64]
            rel=rms([x-y for x,y in zip(sig,sig2)])/max(1,rms(sig))
            assert rel>.12,(name,name2,rel)
            print(f"{name} vs {name2}: relative waveform change {rel:.3f}")

    # Validate the third profile uses actually supplied source stems, never
    # a fabricated vocal-only DSP result.
    av,binst=p/"a-vocals.wav",p/"b-instrumental.wav"
    song(av,0,490,True)
    song(binst,105,3500,False)
    with_stems=p/"vocal-stems.wav"
    render(a,b,with_stems,3,3,stem_a=av,stem_b=binst)
    voices=samples(read(with_stems)[14*RATE*4:22*RATE*4])[::64]
    no_stems=clips["echo-fallback"][::64]
    change=rms([x-y for x,y in zip(voices,no_stems)])/max(1,rms(no_stems))
    assert change>.12,change
    print(f"Prepared-stem transition vs fallback: relative waveform change {change:.3f}")
    print("PASS: distinct DSP at SAME cues, prepared stems, and preview/export audio identity.")
