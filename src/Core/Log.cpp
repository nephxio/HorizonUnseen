#include "Core/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace hu {
namespace {

struct LogState {
    std::mutex mutex;
    std::ofstream file;
    bool initialized = false;
    LogLevel level = LogLevel::Debug;
    std::unordered_map<std::string, bool> categoryEnabled;
    std::deque<LogEntry> history;
    std::size_t historyLimit = 4096;
    unsigned long long frame = 0;
    double time = 0.0;
};

// Function-local static: avoids static initialization order problems when a
// subsystem logs from a constructor during static init.
LogState& state() {
    static LogState s;
    return s;
}

const char* shortFileName(const char* path) {
    if (!path) {
        return "";
    }
    const char* last = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }
    return last;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buffer;
}

} // namespace

void Log::init(const std::string& logDirectory) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.initialized) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);

    const std::string path = logDirectory + "/HorizonUnseen.log";
    s.file.open(path, std::ios::out | std::ios::trunc);
    s.initialized = true;

    if (s.file.is_open()) {
        s.file << "=== Horizon Unseen session started " << timestamp() << " ===\n";
        s.file.flush();
    } else {
        std::fprintf(stderr, "[Log] Could not open %s for writing\n", path.c_str());
    }
}

void Log::shutdown() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.file.is_open()) {
        s.file << "=== Session ended " << timestamp() << " ===\n";
        s.file.flush();
        s.file.close();
    }
    s.initialized = false;
}

void Log::setLevel(LogLevel level) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.level = level;
}

LogLevel Log::getLevel() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.level;
}

void Log::setCategoryEnabled(const std::string& category, bool enabled) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.categoryEnabled[category] = enabled;
}

bool Log::isCategoryEnabled(const std::string& category) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    const auto it = s.categoryEnabled.find(category);
    return it == s.categoryEnabled.end() ? true : it->second;
}

void Log::beginFrame(unsigned long long frameIndex, double timeSeconds) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.frame = frameIndex;
    s.time = timeSeconds;
}

unsigned long long Log::currentFrame() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.frame;
}

double Log::currentTime() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.time;
}

void Log::write(LogLevel level, const char* category, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    writeV(level, category, file, line, fmt, args);
    va_end(args);
}

void Log::writeV(LogLevel level, const char* category, const char* file, int line, const char* fmt, va_list args) {
    LogState& s = state();

    // Check the filters before doing any formatting work.
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (level < s.level) {
            return;
        }
        if (category) {
            const auto it = s.categoryEnabled.find(category);
            if (it != s.categoryEnabled.end() && !it->second) {
                return;
            }
        }
    }

    // vsnprintf consumes the va_list, so copy it for the sizing pass.
    va_list sizingArgs;
    va_copy(sizingArgs, args);
    const int needed = std::vsnprintf(nullptr, 0, fmt, sizingArgs);
    va_end(sizingArgs);

    std::string message;
    if (needed > 0) {
        std::vector<char> buffer(static_cast<std::size_t>(needed) + 1);
        std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
        message.assign(buffer.data(), static_cast<std::size_t>(needed));
    }

    std::lock_guard<std::mutex> lock(s.mutex);

    LogEntry entry;
    entry.level = level;
    entry.category = category ? category : "";
    entry.message = message;
    entry.timeSeconds = s.time;
    entry.frame = s.frame;

    s.history.push_back(entry);
    while (s.history.size() > s.historyLimit) {
        s.history.pop_front();
    }

    char header[160];
    std::snprintf(header, sizeof(header), "[%8.3f][f%06llu][%-5s][%-10s] ",
                  s.time, s.frame, levelName(level), entry.category.c_str());

    std::FILE* console = (level >= LogLevel::Warn) ? stderr : stdout;
    std::fprintf(console, "%s%s\n", header, message.c_str());

    if (s.file.is_open()) {
        s.file << header << message
               << "  (" << shortFileName(file) << ':' << line << ")\n";
        // Flushed every line: a crash mid-session must not lose the tail of the
        // log, which is exactly the part worth reading.
        s.file.flush();
    }
}

std::deque<LogEntry> Log::snapshot() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.history;
}

void Log::clearHistory() {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.history.clear();
}

void Log::setHistoryLimit(std::size_t limit) {
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.historyLimit = limit == 0 ? 1 : limit;
    while (s.history.size() > s.historyLimit) {
        s.history.pop_front();
    }
}

const char* Log::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "?";
    }
}

} // namespace hu
