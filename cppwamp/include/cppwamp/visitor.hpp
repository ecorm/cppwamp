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
#include <type_traits>
#include <utility>
#include "api.hpp"

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
using ResultTypeOf = typename std::remove_reference<TVisitor>::type::Result;

//------------------------------------------------------------------------------
/** Applies the given _static visitor) functor to the given variant.
    @tparam V (Deduced) The visitor type, conforming to @ref StaticVisitor
    @tparam T (Deduced) The Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename T>
CPPWAMP_API ResultTypeOf<V> apply(V&& visitor, T&& variant);

//------------------------------------------------------------------------------
/** Applies the given _binary visitor_ functor to the two given variants.
    @tparam V (Deduced) The visitor type, conforming to @ref BinaryVisitor
    @tparam L (Deduced) The left-hand Variant type
    @tparam R (Deduced) The right-hand Variant type
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename L, typename R>
CPPWAMP_API ResultTypeOf<V> apply(V&& visitor, L&& leftVariant,
                                  R&& rightVariant);

//------------------------------------------------------------------------------
/** Applies the given _static visitor_ functor, with an operand value, to the
    given variant.
    @tparam V (Deduced) The visitor type, conforming to @ref OperandVisitor
    @tparam T (Deduced) The Variant type
    @tparam O (Deduced) The value type of the operand
    @relates Variant */
//------------------------------------------------------------------------------
template <typename V, typename T, typename O>
CPPWAMP_API ResultTypeOf<V> applyWithOperand(V&& visitor, T&& variant,
                                             O&& operand);

/// @}


//******************************************************************************
// Visitation implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename V, typename T>
ResultTypeOf<V> apply(V&& visitor, T&& variant)
{
    using I = decltype(variant.typeId());
    switch (variant.typeId())
    {
    case I::null:    return std::forward<V>(visitor)(variant.template as<I::null>());
    case I::boolean: return std::forward<V>(visitor)(variant.template as<I::boolean>());
    case I::integer: return std::forward<V>(visitor)(variant.template as<I::integer>());
    case I::uint:    return std::forward<V>(visitor)(variant.template as<I::uint>());
    case I::real:    return std::forward<V>(visitor)(variant.template as<I::real>());
    case I::string:  return std::forward<V>(visitor)(variant.template as<I::string>());
    case I::blob:    return std::forward<V>(visitor)(variant.template as<I::blob>());
    case I::array:   return std::forward<V>(visitor)(variant.template as<I::array>());
    case I::object:  return std::forward<V>(visitor)(variant.template as<I::object>());
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return visitor(variant.template as<I::null>());
}

//------------------------------------------------------------------------------
template <typename V, typename L, typename R>
ResultTypeOf<V> apply(V&& v, L&& l, R&& r)
{
    using I = decltype(l.typeId());

    switch(r.typeId())
    {
    case I::null:    return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::null>());
    case I::boolean: return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::boolean>());
    case I::integer: return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::integer>());
    case I::uint:    return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::uint>());
    case I::real:    return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::real>());
    case I::string:  return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::string>());
    case I::blob:    return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::blob>());
    case I::array:   return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::array>());
    case I::object:  return applyWithOperand(std::forward<V>(v), std::forward<L>(l), r.template as<I::object>());
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return applyWithOperand(std::forward<V>(v), std::forward<L>(l),
                            r.template as<I::null>());
}

//------------------------------------------------------------------------------
template <typename V, typename T, typename O>
ResultTypeOf<V> applyWithOperand(V&& v, T&& l, O&& o)
{
    using I = decltype(l.typeId());

    switch(l.typeId())
    {
    case I::null:    return std::forward<V>(v)(l.template as<I::null>(),    std::forward<O>(o));
    case I::boolean: return std::forward<V>(v)(l.template as<I::boolean>(), std::forward<O>(o));
    case I::integer: return std::forward<V>(v)(l.template as<I::integer>(), std::forward<O>(o));
    case I::uint:    return std::forward<V>(v)(l.template as<I::uint>(),    std::forward<O>(o));
    case I::real:    return std::forward<V>(v)(l.template as<I::real>(),    std::forward<O>(o));
    case I::string:  return std::forward<V>(v)(l.template as<I::string>(),  std::forward<O>(o));
    case I::blob:    return std::forward<V>(v)(l.template as<I::blob>(),    std::forward<O>(o));
    case I::array:   return std::forward<V>(v)(l.template as<I::array>(),   std::forward<O>(o));
    case I::object:  return std::forward<V>(v)(l.template as<I::object>(),  std::forward<O>(o));
    default:         assert(false);
    }

    // Unreachable. Return null case to silence warning.
    return std::forward<V>(v)(l.template as<I::null>(), std::forward<O>(o));
}

} // namespace wamp

#endif // CPPWAMP_VISITOR_HPP
