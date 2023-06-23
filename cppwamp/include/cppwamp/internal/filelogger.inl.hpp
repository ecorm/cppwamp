/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../utils/filelogger.hpp"
#include <fstream>
#include "../api.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE FileLoggerOptions&
FileLoggerOptions::withTruncate(bool truncate)
{
    truncate_ = truncate;
    return *this;
}

CPPWAMP_INLINE FileLoggerOptions&
FileLoggerOptions::withFlushOnWrite(bool flushOnWrite)
{
    flushOnWrite_ = flushOnWrite;
    return *this;
}

CPPWAMP_INLINE bool FileLoggerOptions::truncate() const
{
    return truncate_;
}

CPPWAMP_INLINE bool FileLoggerOptions::flushOnWrite() const
{
    return flushOnWrite_;
}

//------------------------------------------------------------------------------
struct FileLogger::Impl
{
    Impl(const std::string& filepath, std::string originLabel,
         FileLoggerOptions options)
        : origin(std::move(originLabel)),
          file(filepath.c_str(), modeFlags(options.truncate())),
          options(options)
    {}

    static std::ios_base::openmode modeFlags(bool truncate)
    {
        return truncate ? std::ios_base::trunc : std::ios_base::app;
    }

    std::string origin;
    std::ofstream file;
    FileLoggerOptions options;
};

CPPWAMP_INLINE FileLogger::FileLogger(const std::string& filepath,
                                      FileLoggerOptions options)
    : FileLogger(filepath, "cppwamp", options)
{}

CPPWAMP_INLINE FileLogger::FileLogger(const std::string& filepath,
                                      std::string originLabel,
                                      FileLoggerOptions options)
    : impl_(std::make_shared<Impl>(filepath, std::move(originLabel), options))
{}

CPPWAMP_INLINE void FileLogger::operator()(const LogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(impl.file, entry, impl.origin) << "\n";
    if ((entry.severity() >= LogLevel::warning) || impl.options.flushOnWrite())
        impl.file << std::flush;
}

CPPWAMP_INLINE void FileLogger::operator()(const AccessLogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(impl.file, entry) << "\n";
    if (impl.options.flushOnWrite())
        impl.file << std::flush;
}

} // namespace utils

} // namespace wamp
