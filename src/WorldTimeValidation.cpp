#include "WorldTime.h"
#include <stdexcept>
#include <algorithm>

bool WorldTime::selfTest(std::string& report) {
    try {
        const auto check=[](bool ok,const char* what){if(!ok)throw std::runtime_error(what);};
        for(float old:{0.f,35.f,105.f,210.f,245.f,290.f,315.f,419.99f,840.f}) {
            std::istringstream legacy("\nWEATHER1 2 179 .8 .6 .2 5 42\nFIRES1 0\n");
            const float now=restore(old,legacy);
            check(std::abs(now/CycleSeconds-std::fmod(old,LegacyCycleSeconds)/LegacyCycleSeconds)<.00001f,"Legacy time phase migration");
            std::istringstream saved("\nWEATHER1 2 179 .8 .6 .2 5 42\nFIRES1 0\nTIME_CYCLE1 1200\n");
            check(std::abs(restore(now,saved)-now)<.001f,"Time was scaled twice after reload");
        }
        check(std::abs(std::sin(angle(NoonSeconds))-1.f)<.00001f &&
              std::abs(std::sin(angle(MidnightSeconds))+1.f)<.00001f,"Noon/midnight solar alignment");
        check(wrap(-30)==1170 && wrap(1230)==30,"Time numeric wrapping");
        std::istringstream corrupt("\nTIME_CYCLE1 nan\nTIME_CYCLE1 -1\n");
        check(std::abs(restore(35,corrupt)-DefaultSeconds)<.001f,"Invalid cycle metadata fallback");
        // Complete cycles in 0.1-second simulation steps. Positive/negative sun
        // elevation occupy exactly equal halves, including the existing offset.
        for(int cycle=0;cycle<3;++cycle) {
            int day=0,night=0;
            for(int i=0;i<12000;++i) {
                const float seconds=cycle*CycleSeconds+i*.1f;
                const float phase=angle(seconds);
                if(std::sin(phase)>=0)++day;else ++night;
                const float sun=std::sin(phase);
                const float d=std::clamp((sun+.18f)/.40f,0.f,1.f);
                check(std::isfinite(d) && d>=0 && d<=1,"Sky transition range");
            }
            check(day==6000&&night==6000,"600-second day/night halves");
        }
        report="20-minute time tests passed: three complete cycles, 600-second halves, legacy phase migration, reload, numeric wrapping and metadata fallback";
        return true;
    } catch(const std::exception& e) {report=e.what();return false;}
}
