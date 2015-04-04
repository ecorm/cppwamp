/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VISITOR_HPP
#define CPPWAMP_VISITOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains facilities for applying _static visitors_ to Variant objects . */
//------------------------------------------------------------------------------

#include <type_traits>

namespace wamp
{

//------------------------------------------------------------------------------
/** Convenience base class used to meet the result type requirements of a
    @ref StaticVisitor. */
//------------------------------------------------------------------------------
template <typename TResult = void>
class Visitor
{
public:
    /** The return type for all of the visitor's dispatch functions. */
    using Result = TResult;
};

/// @name Non-member Visitation Functions (in visitor.hpp)
/// @{

//------------------------------------------------------------------------------
/** Metafunction used to obtain the return type of a static visitor. */
//------------------------------------------------------------------------------
template <typename TVisitor>
using ResultTypeOf = typename std::remove_reference<TVisitor>::type::Result;

//------------------------------------------------------------------------------
/** Applies the given _static visitor) functor to the given variant.
    @tparam V (Deduced) The visitor type, conforming to @ref StaticVisitor
    @tparam T (Deduced) The Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename T>
ResultTypeOf<V> apply(V&& visitor, T&& variant);

//------------------------------------------------------------------------------
/** Applies the given _binary visitor_ functor to the two given variants.
    @tparam V (Deduced) The visitor type, conforming to @ref BinaryVisitor
    @tparam L (Deduced) The left-hand Variant type
    @tparam R (Deduced) The right-hand Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename L, typename R>
ResultTypeOf<V> apply(V&& visitor, L&& leftVariant, R&& rightVariant);

//------------------------------------------------------------------------------
/** Applies the given _static visitor_ functor, with an operand value, to the
    given variant.
    @tparam V (Deduced) The visitor type, conforming to @ref OperandVisitor
    @tparam T (Deduced) The Variant type
    @tparam O (Deduced) The value type of the operand
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename T, typename O>
ResultTypeOf<V> applyWithOperand(V&& visitor, T&& variant, O&& operand);

/// @}


} // namespace wamp

#include "internal/visitor.ipp"

#endif // CPPWAMP_VISITOR_HPP
