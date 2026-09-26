#include "Renderer.h"
#include <algorithm>
#include <array>
#include <string>
#include <vector>
namespace {
struct V{glm::vec2 p;glm::vec4 c;};
void rect(std::vector<V>&v,float x,float y,float w,float h,glm::vec4 c,int W,int H){float l=x/W*2-1,r=(x+w)/W*2-1,t=1-y/H*2,b=1-(y+h)/H*2;v.insert(v.end(),{{{l,t},c},{{l,b},c},{{r,b},c},{{l,t},c},{{r,b},c},{{r,t},c}});}
const std::array<unsigned char,7>& digit(char c){static const std::array<std::array<unsigned char,7>,12>d={{{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},{0,0,0,31,0,0,0},{0,0,0,0,0,0,0}}};if(c>='0'&&c<='9')return d[c-'0'];if(c=='-')return d[10];return d[11];}
void number(std::vector<V>&v,const std::string&s,float x,float y,float scale,glm::vec4 c,int W,int H){for(char ch:s){auto&r=digit(ch);for(int yy=0;yy<7;++yy)for(int xx=0;xx<5;++xx)if(r[yy]&(1<<(4-xx)))rect(v,x+xx*scale,y+yy*scale,scale,scale,c,W,H);x+=6*scale;}}
void icon(std::vector<V>&v,const ItemStack&s,float x,float y,float size,int W,int H){if(s.empty())return;glm::vec3 c=itemColor(s.item);if(isTool(s.item)){rect(v,x+size*.45f,y+size*.28f,size*.12f,size*.58f,{.42f,.25f,.10f,1},W,H);rect(v,x+size*.18f,y+size*.18f,size*.64f,size*.18f,{c,1},W,H);}else if(s.item==Item::Seeds){for(int i=0;i<3;++i)rect(v,x+size*(.26f+i*.16f),y+size*(.3f+i*.08f),size*.12f,size*.28f,{c,1},W,H);}else rect(v,x+size*.22f,y+size*.22f,size*.56f,size*.56f,{c,1},W,H);
 if(s.count>1){std::string n=std::to_string(s.count);number(v,n,x+size-4-n.size()*6,y+size-11,1,{1,1,1,1},W,H);}int max=Inventory::maxDurability(s.item);if(max>0){float f=std::clamp((float)s.durability/max,0.f,1.f);rect(v,x+5,y+size-6,size-10,3,{.08f,.08f,.08f,1},W,H);rect(v,x+5,y+size-6,(size-10)*f,3,{1-f,f,.08f,1},W,H);}}
void slot(std::vector<V>&v,const ItemStack&s,float x,float y,float z,int W,int H,bool selected=false){if(selected)rect(v,x-3,y-3,z+6,z+6,{.96f,.96f,.96f,1},W,H);rect(v,x,y,z,z,{.08f,.08f,.10f,.96f},W,H);rect(v,x+3,y+3,z-6,z-6,{.30f,.30f,.33f,.96f},W,H);icon(v,s,x,y,z,W,H);}
void flush(std::vector<V>& vertices,GLuint vbo,GLuint vao,GLuint program){glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(vertices.size()*sizeof(V)),vertices.data(),GL_STREAM_DRAW);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(program);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,(GLsizei)vertices.size());glBindVertexArray(0);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);}
}
void Renderer::renderSurvivalUi(int W,int H,const Inventory&inv,float hunger,float xp,int level,bool fullbright)const{
 std::vector<V>v;float slotSize=44,gap=4,total=9*slotSize+8*gap,start=(W-total)*.5f,y=static_cast<float>(H)-58.f;
 for(int i=0;i<9;++i){const auto&s=inv.slot(i);int max=Inventory::maxDurability(s.item);if(max>0){float f=std::clamp((float)s.durability/max,0.f,1.f);rect(v,start+i*(slotSize+gap)+5,y+slotSize-6,(slotSize-10)*f,3,{1-f,f,.05f,1},W,H);}}
 for(int i=0;i<10;++i){float filled=std::clamp(hunger-i*2.f,0.f,2.f)/2.f;float x=start+total-14-i*18,hy=y-20;rect(v,x,hy,14,11,{.13f,.08f,.02f,.9f},W,H);if(filled>0)rect(v,x+2,hy+2,10*filled,7,{.92f,.60f,.12f,1},W,H);}
 rect(v,start,y-9,total,5,{.03f,.04f,.03f,.9f},W,H);rect(v,start+2,y-7,(total-4)*std::clamp(xp,0.f,1.f),2,{.28f,.95f,.18f,1},W,H);number(v,std::to_string(level),W*.5f-5,y-23,1.5f,{.45f,1,.28f,1},W,H);if(fullbright){rect(v,static_cast<float>(W)-38.f,12.f,25.f,20.f,{1,.86f,.22f,.92f},W,H);number(v,"1",static_cast<float>(W)-28.f,16.f,1.5f,{.12f,.1f,.02f,1},W,H);}flush(v,uiVbo_,uiVao_,uiProgram_);
}
void Renderer::renderInventory(int W,int H,const Inventory&inv,bool table,double mx,double my)const{
 std::vector<V>v;float px=W*.5f-325,py=H*.5f-280,s=44,g=4,start=px+100;rect(v,px,py,650,560,{.08f,.075f,.07f,.97f},W,H);rect(v,px+10,py+10,630,540,{.34f,.34f,.34f,.98f},W,H);
 for(int y=0;y<3;++y)for(int x=0;x<9;++x)slot(v,inv.slot(9+y*9+x),start+x*(s+g),py+300+y*(s+g),s,W,H);
 for(int x=0;x<9;++x)slot(v,inv.slot(x),start+x*(s+g),py+470,s,W,H,x==inv.selectedSlot());
 int n=table?3:2;for(int y=0;y<n;++y)for(int x=0;x<n;++x)slot(v,inv.craftSlot(y*3+x),px+70+x*(s+g),py+80+y*(s+g),s,W,H);
 rect(v,px+278,py+140,34,6,{.72f,.72f,.72f,1},W,H);rect(v,px+302,py+132,10,22,{.72f,.72f,.72f,1},W,H);slot(v,inv.craftingOutput(table),px+330,py+125,s,W,H);
 number(v,table?"3":"2",px+70,py+48,2,{.95f,.9f,.74f,1},W,H);icon(v,inv.cursorStack(),(float)mx-22,(float)my-22,44,W,H);flush(v,uiVbo_,uiVao_,uiProgram_);
}
