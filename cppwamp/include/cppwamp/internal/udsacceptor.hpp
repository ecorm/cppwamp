/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
#define CPPWAMP_INTERNAL_UDSACCEPTOR_HPP

#include <cassert>
#include <memory>
#include <string>
#include <boost/asio/local/stream_protocol.hpp>
#include "../asiodefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class UdsAcceptor
{
public:
    using Socket    = boost::asio::local::stream_protocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    UdsAcceptor(AsioService& iosvc, const std::string& path, bool deleteFile)
        : iosvc_(iosvc), path_(path), deleteFile_(deleteFile)
    {}

    AsioService& iosvc() {return iosvc_;}

    template <typename TCallback>
    void establish(TCallback&& callback)
    {
        assert(!socket_ && "Accept already in progress");

        if (!acceptor_)
        {
            if (deleteFile_)
                std::remove(path_.c_str());
            acceptor_.reset(new uds::acceptor(iosvc_, path_));
        }
        socket_.reset(new Socket(iosvc_));

        // AsioListener will keep this object alive until completion.
        acceptor_->async_accept(*socket_, [this, callback](AsioErrorCode ec)
        {
            if (ec)
            {
                acceptor_.reset();
                socket_.reset();
            }
            callback(ec, std::move(socket_));
        });
    }

    void cancel()
    {
        if (acceptor_)
            acceptor_->close();
    }

private:
    using uds = boost::asio::local::stream_protocol;

    AsioService& iosvc_;
    std::string path_;
    bool deleteFile_;
    std::unique_ptr<uds::acceptor> acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
