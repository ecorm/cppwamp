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
CPPWAMP_API ResultTypeOf<TVisitor> applyWithOperand(TVisitor&& v, TVariant&& x,
                                                    TOperand&& o);

/// @}


//******************************************************************************
// Visitation implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename V, typename X>
ResultTypeOf<V> apply(V&& v, X&& x)
{
    using A = internal::VariantUncheckedAccess;
    using I = decltype(x.typeId());

    switch (x.typeId())
    {
    case I::null:    return std::forward<V>(v)(A::alt<I::null>   (std::forward<X>(x)));
    case I::boolean: return std::forward<V>(v)(A::alt<I::boolean>(std::forward<X>(x)));
    case I::integer: return std::forward<V>(v)(A::alt<I::integer>(std::forward<X>(x)));
    case I::uint:    return std::forward<V>(v)(A::alt<I::uint>   (std::forward<X>(x)));
    case I::real:    return std::forward<V>(v)(A::alt<I::real>   (std::forward<X>(x)));
    case I::string:  return std::forward<V>(v)(A::alt<I::string> (std::forward<X>(x)));
    case I::blob:    return std::forward<V>(v)(A::alt<I::blob>   (std::forward<X>(x)));
    case I::array:   return std::forward<V>(v)(A::alt<I::array>  (std::forward<X>(x)));
    case I::object:  return std::forward<V>(v)(A::alt<I::object> (std::forward<X>(x)));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return std::forward<V>(v)(x.template as<I::null>());
}

//------------------------------------------------------------------------------
template <typename V, typename X, typename Y>
ResultTypeOf<V> apply(V&& v, X&& x, Y&& y)
{
    using I = decltype(x.typeId());

    switch(y.typeId())
    {
    case I::null:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::null>());
    case I::boolean: return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::boolean>());
    case I::integer: return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::integer>());
    case I::uint:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::uint>());
    case I::real:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::real>());
    case I::string:  return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::string>());
    case I::blob:    return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::blob>());
    case I::array:   return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::array>());
    case I::object:  return applyWithOperand(std::forward<V>(v), std::forward<X>(x), y.template as<I::object>());
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return applyWithOperand(std::forward<V>(v), std::forward<X>(x),
                            y.template as<I::null>());
}

//------------------------------------------------------------------------------
template <typename V, typename X, typename O>
ResultTypeOf<V> applyWithOperand(V&& v, X&& x, O&& o)
{
    using A = internal::VariantUncheckedAccess;
    using I = decltype(x.typeId());

    switch (x.typeId())
    {
    case I::null:    return std::forward<V>(v)(A::alt<I::null>   (std::forward<X>(x)), std::forward<O>(o));
    case I::boolean: return std::forward<V>(v)(A::alt<I::boolean>(std::forward<X>(x)), std::forward<O>(o));
    case I::integer: return std::forward<V>(v)(A::alt<I::integer>(std::forward<X>(x)), std::forward<O>(o));
    case I::uint:    return std::forward<V>(v)(A::alt<I::uint>   (std::forward<X>(x)), std::forward<O>(o));
    case I::real:    return std::forward<V>(v)(A::alt<I::real>   (std::forward<X>(x)), std::forward<O>(o));
    case I::string:  return std::forward<V>(v)(A::alt<I::string> (std::forward<X>(x)), std::forward<O>(o));
    case I::blob:    return std::forward<V>(v)(A::alt<I::blob>   (std::forward<X>(x)), std::forward<O>(o));
    case I::array:   return std::forward<V>(v)(A::alt<I::array>  (std::forward<X>(x)), std::forward<O>(o));
    case I::object:  return std::forward<V>(v)(A::alt<I::object> (std::forward<X>(x)), std::forward<O>(o));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return std::forward<V>(v)(x.template as<I::null>(), std::forward<O>(o));
}

} // namespace wamp

#endif // CPPWAMP_VISITOR_HPP
