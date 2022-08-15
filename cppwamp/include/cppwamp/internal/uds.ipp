/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../uds.hpp"
#include "asioconnector.hpp"
#include "rawsockconnector.hpp"
#include "udsopener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct UdsConnector::Impl
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Endpoint>;

    Impl(IoStrand s, UdsPath p, BufferCodecBuilder b)
        : cnct(ConcreteConnector::create(std::move(s), std::move(p),
                                         std::move(b)))
    {}

    Impl(ConcreteConnector::Ptr clone) : cnct(std::move(clone)) {}

    ConcreteConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE UdsConnector::Ptr
UdsConnector::create(const AnyIoExecutor& e, UdsPath h, BufferCodecBuilder b)
{
    using std::move;
    return Ptr(new UdsConnector(boost::asio::make_strand(e), move(h), move(b)));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE IoStrand UdsConnector::strand() const
{
    return impl_->cnct->strand();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector::Ptr UdsConnector::clone() const
{
    auto& c = *(impl_->cnct);
    return Ptr(new UdsConnector(c.strand(), c.info(), c.codecBuilder()));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void UdsConnector::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void UdsConnector::cancel() {impl_->cnct->cancel();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE UdsConnector::UdsConnector(IoStrand s, UdsPath h,
                                          BufferCodecBuilder b)
    : impl_(new Impl(std::move(s), std::move(h), std::move(b)))
{}

} // namespace wamp
