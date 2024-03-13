/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/wssclient.hpp"
#include "basicwebsocketconnector.hpp"
#include "wsstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class WssConnector : public BasicWebsocketConnector<WssTraits>
{
private:
    using Base = BasicWebsocketConnector<WssTraits>;

public:
    using Base::Base;
};

} // namespace internal


//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Wss>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(std::make_shared<internal::WssConnector>(std::move(i), std::move(s),
                                                     codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Wss>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Wss>::establish(Handler handler)
{
    impl_->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Wss>::cancel() {impl_->cancel();}

} // namespace wamp
