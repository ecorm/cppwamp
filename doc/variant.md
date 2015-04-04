<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Variants
========

A `wamp::Variant` is a discriminated union container that represents a JSON value. It behaves similarly to a dynamically-typed Javascript variable. Its underlying type can change at runtime, depending on the actual values assigned to them. Variants play a central role in CppWAMP, as they are used to represent dynamic data exchanged with a WAMP peer.

Variants can hold any of the following value types:
- `Null` : represents an empty or missing value
- `Bool` : true or false
- `Int` : signed integer (64-bit)
- `UInt`: unsigned integer (64-bit)
- `Real`: floating point number (double precision)
- `String` : only UTF-8 encoded strings are currently supported
- `Array` : dynamically-sized lists of variants
- `Object` : dictionaries having string keys and variant values

`Array` and `Object` variants are recursive composites: their element values are also variants which can themselves be arrays or objects.

Construction and Assignment
---------------------------

A default-constructed `Variant` starts out being null:
```c++
#include <cppwamp/variant.hpp>
using namespace wamp;

Variant v; // v starts out being null
```

A `Variant` can also be constructed with an initial value:
```c++
Variant v(42); // v starts out as an integer
```

The dynamic type of a `Variant` can change at runtime via assignment:
```c++
Variant v;    // v starts out being null
v = 42;       // v becomes an integer
v = "hello";  // v becomes a string
v = null;     // v becomes nullified
v = Array{123, false, "foo"}; // v becomes an array of variants
```

Checking Dynamic Type
---------------------

Each `Variant` stores its current dynamic type as a `wamp::TypeId` enumerator. The dynamic type can be checked with `Variant::typeId()` or `Variant::is<T>()`. In addition, `operator bool` can be used to determine if the variant is null.

```c++
Variant v;    // v starts out being null
assert( !v );   // Check that the variant is indeed null
assert( v == null );
assert( v.typeId() == TypeId::null );
assert( v.is<Null>() );
assert( v.is<TypeId::null>() );

v = 42;       // v becomes an integer
assert( !!v); // Check that the variant is no longer null
assert( v != null );
assert( v.typeId() == TypeId::integer );
assert( v.is<Int>() );
assert( v.is<TypeId::integer>() );
```

The `isNumber` non-member function can be used to check if a Variant is currently a number (`Int`, `UInt`, or `Real`). The `isScalar` non-member function checks if a variant is a boolean or a number:
```c++
Variant v("hello"); // v starts out as a string
assert( !isNumber(v) );
assert( !isScaler(v) );

v = true; // v is now a boolean
assert ( !isNumber(v) );
assert ( isScalar(v) );

v = 123.4; // v is now a real number
assert ( isNumber(v) );
assert ( isScalar(v) );
```

Accessing Values
----------------

`Variant` values can be accessed directly using `Variant::as<T>()`. If the variant does not match the target type, a `wamp::error::access` exception is thrown.
```c++
Variant v(42); // v starts out as an Int
std::cout << v.as<Int>() << "\n"; // Prints 42
v.as<Int>() += 10;
std::cout << v.as<Int>() << "\n"; // Prints 52

try
{
    std::cout << v.as<String>() << "\n"; // throws
}
catch (const error::Access& e)
{
    std::cerr << e.what() << "\n";
}
```

Conversions
-----------

Sometimes, it's not possible to know what numeric type to expect from a WAMP peer. `Variant::to` lets you convert from any scalar type to a known destination type.
```c++
Variant v;
Real x;

v = 42; // v is now an integer
x = v.to<Real>();
assert( x == 42 );

v = true; // v is now a boolean
v.to(x);  // Alternate syntax, equivalent to x = v.to<Real>()
assert( x == 1 );

String s;
try
{
    s = v.to<String>(); // Invalid conversion from boolean to string
}
catch (const error::Conversion& e)
{
    std::cerr << e.what() << "\n";
}
```

Comparisons
-----------

`Variant` objects can be compared with each other, or with other values. For the comparison to be equal, both types must match. However, all numeric types are considered to be the same type for comparison purposes (this is to more closely
match the behavior of the ``===`` operator in Javascript, where it does not distinguish between signed/unsigned or integer/floating-point numbers).
```c++
Variant v; // Starts as null
Variant w; // Starts as null
assert( v == w ); // null == null is always true

v = 42;   // v is now an integer
w = "42"; // w is now a string
assert ( v != w ); // Types do not match

v = 123;   // v is still an integer
w = 123.0; // w is now a real number
assert ( v == w ); // Numeric types match for comparison purposes

v = 0;     // v is still an integer
w = false; // w is now a boolean
assert( v != w ); // Types do not match

v = "hello"; // v is now a String
// String variants can be compared against `char` arrays:
assert( v == "hello" );
```

Output
------

`Variant`, `Array`, and `Object` objects can be outputted to a `std::ostream`:
```c++
Variant v;
std::cout << v << "\n"; // Prints "null"
v = 42;
std::cout << v << "\n"; // Prints "42"

Array a{"foo", false, 123};
v = a;
std::cout << a << "\n"; // Prints "["foo", false, 123]"
std::cout << v << "\n"; // Prints "["foo", false, 123]"
```
