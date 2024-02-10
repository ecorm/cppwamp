/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpprotocol.hpp"
#include <cctype>
#include <utility>
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// AnyHttpAction
//******************************************************************************

CPPWAMP_INLINE AnyHttpAction::AnyHttpAction() = default;

/** Returns false if the AnyHttpAction is empty. */
CPPWAMP_INLINE AnyHttpAction::operator bool() const {return action_ != nullptr;}

/** Obtains the route associated with the action. */
CPPWAMP_INLINE std::string AnyHttpAction::route() const
{
    return (action_ == nullptr) ? std::string{} : action_->route();
}

CPPWAMP_INLINE void AnyHttpAction::initialize(internal::PassKey,
                                              const HttpServerOptions& options)
{
    action_->initialize(options);
}

CPPWAMP_INLINE void AnyHttpAction::expect(internal::PassKey, HttpJob& job)
{
    assert(action_ != nullptr);
    action_->expect(job);
}

CPPWAMP_INLINE void AnyHttpAction::execute(internal::PassKey, HttpJob& job)
{
    assert(action_ != nullptr);
    action_->execute(job);
};


//******************************************************************************
// HttpServerBlock
//******************************************************************************

CPPWAMP_INLINE HttpServerBlock::HttpServerBlock(std::string hostName)
    : hostName_(std::move(hostName))
{}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::withOptions(HttpServerOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::addExactRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByExactKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::addPrefixRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByPrefixKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServerBlock::hostName() const
{
    return hostName_;
}

CPPWAMP_INLINE const HttpServerOptions& HttpServerBlock::options() const
{
    return options_;
}

CPPWAMP_INLINE HttpServerOptions& HttpServerBlock::options() {return options_;}

CPPWAMP_INLINE AnyHttpAction* HttpServerBlock::doFindAction(const char* target)
{
    {
        auto found = actionsByExactKey_.find(target);
        if (found != actionsByExactKey_.end())
            return &(found.value());
    }

    {
        auto found = actionsByPrefixKey_.longest_prefix(target);
        if (found != actionsByPrefixKey_.end())
            return &(found.value());
    }

    return nullptr;
}

CPPWAMP_INLINE void HttpServerBlock::initialize(
    internal::PassKey, const HttpServerOptions& parentOptions)
{
    options_.merge(parentOptions);
    for (auto& a: actionsByExactKey_)
        a.initialize({}, options_);
    for (auto& a: actionsByPrefixKey_)
        a.initialize({}, options_);
}


//******************************************************************************
// HttpListenerLimits
//******************************************************************************

HttpListenerLimits::HttpListenerLimits() = default;

HttpListenerLimits& HttpListenerLimits::withBacklogCapacity(int capacity)
{
    backlogCapacity_ = capacity;
    return *this;
}

int HttpListenerLimits::backlogCapacity() const {return backlogCapacity_;}


//******************************************************************************
// HttpEndpoint
//******************************************************************************

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(Port port)
    : HttpEndpoint("", port)
{}

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(std::string address,
                                          unsigned short port)
    : Base(std::move(address), port)

{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withOptions(HttpServerOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::addBlock(HttpServerBlock block)
{
    auto key = block.hostName();
    toLowercase(key);
    serverBlocks_[std::move(key)] = std::move(block);
    return *this;
}

CPPWAMP_INLINE const HttpServerOptions& HttpEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE HttpServerOptions& HttpEndpoint::options() {return options_;}

CPPWAMP_INLINE HttpServerBlock* HttpEndpoint::findBlock(std::string hostName)
{
    toLowercase(hostName);
    auto found = serverBlocks_.find(hostName);
    if (found != serverBlocks_.end())
        return &found->second;

    static const std::string empty;
    found = serverBlocks_.find(empty);
    if (found != serverBlocks_.end())
        return &found->second;

    return nullptr;
}

CPPWAMP_INLINE std::string HttpEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "HTTP Port " + portString;
    return "HTTP " + address() + ':' + portString;
}

CPPWAMP_INLINE void HttpEndpoint::toLowercase(std::string& str)
{
    for (auto& c: str)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

CPPWAMP_INLINE void HttpEndpoint::initialize(internal::PassKey)
{
    options_.merge(HttpServerOptions::defaults());
    for (auto& kv: serverBlocks_)
        kv.second.initialize({}, options_);
}

} // namespace wamp
