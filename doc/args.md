<!-- ---------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
         Distributed under the Boost Software License, Version 1.0.
             (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
---------------------------------------------------------------------------- -->
Args
====

`Args` is used by the `Client` APIs to bundle variants into positional and/or keyword arguments.

Passing Arguments
-----------------

Assuming this fictitious API function
```c++
#include <cppwamp/Args.hpp>
using namespace wamp;

void call(std::string procedure, Args args);
```
the following examples show various ways to pass positional and keyword arguments.

**Passing positional arguments with a C++11 braced initializer list:**
```c++
call("rpc", {"foo", true, 42});
```

**Passing positional arguments with an `Array` of variants:**
```c++
// 'with' is a tag value used to prevent overload ambiguities
Array array{"foo", true, 42};
call("rpc", {with, array});
```

**Passing keyword arguments with a C++11 braced initializer list:**
```c++
// 'withPairs' is a tag value used to prevent overload ambiguities
call("rpc", {withPairs, { {"item", "Paperclip"}, {"cost", 10} }});
```

**Pasing keyword arguments with an `Object` of key-variant pairs:**
```c++
// 'with' is a tag value used to prevent overload ambiguities
Object object{{"item", "Paperclip"}, {"cost", 10}};
call("rpc", {with, object});
```

**Passing _both_ positonal and keyword arguments:**
```c++
// 'with' is a tag value used to prevent overload ambiguities
Array array{"foo", true, 42};
Object object{{"item", "Paperclip"}, {"cost", 10}};
call("rpc", {with, array, object});
```

Accessing Arguments
-------------------

The examples in this section assume that there's an `Args` instance, named `args`, containing both positional and keyword arguments:
```c++
#include <cppwamp/Args.hpp>
using namespace wamp;

// 'args' contains both positional and keyword arguments
Array array{"foo", true, 42};
Object object{{"item", "Paperclip"}, {"cost", 10}};
Args args{with, array, object});
```

### Accessing positional arguments

`Args` exposes the array of positional arguments as the `list` public member:
```c++
std::cout << args.list.size() << "\n";  // Prints 3
std::cout << args.list[2] << "\n";      // Prints 42
Array positional = args.list;           // Copy positional arguments
Array moved = std::move(args.list);     // Move positional arguments
```

`Args` overloads `operator[](size_t)` so that individual positional arguments
can be accessed:
```c++
args[2] = 123;
std::cout << args[2] << "\n"; // Prints 123
```

### Accessing keyword arguments

`Args` exposes the dictionary of keyword arguments as the `map` public member:
```c++
std::cout << args.map.size() << "\n";   // Prints 2
std::cout << args.map["item"] << "\n";  // Prints Paperclip
Object keywords = args.map;             // Copy keyword arguments
Object moved = std::move(args.map);     // Move keyword arguments
```

`Args` overloads `operator[](String)` so that individual keyword arguments can be accessed:
```c++
args["cost"] = 5;
std::cout << args["cost"] << "\n"; // Prints 5
```

### Converting positional arguments

Positional arguments can be converted and assigned to destination variables using `Args::to`:
```c++
std::string s;
bool b;
int n;
args.to(s, b, n);
std::cout << s << "," << b << "," << n << "\n"; // Prints "foo,1,42"
```
If any of the arguments can't be converted to their target type, an `error::Conversion` exception is thrown.

### Moving positional arguments

Positional arguments can be moved to destination variables using `Args::move`:
```c++
String s;
Bool b;
Int n;
args.move(s, b, n);
std::cout << s << "," << b << "," << n << "\n"; // Prints "foo,1,42"
```
This can be useful to avoid copying large objects, such as strings or arrays. If any of the arguments don't exactly match their target type, an `error::Access` exception is thrown.
