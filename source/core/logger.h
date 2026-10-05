#pragma once
// Tiny logging facility. The spec requires that every parse/render run writes
// a .log file next to the output image, recording warnings and errors with
// descriptive messages instead of crashing.
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace cgr {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
  public:
    static Logger& Instance();

    // Opens `path` for writing, truncating. Safe to call again to redirect.
    void Open(const std::string& path);
    void Close();

    void Write(LogLevel level, const std::string& msg);
    void Debug(const std::string& m) { Write(LogLevel::Debug, m); }
    void Info(const std::string& m) { Write(LogLevel::Info, m); }
    void Warn(const std::string& m) { Write(LogLevel::Warn, m); }
    void Error(const std::string& m) { Write(LogLevel::Error, m); }
    // Record a non-fatal problem, e.g. an unsupported directive or a missing
    // texture. Also echoed to stdout so the console session is useful.
    void Problem(const std::string& where, const std::string& msg);

    // Streaming form: LOG(Logger::Warn) << "..." << 42;
    class Line {
      public:
        Line(LogLevel l) : level(l) {}
        ~Line() { Logger::Instance().Write(level, os.str()); }
        template <typename T>
        Line& operator<<(const T& v) {
            os << v;
            return *this;
        }

      private:
        LogLevel level;
        std::ostringstream os;
    };

    int NumWarnings() const { return nWarn; }
    int NumErrors() const { return nError; }
    void ResetCounts() { nWarn = nError = 0; }
    // Also mirror every record to stdout.
    void SetEcho(bool e) { echo = e; }
    const std::vector<std::string>& Records() const { return records; }

  private:
    Logger() = default;
    std::ofstream out;
    std::mutex mtx;
    int nWarn = 0, nError = 0;
    bool echo = true;
    std::string path;
    std::vector<std::string> records;
};

}  // namespace cgr

#define LOG_CGR(level) ::cgr::Logger::Line(level)
#define LOGD(msg) ::cgr::Logger::Instance().Debug(msg)
#define LOGI(msg) ::cgr::Logger::Instance().Info(msg)
#define LOGW(msg) ::cgr::Logger::Instance().Warn(msg)
#define LOGE(msg) ::cgr::Logger::Instance().Error(msg)
