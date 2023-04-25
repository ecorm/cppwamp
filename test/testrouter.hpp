/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_TESTROUTER_HPP
#define CPPWAMP_TEST_TESTROUTER_HPP

#include <memory>

namespace test
{

//------------------------------------------------------------------------------
class Router
{
public:
    void start();
    void stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace test

#endif // CPPWAMP_TEST_TESTROUTER_HPP
