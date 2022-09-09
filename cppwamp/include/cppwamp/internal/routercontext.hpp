/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
#define CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP

#include <memory>
#include "../logging.hpp"
#include "wampmessage.hpp"

namespace wamp
{


namespace internal
{

class ServerSession;
class RouterImpl;

//------------------------------------------------------------------------------
class RouterContext
{
public:
    RouterContext(std::shared_ptr<RouterImpl> r);
    LogLevel logLevel() const;
    void addSession(std::shared_ptr<ServerSession> s);
    void removeSession(std::shared_ptr<ServerSession> s);
    void onMessage(std::shared_ptr<ServerSession> s, WampMessage m);
    void log(LogEntry e);

private:
    std::weak_ptr<RouterImpl> router_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
