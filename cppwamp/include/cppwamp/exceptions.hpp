/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXCEPTIONS_HPP
#define CPPWAMP_EXCEPTIONS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides exception types. */
//------------------------------------------------------------------------------

#include <stdexcept>
#include <string>
#include <system_error>
#include "api.hpp"

//------------------------------------------------------------------------------
/** Throws an error::Logic exception having the given message string.
    @param msg A string describing the cause of the exception. */
//------------------------------------------------------------------------------
#define CPPWAMP_LOGIC_ERROR(msg) \
    error::Logic::raise(__FILE__, __LINE__, (msg));

//------------------------------------------------------------------------------
/** Conditionally throws an error::Logic exception having the given message
    string.
    @param cond A boolean expression that, if `true`, will cause an exception
                to be thrown.
    @param msg A string describing the cause of the exception. */
//------------------------------------------------------------------------------
#define CPPWAMP_LOGIC_CHECK(cond, msg) \
    {error::Logic::check((cond), __FILE__, __LINE__, (msg));}

namespace wamp
{

//******************************************************************************
// Exception Types
//******************************************************************************

namespace error
{

//------------------------------------------------------------------------------
/** General purpose runtime exception that wraps a std::error_code. */
//------------------------------------------------------------------------------
class CPPWAMP_API Failure : public std::system_error
{
public:
    /** Obtains a human-readable message from the given error code. */
    static std::string makeMessage(std::error_code ec);

    /** Obtains a human-readable message from the given error code and
        information string. */
    static std::string makeMessage(std::error_code ec, const std::string& info);

    /** Constructor taking an error code. */
    explicit Failure(std::error_code ec);

    /** Constructor taking an error code and informational string. */
    Failure(std::error_code ec, const std::string& info);
};


//------------------------------------------------------------------------------
/** Exception thrown when a pre-condition is not met. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Logic : public std::logic_error
{
    using std::logic_error::logic_error;

    /** Throws an error::Logic exception with the given details. */
    static void raise(const char* file, int line, const std::string& msg);

    /** Conditionally throws an error::Logic exception with the given
        details. */
    static void check(bool condition, const char* file, int line,
                      const std::string& msg);
};

//------------------------------------------------------------------------------
/** Base class for exceptions involving invalid Variant types. */
//------------------------------------------------------------------------------
struct CPPWAMP_API BadType : public std::runtime_error
{
    explicit BadType(const std::string& what);
};

//------------------------------------------------------------------------------
/** Exception type thrown when accessing a Variant as an invalid type. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Access : public BadType
{
    explicit Access(const std::string& what);
    Access(const std::string& from, const std::string& to);
};

//------------------------------------------------------------------------------
/** Exception type thrown when converting a Variant to an invalid type. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Conversion : public BadType
{
    explicit Conversion(const std::string& what);
};

} // namespace error

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/exceptions.ipp"
#endif

#endif // CPPWAMP_EXCEPTIONS_HPP
