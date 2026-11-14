"""Keep Vita launcher art as 8-bit indexed PNG, even with only a few colors."""
from pathlib import Path
from PIL import Image
for path in Path('assets').glob('*.png'):
    image=Image.open(path)
    if image.mode!='P':
        raise ValueError(f'{path}: expected indexed art')
    palette=image.getpalette()
    # Pillow may choose 1/2/4-bit output despite bits=8 for a short palette.
    image.putpalette(palette+[0]*(768-len(palette)))
    image.save(path,bits=8)
    assert path.read_bytes()[24:26]==b'\x08\x03',path
