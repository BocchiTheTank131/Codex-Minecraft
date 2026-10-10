"""Create labeled overhead previews from --biome-audit output (Pillow only)."""
from pathlib import Path
import argparse,csv,re
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args()
names=[row['biome'] for row in csv.DictReader((args.directory/'distribution.csv').open())]
font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',16)
for biome_map in args.directory.glob('*-biomes.ppm'):
    seed=biome_map.stem.split('-')[0]
    panel=Image.new('RGB',(1440,970),(24,28,34));draw=ImageDraw.Draw(panel)
    for name,x,y,side in [('biomes',20,50,640),('temperature',690,50,320),('humidity',1060,50,320),('terrain',690,410,320)]:
        image=Image.open(args.directory/(seed+'-'+name+'.ppm')).resize((side,side),Image.Resampling.NEAREST)
        panel.paste(image,(x,y));draw.text((x,y-24),name.upper(),font=font,fill='white')
    draw.text((20,15),'Seed '+seed+' | Generator 9 | 30,720 x 30,720 blocks | North at top',font=font,fill='white')
    draw.text((1060,410),'Temperature / humidity:',font=font,fill='white')
    draw.text((1060,440),'Green = 0; red = 1',font=font,fill='white')
    draw.text((1060,480),'Terrain: darker = lower',font=font,fill='white')
    for i,name in enumerate(names):
        x=20+(i//12)*470;y=754+(i%12)*17
        color=(45+(i*83)%180,45+(i*47)%180,45+(i*131)%180)
        draw.rectangle((x,y,x+12,y+12),fill=color);draw.text((x+20,y-3),name,font=font,fill='white')
    panel.save(args.directory/(seed+'-overview.png'))
    print(args.directory/(seed+'-overview.png'))
