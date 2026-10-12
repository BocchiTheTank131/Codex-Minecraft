#pragma once
#include <algorithm>
// A single refreshable hazard, independent from contact damage invulnerability.
struct Burning {
    double remaining=0, elapsed=0;
    void clear(){remaining=elapsed=0;}
    int update(float dt,bool lava,bool water) {
        if(water){clear();return 0;}
        if(lava) remaining=5.f;
        const double step=std::min(double(std::max(dt,0.f)),remaining);
        elapsed+=step; remaining=std::max(0.0,remaining-step);
        const int damage=int(elapsed+1e-8); elapsed=std::max(0.0,elapsed-double(damage));
        if(remaining==0) elapsed=0;
        return damage;
    }
};
