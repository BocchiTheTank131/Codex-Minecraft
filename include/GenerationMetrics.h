#pragma once
#include <array>
#include <chrono>
#include <cstdint>

// Opt-in per-caller profiling. Normal workers have no clock reads or shared counters.
namespace GenerationMetrics {
enum Stage { Columns, Plans, Entrances, Lattice, Density, Materials, Vegetation, Structures, Shore, Count };
struct Sample { std::array<double,Count> milliseconds{}; std::uint64_t heightQueries=0,heightHits=0; };
inline thread_local Sample* active=nullptr;
class Capture {
    Sample* previous_;
public:
    explicit Capture(Sample& sample):previous_(active){active=&sample;}
    ~Capture(){active=previous_;}
};
class Scope {
    using Clock=std::chrono::steady_clock;
    Sample* sample_; Stage stage_; Clock::time_point start_;
public:
    explicit Scope(Stage stage):sample_(active),stage_(stage) {if(sample_)start_=Clock::now();}
    void next(Stage stage) {if(sample_){auto now=Clock::now();sample_->milliseconds[stage_]+=std::chrono::duration<double,std::milli>(now-start_).count();start_=now;stage_=stage;}}
    ~Scope(){if(sample_)sample_->milliseconds[stage_]+=std::chrono::duration<double,std::milli>(Clock::now()-start_).count();}
};
}
