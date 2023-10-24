/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_MOCKCLIENT_HPP
#define CPPWAMP_TEST_MOCKCLIENT_HPP

#include <cassert>
#include <deque>
#include <vector>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/internal/message.hpp>
#include <cppwamp/transports/tcpclient.hpp>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class MockClient : public std::enable_shared_from_this<MockClient>
{
public:
    using Ptr = std::shared_ptr<MockClient>;
    using StringList = std::vector<std::string>;
    using Requests = std::deque<StringList>;
    using MessageList = std::vector<Message>;

    template <typename E>
    static Ptr create(E&& exec, uint16_t port)
    {
        return Ptr(new MockClient(std::forward<E>(exec), port));
    }

    void load(Requests cannedRequests)
    {
        requests_ = std::move(cannedRequests);
        messages_.clear();
    }

    void connect(YieldContext yield)
    {
        std::weak_ptr<MockClient> self{shared_from_this()};
        connector_.establish(
            [self](ErrorOr<Transporting::Ptr> transport)
            {
                auto me = self.lock();
                if (me && transport.has_value())
                    me->onEstablished(*transport);
            });

        auto exec = boost::asio::get_associated_executor(yield);
        while (!isConnected())
            boost::asio::post(exec, yield);
    }

    void disconnect()
    {
        if (!transport_)
            return;
        transport_->kill();
        transport_.reset();
    }

    bool isConnected() {return transport_ != nullptr;}

    const MessageList& messages() const {return messages_;}

    MessageKind lastMessageKind() const
    {
        return messages_.empty() ? MessageKind::none : messages_.back().kind();
    }

    template <typename C>
    static C toCommand(Message&& m) {return C{PassKey{}, std::move(m)};}

private:
    template <typename E>
    MockClient(E&& exec, uint16_t port)
        : connector_(boost::asio::make_strand(exec),
                     {"localhost", port},
                     {Json::id()})
    {}

    void onEstablished(Transporting::Ptr transport)
    {
        transport_ = std::move(transport);
        auto self = std::weak_ptr<MockClient>(shared_from_this());
        transport_->start(
            [self](ErrorOr<MessageBuffer> b)
            {
                auto me = self.lock();
                if (me)
                    me->onMessage(std::move(b));
            },
            [](std::error_code) {});

        assert(!requests_.empty());
        sendNextRequestBatch();
    }

    void onMessage(ErrorOr<MessageBuffer> buffer)
    {
        if (!buffer)
            return;
        Variant v;
        auto ec = decoder_.decode(*buffer, v);
        assert(!ec);
        auto message = Message::parse(std::move(v.as<Array>()));
        messages_.push_back(message.value());

        if (!requests_.empty())
            sendNextRequestBatch();
    }

    void sendNextRequestBatch()
    {
        const auto& batch = requests_.front();
        for (const auto& json: batch)
        {
            auto ptr = reinterpret_cast<const uint8_t*>(json.data());
            MessageBuffer buffer{ptr, ptr + json.size()};
            transport_->send(std::move(buffer));
        }
        requests_.pop_front();
    }

    Requests requests_;
    AnyIoExecutor executor_;
    Connector<Tcp> connector_;
    Transporting::Ptr transport_;
    JsonBufferDecoder decoder_;
    MessageList messages_;

    friend class ServerContext;
};

} // namespace internal

} // namespace wamp}

#endif // CPPWAMP_TEST_MOCKCLIENT_HPP
