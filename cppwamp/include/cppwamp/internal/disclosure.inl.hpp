/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../disclosure.hpp"
#include <cassert>
#include "../errorcodes.hpp"

namespace wamp
{

CPPWAMP_INLINE DisclosurePolicy::DisclosurePolicy(Mode mode) : mode_(mode) {}

CPPWAMP_INLINE DisclosurePolicy&
DisclosurePolicy::withProducerDisclosureDisallowed(bool disallowed)
{
    producerDisclosureDisallowed_ = disallowed;
    return *this;
}

CPPWAMP_INLINE DisclosurePolicy&
DisclosurePolicy::withConsumerDisclosureDisallowed(bool disallowed)
{
    consumerDisclosureDisallowed_ = disallowed;
    return *this;
}

CPPWAMP_INLINE Disclosure DisclosurePolicy::mode() const {return mode_;}

CPPWAMP_INLINE bool DisclosurePolicy::producerDisclosureDisallowed() const
{
    return producerDisclosureDisallowed_;
}

CPPWAMP_INLINE bool DisclosurePolicy::consumerDisclosureDisallowed() const
{
    return consumerDisclosureDisallowed_;
}

CPPWAMP_INLINE ErrorOr<bool>
DisclosurePolicy::computeDisclosure(bool producerDisclosure,
                                    bool consumerDisclosure)
{
    if (producerDisclosure && producerDisclosureDisallowed_)
        return makeUnexpectedError(WampErrc::discloseMeDisallowed);
    if (consumerDisclosure && consumerDisclosureDisallowed_)
        return makeUnexpectedError(WampErrc::optionNotAllowed);

    switch (mode_)
    {
    case Mode::preset:   return producerDisclosure;
    case Mode::producer: return producerDisclosure;
    case Mode::consumer: return consumerDisclosure;
    case Mode::either:   return producerDisclosure || consumerDisclosure;
    case Mode::both:     return producerDisclosure && consumerDisclosure;
    case Mode::reveal:   return true;
    case Mode::conceal:  return false;
    default:             break;
    }

    assert(false && "Unexpected DisclosureMode enumerator");
    return false;
}

CPPWAMP_INLINE ErrorOr<bool> DisclosurePolicy::computeDisclosure(
    bool producerDisclosure, bool consumerDisclosure, DisclosurePolicy preset)
{
    if (mode_ == Mode::preset)
        return preset.computeDisclosure(producerDisclosure, consumerDisclosure);
    return computeDisclosure(producerDisclosure, consumerDisclosure);
}

} // namespace wamp
