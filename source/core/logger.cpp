#include "core/logger.h"

#include <cstdio>
#include <iostream>

namespace cgr {

namespace {
const char* LevelName(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}
}  // namespace

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

void Logger::Open(const std::string& p) {
    std::lock_guard<std::mutex> lock(mtx);
    if (out.is_open()) out.close();
    path = p;
    out.open(p, std::ios::out | std::ios::trunc);
    nWarn = nError = 0;
    records.clear();
}

void Logger::Close() {
    std::lock_guard<std::mutex> lock(mtx);
    if (out.is_open()) out.close();
}

void Logger::Write(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    if (level == LogLevel::Warn) ++nWarn;
    if (level == LogLevel::Error) ++nError;
    std::string line = std::string("[") + LevelName(level) + "] " + msg;
    records.push_back(line);
    if (out.is_open()) {
        out << line << "\n";
        out.flush();
    }
    if (echo) {
        if (level == LogLevel::Warn)
            std::cerr << line << std::endl;
        else
            std::cout << line << std::endl;
    }
}

void Logger::Problem(const std::string& where, const std::string& msg) {
    Write(LogLevel::Error, where + ": " + msg);
}

}  // namespace cgr
