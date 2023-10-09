/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP
#define CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP

#include <utility>

namespace wamp
{

namespace internal
{

class HttpJob;

//------------------------------------------------------------------------------
template <typename TOptions>
class HttpAction
{};

//------------------------------------------------------------------------------
class PolymorphicHttpActionInterface
{
public:
    virtual ~PolymorphicHttpActionInterface() = default;

    virtual void execute(HttpJob& job) = 0;
};

//------------------------------------------------------------------------------
template <typename TOptions>
class PolymorphicHttpAction : public PolymorphicHttpActionInterface
{
public:
    using Options = TOptions;

    PolymorphicHttpAction() = default;

    explicit PolymorphicHttpAction(Options options)
        : action_(std::move(options))
    {}

    virtual void execute(HttpJob& job) override {action_.execute(job);}

private:
    HttpAction<Options> action_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP
