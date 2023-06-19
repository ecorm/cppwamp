/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIRECTPEER_HPP
#define CPPWAMP_INTERNAL_DIRECTPEER_HPP

#include <memory>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../router.hpp"
#include "../version.hpp"
#include "message.hpp"
#include "peer.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{

class DirectPeer;

//------------------------------------------------------------------------------
class DirectRouterSession : public RouterSession
{
public:
    using Ptr = std::shared_ptr<DirectRouterSession>;
    using WeakPtr = std::weak_ptr<DirectRouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    DirectRouterSession(DirectPeer& peer);

    void connect(DirectRouterLink&& link);

    Object open(Petition&& hello);

    void close();

    void disconnect();

private:
    using Base = RouterSession;

    void onRouterAbort(Reason&& r) override;
    void onRouterMessage(Message&& msg) override;
    
    AuthInfo authInfo_;
    DirectPeer* peer_ = nullptr;
};

//------------------------------------------------------------------------------
// Provides direct in-process communications with a router.
//------------------------------------------------------------------------------
class DirectPeer : public Peer
{
public:
    using State = SessionState;

    DirectPeer()
        : Base(false),
          session_(std::make_shared<DirectRouterSession>(*this))
    {}

    ~DirectPeer() override {realm_.leave(session_);}

    DirectPeer(const DirectPeer&) = delete;
    DirectPeer(DirectPeer&&) = delete;
    DirectPeer& operator=(const DirectPeer&) = delete;
    DirectPeer& operator=(DirectPeer&&) = delete;

private:
    using Base = Peer;
    using IoStrandPtr = std::unique_ptr<IoStrand>;

    void onDirectConnect(IoStrand strand, any routerLink) override
    {
        if (!strand_)
            strand_ = IoStrandPtr(new IoStrand(std::move(strand)));
        auto link = any_cast<DirectRouterLink>(std::move(routerLink));
        router_ = RouterContext{link.router({})};
        session_->connect(std::move(link));
        setState(State::closed);
        session_->report({AccessAction::clientConnect});
    }

    void onConnect(Transporting::Ptr t, AnyBufferCodec c) override
    {
        CPPWAMP_LOGIC_ERROR("Cannot connect a wamp::DirectionSession via "
                            " a wamp::Session base class reference/pointer");
    }

    void onEstablish() override {/* Nothing to do*/}

    void onClose() override {session_->close();}

    void onDisconnect(State previousState) override
    {
        session_->close();
        auto s = previousState;
        if (s == State::established || s == State::shuttingDown)
            realm_.leave(session_);
        session_->report({AccessAction::clientDisconnect});
        router_.reset();
        realm_.reset();
        session_->disconnect();
    }

    ErrorOrDone send(Petition&& hello) override
    {
        assert(state() == State::establishing);
        traceTx(hello.message({}));

        // Trim verbose feature dictionaries before logging
        auto helloActionInfo = hello.info();
        helloActionInfo.options.erase("roles");
        session_->report(std::move(helloActionInfo));

        auto realm = router_.realmAt(hello.uri());
        if (realm.expired())
        {
            return fail("Realm '" + hello.uri() + "' not found",
                        WampErrc::noSuchRealm);
        }
        realm_ = std::move(realm);
        auto details = session_->open(std::move(hello));
        if (!realm_.join(session_))
            return fail("Realm expired", WampErrc::noSuchRealm);

        setState(State::established);
        Welcome welcome{{}, session_->wampId(), std::move(details)};

        struct Posted
        {
            Ptr self;
            Welcome welcome;

            void operator()()
            {
                auto& me = dynamic_cast<DirectPeer&>(*self);
                me.traceRx(welcome.message({}));
                me.session_->report(welcome.info());
                me.listener().onPeerMessage(std::move(welcome.message({})));
            }
        };

        boost::asio::post(*strand_,
                          Posted{shared_from_this(), std::move(welcome)});
        return true;
    }

    ErrorOrDone send(Reason&& goodbye) override
    {
        assert(state() == State::shuttingDown);
        traceTx(goodbye.message({}));
        session_->report(goodbye.info(false));
        realm_.leave(session_);
        realm_.reset();
        close();
        Reason reason{errorCodeToUri(WampErrc::goodbyeAndOut)};
        session_->report(reason.info(true));

        struct Posted
        {
            Ptr self;
            Reason reason;

            void operator()()
            {
                auto& me = dynamic_cast<DirectPeer&>(*self);
                me.listener().onPeerGoodbye(std::move(reason), true);
            }
        };

        boost::asio::post(*strand_,
                          Posted{shared_from_this(), std::move(reason)});
        return true;
    }

    ErrorOrDone send(Error&& c) override             {return sendCommand(c);}

    ErrorOrDone send(Topic&& c) override             {return sendCommand(c);}
    ErrorOrDone send(Pub&& c) override               {return sendCommand(c);}
    ErrorOrDone send(Unsubscribe&& c) override       {return sendCommand(c);}
    ErrorOrDone send(Procedure&& c) override         {return sendCommand(c);}
    ErrorOrDone send(Rpc&& c) override               {return sendCommand(c);}
    ErrorOrDone send(Result&& c) override            {return sendCommand(c);}
    ErrorOrDone send(CallCancellation&& c) override  {return sendCommand(c);}
    ErrorOrDone send(Unregister&& c) override        {return sendCommand(c);}

    ErrorOrDone send(Stream&& c) override {return sendAs<Procedure>(c);}
    ErrorOrDone send(StreamRequest&& c) override     {return sendAs<Rpc>(c);}
    ErrorOrDone send(CallerOutputChunk&& c) override {return sendAs<Rpc>(c);}

    ErrorOrDone send(CalleeOutputChunk&& c) override
    {
        Result result{{}, std::move(c.message({}))};
        result.setKindToYield({});
        return sendCommand(result);
    }

    ErrorOrDone send(Welcome&& c) override           {return badCommand();}
    ErrorOrDone send(Authentication&& c) override    {return badCommand();}
    ErrorOrDone send(Challenge&& c) override         {return badCommand();}
    ErrorOrDone send(Published&& c) override         {return badCommand();}
    ErrorOrDone send(Event&& c) override             {return badCommand();}
    ErrorOrDone send(Subscribed&& c) override        {return badCommand();}
    ErrorOrDone send(Unsubscribed&& c) override      {return badCommand();}
    ErrorOrDone send(Invocation&& c) override        {return badCommand();}
    ErrorOrDone send(Interruption&& c) override      {return badCommand();}
    ErrorOrDone send(Registered&& c) override        {return badCommand();}
    ErrorOrDone send(Unregistered&& c) override      {return badCommand();}

    ErrorOrDone abort(Reason reason) override
    {
        traceTx(reason.message({}));
        session_->report(reason.info(false));
        bool ready = readyToAbort();
        disconnect();
        if (!ready)
            return makeUnexpectedError(MiscErrc::invalidState);
        return true;
    }

    template <typename C>
    ErrorOrDone sendCommand(C& command)
    {
        traceTx(command.message({}));
        if (!realm_.send(session_, std::move(command)))
            return fail("Realm expired", WampErrc::noSuchRealm);
        return true;
    }

    template <typename TDesired, typename C>
    ErrorOrDone sendAs(C& command)
    {
        TDesired desired{{}, std::move(command.message({}))};
        return sendCommand(desired);
    }

    bool badCommand()
    {
        assert(false);
        return false;
    }

    void onAbort(Reason&& reason)
    {
        struct Posted
        {
            Ptr self;
            Reason reason;

            void operator()()
            {
                auto& me = dynamic_cast<DirectPeer&>(*self);
                me.traceRx(reason.message({}));
                me.listener().onPeerAbort(std::move(reason), false);
            }
        };

        setState(State::failed);
        boost::asio::post(*strand_,
                          Posted{shared_from_this(), std::move(reason)});
    };

    void onMessage(Message&& msg)
    {
        struct Posted
        {
            Ptr self;
            Message m;

            void operator()()
            {
                auto& me = dynamic_cast<DirectPeer&>(*self);
                me.traceRx(m);
                me.listener().onPeerMessage(std::move(m));
            }
        };

        boost::asio::post(*strand_,
                          Posted{shared_from_this(), std::move(msg)});
    }

    UnexpectedError fail(std::string why, std::error_code ec)
    {
        struct Posted
        {
            Ptr self;
            std::string why;
            std::error_code ec;

            void operator()()
            {
                auto& me = dynamic_cast<DirectPeer&>(*self);
                me.listener().onPeerFailure(ec, false, std::move(why));
            }
        };

        setState(State::failed);
        realm_.leave(session_);
        realm_.reset();
        boost::asio::post(*strand_,
                          Posted{shared_from_this(), std::move(why), ec});
        return UnexpectedError(ec);
    }

    template <typename TErrc>
    UnexpectedError fail(std::string why, TErrc errc)
    {
        return fail(std::move(why), make_error_code(errc));
    }

    bool readyToAbort() const
    {
        auto s = state();
        return s == State::establishing ||
               s == State::authenticating ||
               s == State::established;
    }

    IoStrandPtr strand_;
    DirectRouterSession::Ptr session_;
    RouterContext router_;
    RealmContext realm_;

    friend class DirectRouterSession;
};


//******************************************************************************
/** DirectRouterSession member function definitions. */
//******************************************************************************

inline DirectRouterSession::DirectRouterSession(DirectPeer& p) : peer_(&p) {}

inline void DirectRouterSession::connect(DirectRouterLink&& link)
{
    authInfo_ = std::move(link.authInfo({}));

    RouterContext router{link.router({})};
    Base::setRouterLogger(router.logger());
    std::string endpointLabel;
    if (link.endpointLabel({}).empty())
        endpointLabel = "direct";
    else
        endpointLabel = std::move(link.endpointLabel({}));
    Object transport{{"protocol", "direct"},
                     {"endpoint", endpointLabel}};
    ConnectionInfo info{std::move(transport), std::move(endpointLabel)};
    info.setServer({}, "direct", router.nextDirectSessionIndex());
    Base::connect(std::move(info));
}

inline Object DirectRouterSession::open(Petition&& hello)
{
    if (!hello.hasOption("authid"))
        hello.withAuthId(authInfo_.id());
    else if (authInfo_.id().empty())
        authInfo_.setId({}, hello.authId().value_or(""));

    Base::open(hello);
    auto info = internal::SessionInfoImpl::create(authInfo_);
    info->setAgent(Version::agentString(), ClientFeatures::provided());
    auto welcomeDetails = info->join(hello.uri());
    Base::join(std::move(info));
    return welcomeDetails;
}

inline void DirectRouterSession::close() {Base::close();}

inline void DirectRouterSession::disconnect() {Base::setRouterLogger(nullptr);}

inline void DirectRouterSession::onRouterAbort(Reason&& r)
{
    peer_->onAbort(std::move(r));
}

void DirectRouterSession::onRouterMessage(Message&& msg)
{
    peer_->onMessage(std::move(msg));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
