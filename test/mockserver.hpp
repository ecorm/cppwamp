/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_MOCKROUTER_HPP
#define CPPWAMP_TEST_MOCKROUTER_HPP

#include <cassert>
#include <deque>
#include <cppwamp/json.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/internal/message.hpp>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MockServerSession : public std::enable_shared_from_this<MockServerSession>
{
public:
    using Ptr = std::shared_ptr<MockServerSession>;
    using StringList = std::deque<std::string>;
    using MessageList = std::vector<Message>;

    static Ptr create(Transporting::Ptr t, StringList cannedResponses)
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
            [this, self](ErrorOr<MessageBuffer> b) {onMessage(std::move(b));},
            [](std::error_code) {});
    }

    void close() {transport_->close();}

    const MessageList& messages() const {return messages_;}

private:
    MockServerSession(Transporting::Ptr&& t, StringList cannedResponses)
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
        const auto& next = responses_.front();
        auto ptr = reinterpret_cast<const uint8_t*>(next.data());
        MessageBuffer response(ptr, ptr + next.size());
        responses_.pop_front();
        transport_->send(std::move(response));
    }

    StringList responses_;
    std::vector<Message> messages_;
    JsonBufferDecoder decoder_;
    Transporting::Ptr transport_;
    bool alreadyStarted_ = false;
};


//------------------------------------------------------------------------------
class MockServer : public std::enable_shared_from_this<MockServer>
{
public:
    using Ptr = std::shared_ptr<MockServer>;
    using StringList = std::deque<std::string>;
    using MessageList = std::vector<Message>;

    template <typename E>
    static Ptr create(E&& exec, uint16_t port)
    {
        return Ptr(new MockServer(std::forward<E>(exec), port));
    }

    void load(StringList cannedResponses)
    {
        responses_ = std::move(cannedResponses);
    }

    void start()
    {
        std::weak_ptr<MockServer> self{shared_from_this()};
        listener_.establish(
            [this, self](ErrorOr<Transporting::Ptr> transport)
            {
                auto me = self.lock();
                if (!me)
                    return;

                if (transport.has_value())
                    onEstablished(*transport);
            });
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

    template <typename C>
    static C toCommand(Message&& m) {return C{{}, std::move(m)};}

private:
    template <typename E>
    MockServer(E&& exec, uint16_t port)
        : listener_(boost::asio::make_strand(exec), port, {Json::id()})
    {}

    void onEstablished(Transporting::Ptr transport)
    {
        session_ = MockServerSession::create(std::move(transport),
                                             std::move(responses_));
        session_->open();
        // Only open one session; don't listen again.
    }

    StringList responses_;
    AnyIoExecutor executor_;
    MockServerSession::Ptr session_;
    Listener<Tcp> listener_;

    friend class ServerContext;
};

} // namespace internal

} // namespace wamp}

#endif // CPPWAMP_TEST_MOCKROUTER_HPP
