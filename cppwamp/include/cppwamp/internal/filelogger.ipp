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
struct FileLogger::Impl
{
    Impl(const std::string& filepath, std::string originLabel, bool truncate)
        : origin(std::move(originLabel)),
          file(filepath.c_str(), modeFlags(truncate))
    {}

    static std::ios_base::openmode modeFlags(bool truncate)
    {
        return truncate ? std::ios_base::trunc : std::ios_base::app;
    }

    std::string origin;
    std::ofstream file;
    bool flushOnWrite;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE FileLogger::FileLogger(const std::string& filepath,
                                      bool truncate)
    : FileLogger(filepath, "cppwamp", truncate)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE FileLogger::FileLogger(const std::string& filepath,
                                      std::string originLabel, bool truncate)
    : impl_(std::make_shared<Impl>(filepath, std::move(originLabel), false))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void FileLogger::operator()(const LogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(impl.file, entry, impl.origin) << "\n";
    if ((entry.severity() >= LogLevel::warning) || impl.flushOnWrite)
        impl.file << std::flush;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void FileLogger::operator()(const AccessLogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(impl.file, entry, impl.origin) << "\n";
    if (impl.flushOnWrite)
        impl.file << std::flush;
}

} // namespace utils

} // namespace wamp
