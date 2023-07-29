/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VISITOR_HPP
#define CPPWAMP_VISITOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for applying _static visitors_
           to Variant objects . */
//------------------------------------------------------------------------------

#include <cassert>
#include <utility>
#include "api.hpp"
#include "traits.hpp"
#include "internal/varianttraits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Convenience base class used to meet the result type requirements of a
    @ref StaticVisitor. */
//------------------------------------------------------------------------------
template <typename TResult = void>
class CPPWAMP_API Visitor
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
using ResultTypeOf = typename ValueTypeOf<TVisitor>::Result;

//------------------------------------------------------------------------------
/** Applies the given _static visitor) functor to the given variant.
    @tparam TVisitor (Deduced) The visitor type, conforming to
            @ref StaticVisitor
    @tparam TVariant (Deduced) The Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename TVisitor, typename TVariant>
CPPWAMP_API ResultTypeOf<TVisitor> apply(TVisitor&& v, TVariant&& x);

//------------------------------------------------------------------------------
/** Applies the given _binary visitor_ functor to the two given variants.
    @tparam TVisitor (Deduced) The visitor type, conforming to
            @ref BinaryVisitor
    @tparam TLeftVariant (Deduced) The left-hand Variant type
    @tparam TRightVariant (Deduced) The right-hand Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename TVisitor, typename TLeftVariant, typename TRightVariant>
CPPWAMP_API ResultTypeOf<TVisitor> apply(TVisitor&& v, TLeftVariant&& x,
                                         TRightVariant&& y);

//------------------------------------------------------------------------------
/** Applies the given _static visitor_ functor, with an operand value, to the
    given variant.
    @tparam TVisitor (Deduced) The visitor type, conforming to
            @ref OperandVisitor
    @tparam TVariant (Deduced) The Variant type
    @tparam TOperand (Deduced) The value type of the operand
    @relates Variant */
//------------------------------------------------------------------------------
template <typename TVisitor, typename TVariant, typename TOperand>
CPPWAMP_API ResultTypeOf<TVisitor>
applyWithOperand(  // NOLINT(misc-no-recursion)
    TVisitor&& v, TVariant&& x, TOperand&& o);

/// @}


//******************************************************************************
// Visitation implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename V, typename X>
ResultTypeOf<V> apply(V&& v, X&& x) // NOLINT(misc-no-recursion)
{
    using A = internal::VariantUncheckedAccess;
    using K = decltype(x.kind());

    switch (x.kind())
    {
    case K::null:    return std::forward<V>(v)(A::alt<K::null>   (std::forward<X>(x)));
    case K::boolean: return std::forward<V>(v)(A::alt<K::boolean>(std::forward<X>(x)));
    case K::integer: return std::forward<V>(v)(A::alt<K::integer>(std::forward<X>(x)));
    case K::uint:    return std::forward<V>(v)(A::alt<K::uint>   (std::forward<X>(x)));
    case K::real:    return std::forward<V>(v)(A::alt<K::real>   (std::forward<X>(x)));
    case K::string:  return std::forward<V>(v)(A::alt<K::string> (std::forward<X>(x)));
    case K::blob:    return std::forward<V>(v)(A::alt<K::blob>   (std::forward<X>(x)));
    case K::array:   return std::forward<V>(v)(A::alt<K::array>  (std::forward<X>(x)));
    case K::object:  return std::forward<V>(v)(A::alt<K::object> (std::forward<X>(x)));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return std::forward<V>(v)(A::alt<K::null>(std::forward<X>(x)));
}

//------------------------------------------------------------------------------
template <typename V, typename X, typename Y>
ResultTypeOf<V> apply(V&& v, X&& x, Y&& y) // NOLINT(misc-no-recursion)
{
    using A = internal::VariantUncheckedAccess;
    using K = decltype(x.kind());

    switch(y.kind())
    {
    case K::null:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::null>   (std::forward<Y>(y)));
    case K::boolean: return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::boolean>(std::forward<Y>(y)));
    case K::integer: return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::integer>(std::forward<Y>(y)));
    case K::uint:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::uint>   (std::forward<Y>(y)));
    case K::real:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::real>   (std::forward<Y>(y)));
    case K::string:  return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::string> (std::forward<Y>(y)));
    case K::blob:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::blob>   (std::forward<Y>(y)));
    case K::array:   return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::array>  (std::forward<Y>(y)));
    case K::object:  return applyWithOperand(std::forward<V>(v), std::forward<X>(x), A::alt<K::object> (std::forward<Y>(y)));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return applyWithOperand(std::forward<V>(v), std::forward<X>(x),
                            A::alt<K::null>(std::forward<Y>(y)));
}

//------------------------------------------------------------------------------
template <typename V, typename X, typename O>
ResultTypeOf<V> applyWithOperand(V&& v, X&& x, O&& o)
{
    using A = internal::VariantUncheckedAccess;
    using K = decltype(x.kind());

    switch (x.kind())
    {
    case K::null:    return std::forward<V>(v)(A::alt<K::null>   (std::forward<X>(x)), std::forward<O>(o));
    case K::boolean: return std::forward<V>(v)(A::alt<K::boolean>(std::forward<X>(x)), std::forward<O>(o));
    case K::integer: return std::forward<V>(v)(A::alt<K::integer>(std::forward<X>(x)), std::forward<O>(o));
    case K::uint:    return std::forward<V>(v)(A::alt<K::uint>   (std::forward<X>(x)), std::forward<O>(o));
    case K::real:    return std::forward<V>(v)(A::alt<K::real>   (std::forward<X>(x)), std::forward<O>(o));
    case K::string:  return std::forward<V>(v)(A::alt<K::string> (std::forward<X>(x)), std::forward<O>(o));
    case K::blob:    return std::forward<V>(v)(A::alt<K::blob>   (std::forward<X>(x)), std::forward<O>(o));
    case K::array:   return std::forward<V>(v)(A::alt<K::array>  (std::forward<X>(x)), std::forward<O>(o));
    case K::object:  return std::forward<V>(v)(A::alt<K::object> (std::forward<X>(x)), std::forward<O>(o));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return std::forward<V>(v)(x.template as<K::null>(), std::forward<O>(o));
}

} // namespace wamp

#endif // CPPWAMP_VISITOR_HPP
