# Optional regeneration; bundled font.h means the build has no Pillow dependency.
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
font=ImageFont.truetype('/usr/share/fonts/truetype/open-sans/OpenSans-Regular.ttf',18)
widths=[];data=[]
for c in range(32,127):
 im=Image.new('L',(24,26));ImageDraw.Draw(im).text((0,-1),chr(c),font=font,fill=255)
 widths.append(round(font.getlength(chr(c))));data.extend(im.tobytes())
Path('vita/font.h').write_text('static const unsigned char widths[95]={'+','.join(map(str,widths))+'};\nstatic const unsigned char glyphs[95*24*26]={'+','.join(map(str,data))+'};\n')
