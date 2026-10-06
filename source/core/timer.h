#pragma once
// Per-stage profiling. The spec asks for per-frame render time and optional
// per-stage timings (parsing, BVH construction, shading, ...), all written to
// the log file.
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace cgr {

using Clock = std::chrono::steady_clock;

double ElapsedSeconds(Clock::time_point a, Clock::time_point b);

class ScopeTimer {
  public:
    explicit ScopeTimer(const std::string& stage);
    ~ScopeTimer();
    void Stop();
    double Elapsed() const { return ElapsedSeconds(start, end); }

  private:
    std::string stage;
    Clock::time_point start;
    Clock::time_point end;
    bool stopped = false;
};

class Profiler {
  public:
    static Profiler& Instance();
    void Add(const std::string& stage, double seconds);
    void Report();
    void Reset();
    double Get(const std::string& stage) const;

  private:
    Profiler() = default;
    std::map<std::string, double> stages;
};

}  // namespace cgr
