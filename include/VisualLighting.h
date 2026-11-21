#pragma once
// Shared GLSL 330 artistic lighting. Textures remain in their existing color space.
#define VOXEL_VISUAL_LIGHTING R"GLSL(
// Frame-constant colors are calculated once on the CPU and uploaded together.
uniform vec3 uVisualColors[3];
vec3 visualSunTint(vec3 sun){return uVisualColors[0];}
vec3 visualTint(vec3 sun,float day){return uVisualColors[1];}
vec3 visualHorizon(vec3 sun,float day,float rain,float storm){
 return mix(uVisualColors[2],mix(vec3(.29,.33,.37),vec3(.10,.13,.18),storm),rain*.7);
}
vec3 visualResponse(vec3 normal,float sky,float block,float day,vec3 sun,vec4 weather,float brightness){
 float d=smoothstep(0.0,1.0,day),shade=.84+max(normal.y,0.0)*.16-max(-normal.y,0.0)*.14;
 float raw=clamp(max(sky*(mix(.055,.48,d)+max(dot(normal,sun),0.0)*.54*d)*(1.0-.25*weather.x-.55*weather.y)+sky*max(dot(normal,-sun),0.0)*(1.0-d)*.17,block*.9),0.0,1.0);
 float floorLight=mix(.025,.085,brightness);
 float response=pow(clamp(floorLight+(1.0-floorLight)*pow(raw,mix(1.0,.72,brightness))*shade,0.0,1.0),.75);
 vec3 tint=visualTint(sun,day);
 if(block>.02)tint=mix(tint,vec3(1.0,.78,.52),smoothstep(.02,.5,block)*(1.0-clamp(sky*d/max(block,.0001),0.0,1.0)));
 return response*tint+vec3(1.0,.60,.28)*block*.14;
}
float visualHaze(float distance,float farDistance,float sky,int quality,vec4 weather){
 if(sky<=.02||(quality==0&&weather.w*(weather.x+weather.y)<.001))return 0.0;
 float clear=quality==0?0.0:mix(.08,.14,float(quality-1));
 float fog=clear+weather.w*(weather.x*.32+weather.y*.18);
 return clamp(fog*smoothstep(max(24.0,farDistance*.25),max(48.0,farDistance),distance)*smoothstep(.02,.4,sky),0.0,.65);
}
vec3 visualWaterFog(vec3 color,float distance,float blend,float day){
 if(blend<.001)return color;
 vec3 water=mix(vec3(.013,.06,.09),vec3(.035,.22,.27),day);
 return mix(color*mix(vec3(1),vec3(.76,.94,1),blend),water,(1.0-exp(-distance*.075))*blend);
}
)GLSL"
