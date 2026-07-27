#pragma once

// Central logging facility.
//
// Every subsystem logs through here so that a play session can be reconstructed
// after the fact. Messages go to three places at once:
//   * stdout (developer convenience)
//   * logs/HorizonUnseen.log (post-mortem debugging)
//   * an in-memory ring buffer that the in-game debug console renders
//
// Use the HU_LOG_* macros rather than calling Log::write directly; they capture
// the source location and compile the formatting away when the level is off.

#include <cstdarg>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace hu {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Count
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
    double timeSeconds = 0.0;
    unsigned long long frame = 0;
};

class Log {
public:
    // Opens logs/HorizonUnseen.log and records a session banner. Safe to call
    // more than once; later calls are ignored.
    static void init(const std::string& logDirectory = "logs");
    static void shutdown();

    // Messages below this level are dropped entirely.
    static void setLevel(LogLevel level);
    static LogLevel getLevel();

    // Per-category suppression, so a noisy subsystem can be muted at runtime
    // from the debug console without recompiling.
    static void setCategoryEnabled(const std::string& category, bool enabled);
    static bool isCategoryEnabled(const std::string& category);

    // The frame counter is stamped onto every entry, which makes it possible to
    // line up logs from different subsystems within a single frame.
    static void beginFrame(unsigned long long frameIndex, double timeSeconds);
    static unsigned long long currentFrame();
    static double currentTime();

    static void write(LogLevel level, const char* category, const char* file, int line, const char* fmt, ...);
    static void writeV(LogLevel level, const char* category, const char* file, int line, const char* fmt, va_list args);

    // Snapshot of the ring buffer for the debug console.
    static std::deque<LogEntry> snapshot();
    static void clearHistory();
    static void setHistoryLimit(std::size_t limit);

    static const char* levelName(LogLevel level);

private:
    Log() = default;
};

} // namespace hu

// The do/while(0) wrapper keeps these usable as single statements inside an
// unbraced if/else.
#define HU_LOG(level, category, ...) \
    do { ::hu::Log::write((level), (category), __FILE__, __LINE__, __VA_ARGS__); } while (0)

#define HU_LOG_TRACE(category, ...) HU_LOG(::hu::LogLevel::Trace, category, __VA_ARGS__)
#define HU_LOG_DEBUG(category, ...) HU_LOG(::hu::LogLevel::Debug, category, __VA_ARGS__)
#define HU_LOG_INFO(category, ...)  HU_LOG(::hu::LogLevel::Info,  category, __VA_ARGS__)
#define HU_LOG_WARN(category, ...)  HU_LOG(::hu::LogLevel::Warn,  category, __VA_ARGS__)
#define HU_LOG_ERROR(category, ...) HU_LOG(::hu::LogLevel::Error, category, __VA_ARGS__)

// Logs and continues when an invariant is violated. Deliberately not fatal:
// a broken assumption should be visible in the log rather than crash a play test.
#define HU_CHECK(cond, category, ...) \
    do { if (!(cond)) { HU_LOG_ERROR(category, __VA_ARGS__); } } while (0)
