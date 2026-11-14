# Optional regeneration: Pillow + Open Sans. All raster fonts are bundled.
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
out=[]
for name,size,weight in [('small',13,'Regular'),('body',17,'Regular'),('title',23,'Semibold'),('large',30,'Semibold')]:
 font=ImageFont.truetype('/usr/share/fonts/truetype/open-sans/OpenSans-'+weight+'.ttf',size)
 width=size+5;height=size+10;data=[];widths=[]
 for c in range(32,127):
  im=Image.new('L',(width,height));ImageDraw.Draw(im).text((0,-2),chr(c),font=font,fill=255)
  widths.append(round(font.getlength(chr(c))));data.extend(im.tobytes())
 out.append('static const unsigned char '+name+'_widths[95]={'+','.join(map(str,widths))+'};')
 out.append('static const unsigned char '+name+'_glyphs[]={'+','.join(map(str,data))+'};')
 out.append('static const Font '+name+'={'+f'{width},{height},'+name+'_widths,'+name+'_glyphs};')
Path('vita/font.h').write_text('\n'.join(out)+'\n')
