/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_DISCLOSURE_HPP
#define CPPWAMP_DISCLOSURE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the facilities for managing caller/publisher disclosure. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Determines how callers and publishers are disclosed. */
//------------------------------------------------------------------------------
enum class Disclosure
{
    preset,   ///< Disclose as per the realm configuration preset.
    producer, ///< Disclose as per the producer's `disclose_me` option.
    consumer, /**< Disclose if the callee requested disclosure when registering
                   (ignored for subscriptions) */
    either,   /**< Disclose if either the producer or the consumer
                   requested disclosure. */
    both,     /**< Disclose if both the originator and the consumer
                   requested disclosure. */
    reveal,   ///< Disclose even if disclosure was not requested.
    conceal   ///< Don't disclose even if disclosure was requested.
};

} // namespace wamp

#endif // CPPWAMP_DISCLOSURE_HPP
