/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
#define CPPWAMP_INTERNAL_UDSACCEPTOR_HPP

#include <cassert>
#include <memory>
#include <string>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/strand.hpp>
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

    template <typename TExecutorOrStrand>
    UdsAcceptor(TExecutorOrStrand&& exec, const std::string& path,
                bool deleteFile)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          path_(path),
          deleteFile_(deleteFile)
    {}

    IoStrand& strand() {return strand_;}

    template <typename F>
    void establish(F&& callback)
    {
        struct Accepted
        {
            UdsAcceptor* self;
            typename std::decay<F>::type callback;

            void operator()(AsioErrorCode ec)
            {
                if (ec)
                {
                    self->acceptor_.reset();
                    self->socket_.reset();
                }
                callback(ec, std::move(self->socket_));
            }
        };

        assert(!socket_ && "Accept already in progress");

        if (!acceptor_)
        {
            if (deleteFile_)
                std::remove(path_.c_str());
            acceptor_.reset(new uds::acceptor(strand_, path_));
        }
        socket_.reset(new Socket(strand_));

        // AsioListener will keep this object alive until completion.
        acceptor_->async_accept(*socket_,
                                Accepted{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (acceptor_)
            acceptor_->close();
    }

private:
    using uds = boost::asio::local::stream_protocol;

    IoStrand strand_;
    std::string path_;
    bool deleteFile_;
    std::unique_ptr<uds::acceptor> acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
