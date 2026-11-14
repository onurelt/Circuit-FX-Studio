"""Prove bundled samples are actually audible, pitch-preserved and exported."""
import array, os, pathlib, subprocess, tempfile, wave
ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE = str(ROOT / 'tests/render')
RATE = 44100

def pcm(path):
    with wave.open(str(path), 'rb') as w:
        return array.array('h', w.readframes(w.getnframes()))

with tempfile.TemporaryDirectory() as temp:
    p = pathlib.Path(temp)
    for name in ['a', 'b']:
        with wave.open(str(p / (name + '.wav')), 'wb') as w:
            w.setparams((2, 2, RATE, 0, 'NONE', 'not compressed'))
            w.writeframes(bytes(RATE * 18 * 4))
    def render(id, name, duration=8, preview=False, intensity=1, speed=1, reverse=0, cwd=ROOT, ok=True, cancel=False):
        env = dict(os.environ, PRESET_TEST=str(id), INTENSITY_TEST=str(intensity), SPEED_TEST=str(speed), REVERSE_TEST=str(reverse))
        if cancel: env['CANCEL_TEST'] = '1'
        out = p / (name + '.wav')
        result = subprocess.run([EXE,str(p/'a.wav'),str(p/'b.wav'),str(out),'6','1',str(duration),'1',str(int(preview))], cwd=cwd, env=env, capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr
        if not ok:
            assert not out.exists()
            return
        return pcm(out)
    for id in range(65,90):
        full = render(id,str(id))
        source = pcm(ROOT / f'assets/sfx/{id-64:02}.wav')
        count = len(source)//2
        start = (8*RATE-count) if 73<=id<=78 else (8*RATE-count)//2
        # Every original WAV frame appears at natural speed with the documented
        # level and 10ms edges. Float-to-int rounding is allowed one LSB.
        for frame in range(count):
            level = .32 * min(1,frame/441,(count-1-frame)/441)
            for ch in [0,1]:
                actual = full[(6*RATE+start+frame)*2+ch]
                expected = source[frame*2+ch]*level
                assert abs(actual-expected)<1.1, (id,frame,actual,expected)
        assert max(map(abs,full))>100, ('inaudible sample',id)
        assert not any(render(id,f'{id}-zero',intensity=0))
        excerpt=render(id,f'{id}-preview',preview=True)
        assert excerpt==full[2*RATE*2:18*RATE*2]
        short=render(id,f'{id}-short',duration=.1)
        assert any(short), id
        if id in [65,73,78,80,81,82,83,84,85,86,87,88,89]:
            for speed, reverse in [(0.5,0),(1,1),(1.5,1),(2,0)]:
                altered=render(id,f'{id}-{speed}-{reverse}',speed=speed,reverse=reverse)
                source_count=min(count,int(8*RATE*speed))
                source_offset=count-source_count if 73<=id<=78 else 0
                count_out=min(8*RATE,int(__import__('math').ceil(source_count/speed)))
                start_alt=(8*RATE-count_out) if 73<=id<=78 else (8*RATE-count_out)//2
                for out_frame in [min(500,count_out-1),count_out//3,count_out//2,count_out-1]:
                    srcpos=min(source_count-1,out_frame*speed)
                    if reverse: srcpos=source_count-1-srcpos
                    srcpos += source_offset
                    lo=int(srcpos);hi=min(count-1,lo+1);frac=srcpos-lo
                    level=.32*min(1,out_frame/441,(count_out-1-out_frame)/441)
                    for ch in [0,1]:
                        expected=(source[lo*2+ch]*(1-frac)+source[hi*2+ch]*frac)*level
                        actual=altered[(6*RATE+start_alt+out_frame)*2+ch]
                        assert abs(actual-expected)<1.2,(id,speed,reverse,out_frame,actual,expected)
    for id in range(90,97):
        data=render(id,f'{id}-variant',reverse=1)
        assert max(map(abs,data))>100
        assert data!=render(id-9,f'{id}-base')
    render(65,'missing',cwd=p,ok=False)
    (p/'assets/sfx').mkdir(parents=True)
    (p/'assets/sfx/01.wav').write_bytes(b'damaged')
    render(65,'damaged',cwd=p,ok=False)
    render(65,'cancel',cancel=True,ok=False)
    render(65,'bad-speed',speed=.2,ok=False)
    render(65,'bad-reverse',reverse=2,ok=False)
print('PASS: 25 real WAVs, natural-speed sample identity, silence-input audibility, zero intensity, preview/export equality, 100ms crops, missing/corrupt files and cancellation.')
