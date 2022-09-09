/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SERVER_HPP
#define CPPWAMP_SERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for configuring and interacting with routing
           servers. */
//------------------------------------------------------------------------------

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "listener.hpp"
#include "peerdata.hpp"

namespace wamp
{

namespace internal { class RouterServer; } // Forward declaration


//------------------------------------------------------------------------------
class CPPWAMP_API ServerConfig
{
public:
    using AuthExchangeHandler = AnyReusableHandler<void (AuthExchange)>;

    template <typename S>
    explicit ServerConfig(std::string name, S&& transportSettings)
        : name_(std::move(name)),
          listenerBuilder_(std::forward<S>(transportSettings))
    {}

    template <typename... TFormats>
    ServerConfig& withFormats(TFormats... formats)
    {
        codecBuilders_ = {BufferCodecBuilder{formats}...};
        return *this;
    }

    ServerConfig& withAuthenticator(AuthExchangeHandler f)
    {
        authenticator_ = std::move(f);
        return *this;
    }

    const std::string& name() const {return name_;}

private:
    Listening::Ptr makeListener(IoStrand s) const
    {
        std::set<int> codecIds;
        for (const auto& c: codecBuilders_)
            codecIds.emplace(c.id());
        return listenerBuilder_(std::move(s), std::move(codecIds));
    }

    AnyBufferCodec makeCodec(int codecId) const
    {
        for (const auto& c: codecBuilders_)
            if (c.id() == codecId)
                return c();
        assert(false);
        return {};
    }

    std::string name_;
    ListenerBuilder listenerBuilder_;
    std::vector<BufferCodecBuilder> codecBuilders_;
    AnyReusableHandler<void (AuthExchange)> authenticator_;

    friend class internal::RouterServer;
};

//------------------------------------------------------------------------------
class CPPWAMP_API Server : public std::enable_shared_from_this<Server>
{
public:
    using Ptr = std::shared_ptr<Server>;

    virtual ~Server() {}

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual const std::string& name() const = 0;

    virtual bool isRunning() const = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/router.ipp"
#endif

#endif // CPPWAMP_SERVER_HPP
