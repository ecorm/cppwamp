/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../exceptions.hpp"
#include <sstream>
#include "../api.hpp"

namespace wamp
{

namespace error
{

//------------------------------------------------------------------------------
// error::Failure exception
//------------------------------------------------------------------------------

CPPWAMP_INLINE Failure::Failure(std::error_code ec)
    : std::system_error(ec, makeMessage(ec))
{}

CPPWAMP_INLINE Failure::Failure(std::error_code ec, const std::string& info)
    : std::system_error(ec, makeMessage(ec, info))
{}

CPPWAMP_INLINE std::string Failure::makeMessage(std::error_code ec)
{
    std::ostringstream oss;
    oss << "error::Failure: \n"
           "    error code = " << ec << "\n"
           "    message = \"" << ec.message() << "\"\n";
    return oss.str();
}

CPPWAMP_INLINE std::string Failure::makeMessage(std::error_code ec,
                                                const std::string& info)
{
    return makeMessage(ec) + "    info = \"" + info + "\"\n";
}

//------------------------------------------------------------------------------
// error::Logic exception
//------------------------------------------------------------------------------

/** @details
    The @ref CPPWAMP_LOGIC_ERROR macro should be used instead, which will
    conveniently fill in the `file` and `line` details. */
CPPWAMP_INLINE void Logic::raise(
    const char* file,      ///< The source file where the exception is raised
    int line,              ///< The source line where the exception is raised
    const std::string& msg ///< Describes the cause of the exception
)
{
    std::ostringstream oss;
    oss << file << ':' << line << ", wamp::error::Logic: " << msg;
    throw Logic(oss.str());
}

/** @details
    This function is intended for asserting preconditions.
    The @ref CPPWAMP_LOGIC_CHECK macro should be used instead, which will
    conveniently fill in the `file` and `line` details. */
CPPWAMP_INLINE void Logic::check(
    bool condition,        ///< If `true`, then an exception will be thrown
    const char* file,      ///< The source file where the exception is raised
    int line,              ///< The source line where the exception is raised
    const std::string& msg ///< Describes the cause of the exception
)
{
    if (!condition)
        raise(file, line, msg);
}

//------------------------------------------------------------------------------
// error::BadType exception and its subclasses
//------------------------------------------------------------------------------

CPPWAMP_INLINE BadType::BadType(const std::string& what)
    : std::runtime_error(what)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Access::Access(const std::string& what)
    : BadType("wamp::error::Access: " + what)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Access::Access(const std::string& from, const std::string& to)
    : Access("Attemping to access field type " + from + " as " + to)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Conversion::Conversion(const std::string& what)
    : BadType("wamp::error::Conversion: " + what)
{}

} // namespace error

} // namespace wamp
