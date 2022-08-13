/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_BLOB_HPP
#define CPPWAMP_BLOB_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of Variant and other closely related
           types/functions. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <vector>
#include "api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains binary data as an array of bytes. */
//------------------------------------------------------------------------------
class CPPWAMP_API Blob
{
public:
    /// Array of bytes used to contain the binary data.
    using Data = std::vector<uint8_t>;

    /** Default constructor. */
    Blob();

    /** Constructor taking a vector of bytes. */
    explicit Blob(Data data);

    /** Constructor taking an initializer list. */
    explicit Blob(std::initializer_list<uint8_t> list);

    /** Obtains the blob's array of bytes. */
    Data& data();

    /** Obtains the blob's constant array of bytes. */
    const Data& data() const;

    /** Equality comparison. */
    bool operator==(const Blob& other) const;

    /** Inequality comparison. */
    bool operator!=(const Blob& other) const;

    /** Less-than comparison. */
    bool operator<(const Blob& other) const;

private:
    std::vector<uint8_t> data_;
};

//------------------------------------------------------------------------------
/** Outputs a Blob to the given output stream.
    @relates Blob */
//------------------------------------------------------------------------------
CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Blob& blob);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/blob.ipp"
#endif

#endif // CPPWAMP_BLOB_HPP
