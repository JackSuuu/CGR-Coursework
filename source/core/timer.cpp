#include "core/timer.h"

#include <iomanip>
#include <sstream>

#include "core/logger.h"

namespace cgr {

double ElapsedSeconds(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::duration<double>>(b - a).count();
}

ScopeTimer::ScopeTimer(const std::string& s) : stage(s), start(Clock::now()) {
    end = start;
}
ScopeTimer::~ScopeTimer() {
    end = Clock::now();
    Profiler::Instance().Add(stage, Elapsed());
}

Profiler& Profiler::Instance() {
    static Profiler inst;
    return inst;
}

void Profiler::Add(const std::string& stage, double seconds) {
    stages[stage] += seconds;
}

double Profiler::Get(const std::string& stage) const {
    auto it = stages.find(stage);
    return it == stages.end() ? 0.0 : it->second;
}

void Profiler::Reset() {
    stages.clear();
    raysTested = trisTested = 0;
}

void Profiler::Report() {
    std::ostringstream os;
    os << "---- profile ----\n";
    for (const auto& kv : stages) {
        std::ostringstream l;
        l << "  " << std::left << std::setw(18) << kv.first << std::fixed
          << std::setprecision(4) << kv.second << " s";
        os << l.str() << "\n";
        Logger::Instance().Write(LogLevel::Info, l.str());
    }
    std::ostringstream c;
    c << "  rays traced        " << std::fixed << std::setprecision(0) << raysTested
      << "  (primitive tests " << std::fixed << std::setprecision(0) << trisTested << ")";
    os << c.str();
    Logger::Instance().Write(LogLevel::Info, c.str());
}

}  // namespace cgr
