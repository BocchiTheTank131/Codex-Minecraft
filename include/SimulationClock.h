#pragma once
#include <algorithm>
#include <cmath>
#include "WorldTime.h"
// 120 Hz simulation. At most 32 steps/render; retain up to two seconds of debt.
// Sustained overload beyond that drops excess explicitly, avoiding an unbounded
// spiral. Ordinary 10+ FPS and isolated one-second spikes lose no simulation time.
struct SimulationClock {
    static constexpr double Step=1.0/120.0;
    double debt=0, dropped=0;
    unsigned advance(double elapsed) {
        if(!std::isfinite(elapsed)||elapsed<0)return 0;
        debt+=elapsed;
        if(debt>2){dropped+=debt-2;debt=2;}
        const auto steps=unsigned(std::min(32.0,std::floor((debt+1e-9)/Step)));
        debt=std::max(0.0,debt-steps*Step);return steps;
    }
    void reset(){debt=0;}
};

// Preserve the float command/rendering interface without rounding the running
// clock on every 120 Hz tick. Detect direct /time assignments before advancing.
struct SimulationTime {
    double exact=WorldTime::DefaultSeconds;
    float published=WorldTime::DefaultSeconds;
    void advance(float& value,double elapsed){
        if(!std::isfinite(value)||value<0)value=WorldTime::DefaultSeconds;
        if(value!=published)exact=value;
        exact+=elapsed;value=static_cast<float>(exact);published=value;
    }
};
