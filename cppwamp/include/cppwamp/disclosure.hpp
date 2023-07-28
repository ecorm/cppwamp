/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_DISCLOSURE_HPP
#define CPPWAMP_DISCLOSURE_HPP

#include "erroror.hpp"

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
    consumer, ///< Disclose if the consumer requested disclosure.
    either,   /**< Disclose if either the producer or the consumer
                   requested disclosure. */
    both,     /**< Disclose if both the originator and the consumer
                   requested disclosure. */
    reveal,   ///< Disclose even if disclosure was not requested.
    conceal   ///< Don't disclose even if disclosure was requested.
};

//------------------------------------------------------------------------------
/** Specifies the policy for handling caller and publisher disclosure. */
//------------------------------------------------------------------------------
class DisclosurePolicy
{
public:
    using Mode = Disclosure;

    DisclosurePolicy(Mode mode);

    DisclosurePolicy& withProducerDisclosureDisallowed(bool disallowed = true);

    DisclosurePolicy& withConsumerDisclosureDisallowed(bool disallowed = true);

    Mode mode() const;

    bool producerDisclosureDisallowed() const;

    bool consumerDisclosureDisallowed() const;

    ErrorOr<bool> computeDisclosure(bool producerDisclosure,
                                    bool consumerDisclosure);

    ErrorOr<bool> computeDisclosure(bool producerDisclosure,
                                    bool consumerDisclosure,
                                    DisclosurePolicy preset);

private:
    Mode mode_ = Mode::preset;
    bool producerDisclosureDisallowed_ = false;
    bool consumerDisclosureDisallowed_ = false;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/disclosure.inl.hpp"
#endif

#endif // CPPWAMP_DISCLOSURE_HPP
