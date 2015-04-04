/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../visitor.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename V, typename T>
ResultTypeOf<V> apply(V&& visitor, T&& variant)
{
    using I = TypeId;
    switch (variant.typeId())
    {
    case I::null:    return visitor(variant.template as<I::null>());
    case I::boolean: return visitor(variant.template as<I::boolean>());
    case I::integer: return visitor(variant.template as<I::integer>());
    case I::uint:    return visitor(variant.template as<I::uint>());
    case I::real:    return visitor(variant.template as<I::real>());
    case I::string:  return visitor(variant.template as<I::string>());
    case I::array:   return visitor(variant.template as<I::array>());
    case I::object:  return visitor(variant.template as<I::object>());
    default:                 assert(false);
    }
}

//------------------------------------------------------------------------------
template <typename V, typename L, typename R>
ResultTypeOf<V> apply(V&& v, L&& l, R&& r)
{
    using std::forward;
    using I = TypeId;

    switch(r.typeId())
    {
    case I::null:    return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::null>());
    case I::boolean: return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::boolean>());
    case I::integer: return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::integer>());
    case I::uint:    return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::uint>());
    case I::real:    return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::real>());
    case I::string:  return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::string>());
    case I::array:   return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::array>());
    case I::object:  return applyWithOperand(forward<V>(v), forward<L>(l), r.template as<I::object>());
    default:         assert(false);
    }
}

//------------------------------------------------------------------------------
template <typename V, typename T, typename O>
ResultTypeOf<V> applyWithOperand(V&& v, T&& l, O&& o)
{
    using std::forward;
    using I = TypeId;

    switch(l.typeId())
    {
    case I::null:    return v(l.template as<I::null>(),    forward<O>(o));
    case I::boolean: return v(l.template as<I::boolean>(), forward<O>(o));
    case I::integer: return v(l.template as<I::integer>(), forward<O>(o));
    case I::uint:    return v(l.template as<I::uint>(),    forward<O>(o));
    case I::real:    return v(l.template as<I::real>(),    forward<O>(o));
    case I::string:  return v(l.template as<I::string>(),  forward<O>(o));
    case I::array:   return v(l.template as<I::array>(),   forward<O>(o));
    case I::object:  return v(l.template as<I::object>(),  forward<O>(o));
    default:         assert(false);
    }
}

} // namespace wamp
