"""Append original relic-tool pixel art; preserve every historical source cell."""
from pathlib import Path
import re
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[1]
text=(root/'include/Survival.h').read_text(encoding='utf-8').split('enum class Item')[1].split('};')[0]
text=re.sub(r'//[^\n]*','',text).split('{',1)[1]
names=[v.strip().split('=')[0].strip() for v in text.split(',') if v.strip()]
names.remove('Count')
old=Image.open(root/'assets/inventory_atlas_16.png').convert('RGBA')
atlas=Image.new('RGBA',(160,((len(names)+9)//10)*16));atlas.paste(old,(0,0))
icons={}
compass=Image.new('RGBA',(16,16));d=ImageDraw.Draw(compass)
d.polygon([(5,1),(10,1),(14,5),(14,10),(10,14),(5,14),(1,10),(1,5)],fill='#8a602e',outline='#362b25')
d.ellipse((3,3,12,12),fill='#d2b570',outline='#fee0a0');d.ellipse((4,4,11,11),fill='#274855')
d.polygon([(8,3),(10,9),(7,8),(5,11)],fill='#e5f9ed');d.polygon([(7,8),(10,9),(8,13)],fill='#db5547')
icons['SurveyorCompass']=compass
lantern=Image.new('RGBA',(16,16));d=ImageDraw.Draw(lantern)
d.arc((5,0,10,6),180,360,fill='#e9cd8d',width=2);d.rectangle((3,4,12,13),fill='#435663',outline='#1d2d3b')
d.rectangle((5,6,10,11),fill='#50b8c5');d.polygon([(8,5),(10,9),(7,12),(6,8)],fill='#c4fff0')
d.line((3,5,12,5),fill='#c1c5a5');d.line((3,13,12,13),fill='#d1ae68');d.rectangle((6,13,9,14),fill='#725a38')
icons['EchoLantern']=lantern
pick=Image.new('RGBA',(16,16));d=ImageDraw.Draw(pick)
d.line((4,13,11,5),fill='#382a2a',width=4);d.line((4,13,11,5),fill='#ad804e',width=2)
d.polygon([(3,2),(9,1),(13,3),(14,7),(12,8),(10,5),(5,5)],fill='#203d4a',outline='#15272d')
d.line([(4,2),(9,2),(12,4),(13,6)],fill='#9ee3d7',width=2);d.rectangle((7,2,8,4),fill='#e8bf65')
icons['AncientExplorerTool']=pick
core=Image.new('RGBA',(16,16));d=ImageDraw.Draw(core)
d.polygon([(4,2),(11,2),(14,7),(11,13),(4,13),(1,7)],fill='#7b6843',outline='#292d31')
d.polygon([(6,3),(10,4),(12,8),(8,12),(4,9),(4,5)],fill='#4c9ca2',outline='#d7b66a')
d.line([(8,4),(9,7),(6,9),(8,11)],fill='#d2fff1',width=2)
icons['ResonantCore']=core
for name,icon in icons.items():
    index=names.index(name);atlas.paste(icon,((index%10)*16,(index//10)*16))
    icon.save(root/'assets/inventory'/('relic_'+name.lower()+'.png'))
for index in range(names.index('SurveyorCompass')):
    x,y=index%10*16,index//10*16;box=(x,y,x+16,y+16)
    assert old.crop(box).tobytes()==atlas.crop(box).tobytes(),index
atlas.save(root/'assets/inventory_atlas_16.png')
print('Three relic tools and one core appended; all historical source pixels preserved')
