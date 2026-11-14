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
    def render(id, name, duration=8, preview=False, intensity=1, cwd=ROOT, ok=True, cancel=False):
        env = dict(os.environ, PRESET_TEST=str(id), INTENSITY_TEST=str(intensity))
        if cancel: env['CANCEL_TEST'] = '1'
        out = p / (name + '.wav')
        result = subprocess.run([EXE,str(p/'a.wav'),str(p/'b.wav'),str(out),'6','1',str(duration),'1',str(int(preview))], cwd=cwd, env=env, capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr
        if not ok:
            assert not out.exists()
            return
        return pcm(out)
    for id in range(65,81):
        full = render(id,str(id))
        source = pcm(ROOT / f'assets/sfx/{id-64:02}.wav')
        count = len(source)//2
        start = (8*RATE-count) if 73<=id<=76 else (8*RATE-count)//2
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
        assert any(short)
    render(65,'missing',cwd=p,ok=False)
    (p/'assets/sfx').mkdir(parents=True)
    (p/'assets/sfx/01.wav').write_bytes(b'damaged')
    render(65,'damaged',cwd=p,ok=False)
    render(65,'cancel',cancel=True,ok=False)
print('PASS: 16 real WAVs, natural-speed sample identity, silence-input audibility, zero intensity, preview/export equality, 100ms crops, missing/corrupt files and cancellation.')
