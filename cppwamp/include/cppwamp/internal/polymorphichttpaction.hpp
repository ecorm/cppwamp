/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP
#define CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP

#include <string>
#include <utility>
#include "../transports/httpstatus.hpp"

namespace wamp
{

class HttpEndpoint;

namespace internal
{

// Forward declarations
class HttpJob;
template <typename TOptions> class HttpAction;

//------------------------------------------------------------------------------
class PolymorphicHttpActionInterface
{
public:
    virtual ~PolymorphicHttpActionInterface() = default;

    virtual std::string route() const = 0;

    virtual void initialize(const HttpEndpoint& settings) = 0;

    virtual void expect(HttpJob& job) = 0;

    virtual void execute(HttpJob& job) = 0;
};

//------------------------------------------------------------------------------
template <typename TOptions>
class PolymorphicHttpAction : public PolymorphicHttpActionInterface
{
public:
    using Options = TOptions;

    PolymorphicHttpAction() = default;

    PolymorphicHttpAction(Options options) : action_(std::move(options)) {}

    std::string route() const override {return action_.route();}

    void initialize(const HttpEndpoint& settings) override
    {
        action_.initialize(settings);
    }

    void expect(HttpJob& job) override {return action_.expect(job);}

    void execute(HttpJob& job) override {action_.execute(job);}

private:
    HttpAction<Options> action_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_POLYMORPHICHTTPACTION_HPP
