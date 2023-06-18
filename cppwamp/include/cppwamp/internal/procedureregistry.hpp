/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PROCEDUREREGISTRY_HPP
#define CPPWAMP_INTERNAL_PROCEDUREREGISTRY_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../calleestreaming.hpp"
#include "../erroror.hpp"
#include "../registration.hpp"
#include "../rpcinfo.hpp"
#include "peer.hpp"
#include "slotlink.hpp"
#include "streamchannel.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct ProcedureRegistration
{
    using CallSlot = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;

    ProcedureRegistration(CallSlot&& cs, InterruptSlot&& is, Uri uri,
                          ClientContext ctx)
        : callSlot(std::move(cs)),
          interruptSlot(std::move(is)),
          uri(std::move(uri)),
          link(RegistrationLink::create(std::move(ctx)))
    {}

    void setRegistrationId(RegistrationId rid) {link->setKey(rid);}

    RegistrationId registrationId() const {return link->key();}

    CallSlot callSlot;
    InterruptSlot interruptSlot;
    Uri uri;
    RegistrationLink::Ptr link;
};


//------------------------------------------------------------------------------
struct StreamRegistration
{
    using StreamSlot = AnyReusableHandler<void (CalleeChannel)>;

    StreamRegistration(StreamSlot&& ss, Uri uri, ClientContext ctx,
                       bool invitationExpected)
        : streamSlot(std::move(ss)),
        uri(std::move(uri)),
        link(RegistrationLink::create(std::move(ctx))),
        invitationExpected(invitationExpected)
    {}

    void setRegistrationId(RegistrationId rid) {link->setKey(rid);}

    RegistrationId registrationId() const {return link->key();}

    StreamSlot streamSlot;
    Uri uri;
    RegistrationLink::Ptr link;
    bool invitationExpected;
};

//------------------------------------------------------------------------------
struct InvocationRecord
{
    InvocationRecord(RegistrationId regId) : registrationId(regId) {}

    CalleeChannelImpl::WeakPtr channel;
    RegistrationId registrationId;
    bool invoked = false;     // Set upon the first streaming invocation
    bool interrupted = false; // Set when an interruption was received
                              //     for this invocation.
    bool moot = false;        // Set when auto-responding to an interruption
                              //     with an error.
    bool closed = false;      // Set when the initiating or subsequent
                              //     invocation is not progressive
};

//------------------------------------------------------------------------------
class ProcedureRegistry
{
public:
    using CallSlot      = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot = AnyReusableHandler<Outcome (Interruption)>;
    using StreamSlot    = AnyReusableHandler<void (CalleeChannel)>;

    ProcedureRegistry(Peer& peer, AnyIoExecutor exec)
        : executor_(std::move(exec)),
          peer_(peer)
    {}

    ErrorOr<Registration> enroll(ProcedureRegistration&& reg)
    {
        auto regId = reg.registrationId();
        auto emplaced = procedures_.emplace(regId, std::move(reg));
        if (!emplaced.second)
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        return Registration{{}, emplaced.first->second.link};
    }

    ErrorOr<Registration> enroll(StreamRegistration&& reg)
    {
        auto regId = reg.registrationId();
        auto emplaced = streams_.emplace(regId, std::move(reg));
        if (!emplaced.second)
            return makeUnexpectedError(WampErrc::procedureAlreadyExists);
        return Registration{{}, emplaced.first->second.link};
    }

    bool unregister(RegistrationId regId)
    {
        bool erased = procedures_.erase(regId) != 0;
        if (!erased)
            erased = streams_.erase(regId) != 0;
        return erased;
    }

    ErrorOrDone yield(Result&& result)
    {
        auto reqId = result.requestId({});
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        bool erased = !result.isProgress({}) || moot;
        if (erased)
            invocations_.erase(found);
        if (moot)
            return false;

        result.setKindToYield({});
        auto done = peer_.send(std::move(result));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.send(Error{{}, MessageKind::invocation, reqId,
                             WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(CalleeOutputChunk&& chunk)
    {
        auto reqId = chunk.requestId({});
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        bool erased = chunk.isFinal() || moot;
        if (erased)
            invocations_.erase(found);
        if (moot)
            return false;

        auto done = peer_.send(std::move(chunk));
        if (done == makeUnexpectedError(WampErrc::payloadSizeExceeded))
        {
            if (!erased)
                invocations_.erase(found);
            peer_.send(Error{{}, MessageKind::invocation, reqId,
                             WampErrc::payloadSizeExceeded});
        }
        return done;
    }

    ErrorOrDone yield(Error&& error)
    {
        auto reqId = error.requestId({});
        auto found = invocations_.find(reqId);
        if (found == invocations_.end())
            return false;

        // Error may have already been returned due to interruption being
        // handled by Client::onInterrupt.
        bool moot = found->second.moot;
        invocations_.erase(found);
        if (moot)
            return false;

        error.setRequestKind({}, MessageKind::invocation);
        return peer_.send(std::move(error));
    }

    WampErrc onInvocation(Invocation&& inv)
    {
        auto regId = inv.registrationId();

        {
            auto kv = procedures_.find(regId);
            if (kv != procedures_.end())
            {
                return onProcedureInvocation(inv, kv->second);
            }
        }

        {
            auto kv = streams_.find(regId);
            if (kv != streams_.end())
            {
                return onStreamInvocation(inv, kv->second);
            }
        }

        return WampErrc::noSuchProcedure;
    }

    void onInterrupt(Interruption&& intr)
    {
        auto found = invocations_.find(intr.requestId());
        if (found == invocations_.end())
            return;

        InvocationRecord& rec = found->second;
        if (rec.interrupted)
            return;
        rec.interrupted = true;
        intr.setRegistrationId({}, rec.registrationId);

        bool interruptHandled = false;

        {
            auto kv = procedures_.find(rec.registrationId);
            if (kv != procedures_.end())
                interruptHandled = onProcedureInterruption(intr, kv->second);
        }

        {
            auto kv = streams_.find(rec.registrationId);
            if (kv != streams_.end())
                interruptHandled = postStreamInterruption(intr, rec);
        }

        if (!interruptHandled)
            automaticallyRespondToInterruption(intr, rec);
    }

    const Uri& lookupProcedureUri(RegistrationId regId) const
    {
        static const Uri empty;
        auto found = procedures_.find(regId);
        if (found == procedures_.end())
            return empty;
        return found->second.uri;
    }

    const Uri& lookupStreamUri(RegistrationId regId) const
    {
        static const Uri empty;
        auto found = streams_.find(regId);
        if (found == streams_.end())
            return empty;
        return found->second.uri;
    }

    void abandonAllStreams(std::error_code ec)
    {
        for (auto& kv: invocations_)
        {
            InvocationRecord& rec = kv.second;
            auto ch = rec.channel.lock();
            if (ch)
                ch->abandon(ec);
        }
    }

    void clear()
    {
        procedures_.clear();
        streams_.clear();
        invocations_.clear();
    }

private:
    using InvocationMap = std::map<RequestId, InvocationRecord>;
    using ProcedureMap = std::map<RegistrationId, ProcedureRegistration>;
    using StreamMap = std::map<RegistrationId, StreamRegistration>;

    WampErrc onProcedureInvocation(Invocation& inv,
                                   const ProcedureRegistration& reg)
    {
        // Progressive calls not allowed on procedures not registered
        // as streams.
        if (inv.isProgress({}) || inv.resultsAreProgressive({}))
            return WampErrc::optionNotAllowed;

        auto requestId = inv.requestId();
        auto registrationId = inv.registrationId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{registrationId});

        // Detect attempt to reinvoke same pending call
        if (!emplaced.second)
            return WampErrc::protocolViolation;

        auto& invocationRec = emplaced.first->second;
        invocationRec.closed = true;
        postRpcRequest(reg.callSlot, inv, reg.link);
        return WampErrc::success;
    }

    bool onProcedureInterruption(Interruption& intr,
                                 const ProcedureRegistration& reg)
    {
        if (reg.interruptSlot == nullptr)
            return false;
        postRpcRequest(reg.interruptSlot, intr, reg.link);
        return true;
    }

    template <typename TSlot, typename TInvocationOrInterruption>
    void postRpcRequest(TSlot slot, TInvocationOrInterruption& request,
                        RegistrationLink::Ptr link)
    {
        struct Posted
        {
            TSlot slot;
            TInvocationOrInterruption request;
            RegistrationLink::Ptr link;

            void operator()()
            {
                if (!link->armed())
                    return;

                // Copy the request ID before the request object gets moved away.
                auto requestId = request.requestId();

                try
                {
                    Outcome outcome(slot(std::move(request)));
                    switch (outcome.type())
                    {
                    case Outcome::Type::deferred:
                        // Do nothing
                        break;

                    case Outcome::Type::result:
                    {
                        auto callee = link->context();
                        auto regId = link->key();
                        callee.yieldResult(std::move(outcome).asResult(),
                                           requestId, regId);
                        break;
                    }

                    case Outcome::Type::error:
                    {
                        auto callee = link->context();
                        auto regId = link->key();
                        callee.yieldError(std::move(outcome).asError(),
                                          requestId, regId);
                        break;
                    }

                    default:
                        assert(false && "unexpected Outcome::Type");
                    }
                }
                catch (Error& error)
                {
                    auto callee = link->context();
                    auto regId = link->key();
                    callee.yieldError(std::move(error), requestId, regId);
                }
                catch (const error::BadType& e)
                {
                    // Forward Variant conversion exceptions as ERROR messages.
                    auto callee = link->context();
                    auto regId = link->key();
                    callee.yieldError(Error{e}, requestId, regId);
                }
            }
        };

        auto slotExec = boost::asio::get_associated_executor(slot);
        request.setExecutor({}, slotExec);
        Posted posted{std::move(slot), std::move(request), link};
        boost::asio::post(
            executor_,
            boost::asio::bind_executor(slotExec, std::move(posted)));
    }

    WampErrc onStreamInvocation(Invocation& inv, const StreamRegistration& reg)
    {
        if (!reg.link->armed())
            return WampErrc::noSuchProcedure;

        auto requestId = inv.requestId();
        auto registrationId = inv.registrationId();
        auto emplaced = invocations_.emplace(requestId,
                                             InvocationRecord{registrationId});
        auto& invocationRec = emplaced.first->second;
        if (invocationRec.closed)
            return WampErrc::protocolViolation;

        invocationRec.closed = !inv.isProgress({});
        processStreamInvocation(reg, invocationRec, inv);
        return WampErrc::success;
    }

    void processStreamInvocation(
        const StreamRegistration& reg, InvocationRecord& rec, Invocation& inv)
    {
        if (!rec.invoked)
        {
            auto exec = boost::asio::get_associated_executor(reg.streamSlot);
            inv.setExecutor({}, std::move(exec));
            auto channel = std::make_shared<CalleeChannelImpl>(
                std::move(inv), reg.invitationExpected, executor_);
            rec.channel = channel;
            rec.invoked = true;
            CalleeChannel proxy{{}, channel};

            try
            {
                // Execute the slot directly from this strand in order to avoid
                // a race condition between accept and
                // postInvocation/postInterrupt on the CalleeChannel.
                reg.streamSlot(std::move(proxy));
            }
            catch (Error& error)
            {
                channel->fail(std::move(error));
            }
            catch (const error::BadType& e)
            {
                // Forward Variant conversion exceptions as ERROR messages.
                channel->fail(Error(e));
            }
        }
        else
        {
            auto channel = rec.channel.lock();
            if (channel)
                channel->postInvocation(std::move(inv));
        }
    }

    bool postStreamInterruption(Interruption& intr, InvocationRecord& rec)
    {
        auto channel = rec.channel.lock();
        return bool(channel) && channel->postInterrupt(std::move(intr));
    }

    void automaticallyRespondToInterruption(Interruption& intr,
                                            InvocationRecord& rec)
    {
        // Respond immediately when cancel mode is 'kill' and no interrupt
        // slot is provided.
        // Dealer will have already responded in `killnowait` mode.
        // Dealer does not emit an INTERRUPT in `skip` mode.
        if (intr.cancelMode() == CallCancelMode::kill)
        {
            rec.moot = true;
            Error error{intr.reason().value_or(
                errorCodeToUri(WampErrc::cancelled))};
            error.setRequestId({}, intr.requestId());
            error.setRequestKind({}, MessageKind::invocation);
            peer_.send(std::move(error));
        }
    }

    ProcedureMap procedures_;
    StreamMap streams_;
    InvocationMap invocations_;
    AnyIoExecutor executor_;
    Peer& peer_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PROCEDUREREGISTRY_HPP
