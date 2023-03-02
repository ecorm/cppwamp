/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_STREAMING_HPP
#define CPPWAMP_STREAMING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains common streaming facilities. */
//------------------------------------------------------------------------------

#include "payload.hpp"
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/// Ephemeral ID associated with a streaming channel
//------------------------------------------------------------------------------
using ChannelId = RequestId;


//------------------------------------------------------------------------------
/// Enumerates the possible streaming modes.
//------------------------------------------------------------------------------
enum class StreamMode
{
    simpleCall,     ///< No progressive calls results or invocations.
    calleeToCaller, ///< Progressive call results only.
    callerToCallee, ///< Progressive call invocations only.
    bidirectional   ///< Both progressive calls results and invocations
};


//------------------------------------------------------------------------------
/** Consolidates common properties of streaming chunks. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TMessage>
class CPPWAMP_API Chunk : public Payload<TDerived, TMessage>
{
public:
    /** Indicates if the chunk is the final one. */
    bool isFinal() const {return isFinal_;}

private:
    using Base = Payload<TDerived, TMessage>;

    bool isFinal_ = false;

protected:
    Chunk() = default;

    explicit Chunk(bool isFinal)
        : isFinal_(isFinal)
    {
        if (!this->isFinal())
            withOption("progress", true);
    }

    explicit Chunk(TMessage&& msg)
        : Base(std::move(msg))
    {
        isFinal_ = !this->template optionOr<bool>("progress", false);
    }

    // Disallow the user setting options.
    using Base::withOption;
    using Base::withOptions;
};

} // namespace wamp

#endif // CPPWAMP_STREAMING_HPP
