/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DIRECTPEER_HPP
#define CPPWAMP_INTERNAL_DIRECTPEER_HPP

#include "../errorcodes.hpp"
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

    using RouterSession::setRouterLogger;
    using RouterSession::routerLogLevel;
    using RouterSession::routerLog;
    using RouterSession::setTransportInfo;
    using RouterSession::setHelloInfo;
    using RouterSession::setWelcomeInfo;
    using RouterSession::resetSessionInfo;

private:
    void onRouterAbort(Reason&& r) override;
    void onRouterCommand(Error&& e) override;
    void onRouterCommand(Subscribed&& s) override;
    void onRouterCommand(Unsubscribed&& u) override;
    void onRouterCommand(Published&& p) override;
    void onRouterCommand(Event&& e) override;
    void onRouterCommand(Registered&& r) override;
    void onRouterCommand(Unregistered&& u) override;
    void onRouterCommand(Result&& r) override;
    void onRouterCommand(Interruption&& i) override;
    void onRouterCommand(Invocation&& i) override;

    DirectPeer& peer_;
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

    ~DirectPeer()
    {
        realm_.leave(session_->wampId());
    }

private:
    using Base = Peer;

    void onDirectConnect(any link) override
    {
        router_ = any_cast<RouterContext>(std::move(link));
        assert(!router_.expired());

        session_->setRouterLogger(router_.logger());
        auto n = router_.nextDirectSessionIndex();
        session_->setTransportInfo({"direct", "direct", n});

        setState(State::closed);
        session_->report({AccessAction::clientConnect});
    }

    void onClose() override
    {
        session_->resetSessionInfo();
    }

    void onDisconnect(State previousState) override
    {
        session_->resetSessionInfo();
        auto s = previousState;
        if (s == State::established || s == State::shuttingDown)
            realm_.leave(session_->wampId());
        session_->report({AccessAction::clientDisconnect});
        router_.reset();
        session_->setRouterLogger(nullptr);
    }

    ErrorOrDone send(Realm&& hello) override
    {
        assert(state() == State::establishing);
        traceTx(hello.message({}));
        session_->report(hello.info());

        auto realm = router_.realmAt(hello.uri());
        bool found = false;
        if (!realm.expired())
        {
            realm_ = std::move(realm);
            found = realm_.join(session_);
        }
        if (!found)
        {
            return fail("Realm '" + hello.uri() + "' not found",
                        WampErrc::noSuchRealm);
        }

        AuthInfo authInfo
        {
            hello.authId().value_or(""),
            hello.optionOr<String>("authrole", ""),
            hello.optionOr<String>("authmethod", "x_cppwamp_direct"),
            hello.optionOr<String>("authprovider", "direct")
        };
        session_->setHelloInfo(hello);
        session_->setWelcomeInfo(std::move(authInfo));

        setState(State::established);
        return true;
    }

    ErrorOrDone send(Reason&& goodbye) override
    {
        assert(state() == State::shuttingDown);
        traceTx(goodbye.message({}));
        session_->report(goodbye.info(false));
        realm_.leave(session_->wampId());
        realm_.reset();
        close();
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
        traceRx(reason.message({}));
        setState(State::failed);
        listener().onPeerAbort(std::move(reason), false);
    };

    template <typename C>
    void onCommand(C&& command)
    {
        traceRx(command.message({}));
        listener().onPeerCommand(std::move(command));
    }

    UnexpectedError fail(std::string why, std::error_code ec)
    {
        setState(State::failed);
        realm_.leave(session_->wampId());
        listener().onPeerFailure(ec, false, std::move(why));
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

    DirectRouterSession::Ptr session_;
    RouterContext router_;
    RealmContext realm_;

    friend class DirectRouterSession;
};


//******************************************************************************
/** DirectRouterSession member function definitions. */
//******************************************************************************

inline DirectRouterSession::DirectRouterSession(DirectPeer& peer) : peer_(peer) {}
inline void DirectRouterSession::onRouterAbort(Reason&& r)         {peer_.onAbort(std::move(r));};
inline void DirectRouterSession::onRouterCommand(Error&& e)        {peer_.onCommand(std::move(e));}
inline void DirectRouterSession::onRouterCommand(Subscribed&& s)   {peer_.onCommand(std::move(s));}
inline void DirectRouterSession::onRouterCommand(Unsubscribed&& u) {peer_.onCommand(std::move(u));}
inline void DirectRouterSession::onRouterCommand(Published&& p)    {peer_.onCommand(std::move(p));}
inline void DirectRouterSession::onRouterCommand(Event&& e)        {peer_.onCommand(std::move(e));}
inline void DirectRouterSession::onRouterCommand(Registered&& r)   {peer_.onCommand(std::move(r));}
inline void DirectRouterSession::onRouterCommand(Unregistered&& u) {peer_.onCommand(std::move(u));}
inline void DirectRouterSession::onRouterCommand(Result&& r)       {peer_.onCommand(std::move(r));}
inline void DirectRouterSession::onRouterCommand(Interruption&& i) {peer_.onCommand(std::move(i));}
inline void DirectRouterSession::onRouterCommand(Invocation&& i)   {peer_.onCommand(std::move(i));}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DIRECTPEER_HPP
