/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DISCLOSUREMODE_HPP
#define CPPWAMP_INTERNAL_DISCLOSUREMODE_HPP

#include <cassert>
#include "../disclosure.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DisclosureMode
{
public:
    DisclosureMode(Disclosure disclosure)
        : disclosure_(disclosure)
    {}

    Disclosure disclosure() const {return disclosure_;}

    bool compute(bool producerDisclosure, bool consumerDisclosure)
    {
        switch (disclosure_)
        {
        case Disclosure::preset:   return producerDisclosure;
        case Disclosure::producer: return producerDisclosure;
        case Disclosure::consumer: return consumerDisclosure;
        case Disclosure::either:   return producerDisclosure || consumerDisclosure;
        case Disclosure::both:     return producerDisclosure && consumerDisclosure;
        case Disclosure::reveal:   return true;
        case Disclosure::conceal:  return false;
        default:             break;
        }

        assert(false && "Unexpected Disclosure enumerator");
        return false;
    }

    bool compute(bool producerDisclosure, bool consumerDisclosure,
                 DisclosureMode preset)
    {
        if (disclosure_ == Disclosure::preset)
            return preset.compute(producerDisclosure, consumerDisclosure);
        return compute(producerDisclosure, consumerDisclosure);
    }

private:
    Disclosure disclosure_ = Disclosure::preset;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DISCLOSUREMODE_HPP
