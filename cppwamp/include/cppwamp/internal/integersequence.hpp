/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTEGERSEQUENCE_HPP
#define CPPWAMP_INTEGERSEQUENCE_HPP

#ifndef CPPWAMP_FOR_DOXYGEN // GenIntegerSequence confuses the Doxygen parser

namespace wamp
{

namespace internal
{

// Represents a compile-time sequence of integers.
template<int ...> struct IntegerSequence { };


// Generates a compile-time sequence of integers.
template<int N, int ...S>
struct GenIntegerSequence : GenIntegerSequence<N-1, N-1, S...> { };

template<int ...S>
struct GenIntegerSequence<0, S...>
{
    using type = IntegerSequence<S...>;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_FOR_DOXYGEN

#endif // CPPWAMP_INTEGERSEQUENCE_HPP
