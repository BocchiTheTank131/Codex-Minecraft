#pragma once
#include <algorithm>
struct VillageUiLayout {
 float x,y,width,height,rowHeight;
 VillageUiLayout(int w,int h){width=std::min(720.f,float(w)-24);height=std::min(480.f,float(h)-24);x=(w-width)*.5f;y=(h-height)*.5f;rowHeight=(height-190)/5;}
 int tab(double px,double py)const{return px>=x+16&&px<x+width-16&&py>=y+68&&py<y+96?std::clamp(int((px-x-16)/((width-32)/4)),0,3):-1;}
 int row(double px,double py,int count)const{if(px<x+16||px>=x+width-16||py<y+108||py>=y+108+rowHeight*count)return -1;return int((py-y-108)/rowHeight);}
};
