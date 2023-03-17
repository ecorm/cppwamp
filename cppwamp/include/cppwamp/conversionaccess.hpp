/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONVERSION_ACCESS_HPP
#define CPPWAMP_CONVERSION_ACCESS_HPP

#include <type_traits>
#include "api.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains the ConversionAccess class. */
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/* Generates a metafunction that checks for the presence of a member.
   Adapted from
   http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector. */
//------------------------------------------------------------------------------
#define CPPWAMP_GENERATE_HAS_MEMBER(member)                             \
                                                                        \
template <typename T>                                                   \
class CPPWAMP_API HasMember_##member                                                \
{                                                                       \
private:                                                                \
    using Yes = char[2];                                                \
    using  No = char[1];                                                \
                                                                        \
    struct Fallback { int member; };                                    \
    struct Derived : T, Fallback { };                                   \
                                                                        \
    template <typename U> static No& test ( decltype(U::member)* );     \
    template <typename U> static Yes& test ( U* );                      \
                                                                        \
public:                                                                 \
    static constexpr bool result =                                      \
        sizeof(test<Derived>(nullptr)) == sizeof(Yes);                  \
};                                                                      \
                                                                        \
template < class T >                                                    \
struct has_member_##member                                              \
: public std::integral_constant<bool, HasMember_##member<T>::result>    \
{};


namespace wamp
{

//------------------------------------------------------------------------------
/** Helper class used to gain access to private conversion member functions.
    If you make your conversion member functions private, then you must grant
    friendship to the ConversionAccess class. Other than granting friendship,
    users should not have to use this class. */
//------------------------------------------------------------------------------
class CPPWAMP_API ConversionAccess
{
public:
    template <typename TConverter, typename TObject>
    static void convert(TConverter& c, TObject& obj)
    {
        static_assert(
            has_member_convert<TObject>(),
            "The 'convert' function has not been specialized for this type. "
            "Either provide a 'convert' free function specialization, or "
            "a 'convert' member function.");
        obj.convert(c);
    }

    template <typename TConverter, typename TObject>
    static void convertFrom(TConverter& c, TObject& obj)
    {
        static_assert(has_member_convertFrom<TObject>(),
                      "The 'convertFrom' member function has not been provided "
                      "for this type.");
        obj.convertFrom(c);
    }

    template <typename TConverter, typename TObject>
    static void convertTo(TConverter& c, const TObject& obj)
    {
        static_assert(has_member_convertTo<TObject>(),
                      "The 'convertTo' member function has not been provided "
                      "for this type.");
        obj.convertTo(c);
    }

    template <typename TObject>
    static TObject defaultConstruct() {return TObject();}

    template <typename TObject>
    static TObject* defaultHeapConstruct() {return new TObject;}

private:
    CPPWAMP_GENERATE_HAS_MEMBER(convert)
    CPPWAMP_GENERATE_HAS_MEMBER(convertFrom)
    CPPWAMP_GENERATE_HAS_MEMBER(convertTo)
};

} // namespace wamp

#endif // CPPWAMP_CONVERSION_ACCESS_HPP
