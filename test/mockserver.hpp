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
#include <cppwamp/json.hpp>
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
        transport_->start(
            [self](ErrorOr<MessageBuffer> b)
            {
                auto me = self.lock();
                if (me)
                    me->onMessage(std::move(b));
            },
            [](std::error_code) {});
    }
    
    void close() {transport_->stop();}

    const MessageList& messages() const {return messages_;}

private:
    MockServerSession(Transporting::Ptr&& t, Responses cannedResponses)
        : responses_(std::move(cannedResponses)),
          transport_(std::move(t))
    {}

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

    template <typename E>
    static Ptr create(E&& exec, uint16_t port)
    {
        return Ptr(new MockServer(std::forward<E>(exec), port));
    }

    void load(Responses cannedResponses)
    {
        responses_ = std::move(cannedResponses);
    }

    void start() {listen();}

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
    template <typename E>
    MockServer(E&& exec, uint16_t port)
        : listener_(boost::asio::make_strand(exec), TcpEndpoint{port},
                    {Json::id()})
    {}

    void listen()
    {
        std::weak_ptr<MockServer> self{shared_from_this()};
        listener_.establish(
            [self](ErrorOr<Transporting::Ptr> transport)
            {
                auto me = self.lock();
                if (me && transport.has_value())
                    me->onEstablished(*transport);
            });
    }

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
