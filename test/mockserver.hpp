/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_MOCKROUTER_HPP
#define CPPWAMP_TEST_MOCKROUTER_HPP

#include <cassert>
#include <deque>
#include <vector>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/internal/message.hpp>
#include <cppwamp/transports/tcp.hpp>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MockServerSession : public std::enable_shared_from_this<MockServerSession>
{
public:
    using Ptr = std::shared_ptr<MockServerSession>;
    using ResponseBatch = std::vector<std::string>;
    using Responses = std::deque<ResponseBatch>;
    using MessageList = std::vector<Message>;

    static Ptr create(Transporting::Ptr t, Responses cannedResponses)
    {
        return Ptr(new MockServerSession(std::move(t),
                                         std::move(cannedResponses)));
    }

    void open()
    {
        assert(!alreadyStarted_);
        alreadyStarted_ = true;
        std::weak_ptr<MockServerSession> self = shared_from_this();
        transport_->accept(
            [self](ErrorOr<int> codecId)
            {
                auto me = self.lock();
                if (me)
                    me->onAccept(codecId);
            });
    }
    
    void close() {transport_->stop();}

    const MessageList& messages() const {return messages_;}

private:
    MockServerSession(Transporting::Ptr&& t, Responses cannedResponses)
        : responses_(std::move(cannedResponses)),
          transport_(std::move(t))
    {}

    void onAccept(ErrorOr<int> codecId)
    {
        if (!codecId)
            return;
        std::weak_ptr<MockServerSession> self = shared_from_this();
        transport_->start(
            [self](ErrorOr<MessageBuffer> b)
            {
                auto me = self.lock();
                if (me)
                    me->onMessage(std::move(b));
            },
            [](std::error_code) {});
    }

    void onMessage(ErrorOr<MessageBuffer> buffer)
    {
        if (!buffer)
            return;
        Variant decoded;
        auto ec = decoder_.decode(*buffer, decoded);
        assert(!ec);
        auto message = internal::Message::parse(std::move(decoded.as<Array>()));
        messages_.emplace_back(std::move(message).value());

        if (responses_.empty())
            return;
        const auto& batch = responses_.front();
        for (const auto& json: batch)
        {
            auto ptr = reinterpret_cast<const uint8_t*>(json.data());
            MessageBuffer buffer(ptr, ptr + json.size());
            transport_->send(std::move(buffer));
        }
        responses_.pop_front();
    }

    Responses responses_;
    MessageList messages_;
    JsonBufferDecoder decoder_;
    Transporting::Ptr transport_;
    bool alreadyStarted_ = false;
};


//------------------------------------------------------------------------------
class MockServer : public std::enable_shared_from_this<MockServer>
{
public:
    using Ptr = std::shared_ptr<MockServer>;
    using Responses = MockServerSession::Responses;
    using MessageList = std::vector<Message>;

    static Ptr create(AnyIoExecutor exec, uint16_t port)
    {
        return Ptr(new MockServer(std::move(exec), port));
    }

    void load(Responses cannedResponses)
    {
        responses_ = std::move(cannedResponses);
    }

    void start()
    {
        std::weak_ptr<MockServer> self{shared_from_this()};
        listener_.observe(
            [self](ListenResult result)
            {
                auto me = self.lock();
                if (me && result.ok())
                    me->onEstablished(result.transport());
            });

        listen();
    }

    void stop()
    {
        listener_.cancel();
        if (session_)
            session_->close();
    }

    const MessageList& messages() const
    {
        static const MessageList empty;
        return session_ ? session_->messages() : empty;
    }

    MessageKind lastMessageKind() const
    {
        const auto& m = messages();
        return m.empty() ? MessageKind::none : m.back().kind();
    }

    template <typename C>
    static C toCommand(Message&& m) {return C{PassKey{}, std::move(m)};}

private:
    MockServer(AnyIoExecutor exec, uint16_t port)
        : listener_(exec, boost::asio::make_strand(exec), TcpEndpoint{port},
                    {Json::id()})
    {}

    void listen() {listener_.establish();}

    void onEstablished(Transporting::Ptr transport)
    {
        if (session_)
            session_->close();
        session_ = MockServerSession::create(std::move(transport),
                                             std::move(responses_));
        session_->open();
        listen();
    }

    Responses responses_;
    AnyIoExecutor executor_;
    MockServerSession::Ptr session_;
    Listener<Tcp> listener_;

    friend class ServerContext;
};

} // namespace internal

} // namespace wamp}

#endif // CPPWAMP_TEST_MOCKROUTER_HPP
