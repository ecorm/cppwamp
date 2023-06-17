/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SOCKETOPTIONS_HPP
#define CPPWAMP_SOCKETOPTIONS_HPP

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Base class for polymorphic socket options.
//------------------------------------------------------------------------------
template <typename TProtocol>
class SocketOptionBase
{
public:
    using Protocol = TProtocol;
    using Ptr = std::shared_ptr<SocketOptionBase>;

    virtual ~SocketOptionBase() {}

    virtual int level(const Protocol& p) const = 0;

    virtual int name(const Protocol& p) const = 0;

    virtual const void* data(const Protocol& p) const = 0;

    virtual size_t size(const Protocol& p) const = 0;
};

//------------------------------------------------------------------------------
// Polymorphic wrapper around a Boost Asio socket option.
//------------------------------------------------------------------------------
template <typename TProtocol, typename TOption>
class SocketOptionWrapper : public SocketOptionBase<TProtocol>
{
public:
    using Protocol = TProtocol;
    using Option = TOption;

    SocketOptionWrapper(TOption&& option) : option_(std::move(option)) {}

    virtual int level(const Protocol& p) const override
        {return option_.level(p);}

    virtual int name(const Protocol& p) const override
        {return option_.name(p);}

    virtual const void* data(const Protocol& p) const override
        {return option_.data(p);}

    virtual size_t size(const Protocol& p) const override
        {return option_.size(p);}

private:
    TOption option_;
};

//------------------------------------------------------------------------------
// Polymorphic holder of a Boost Asio socket option.
// Meets the SettableSocketOption requirement of Boost.Asio.
// https://www.boost.org/doc/libs/release/doc/html/boost_asio/reference/SettableSocketOption.html
//------------------------------------------------------------------------------
template <typename TProtocol>
class SocketOption
{
public:
    using Protocol = TProtocol;

    template <typename TOption>
    SocketOption(TOption&& option)
    {
        using DecayedOption = typename std::decay<TOption>::type;
        using ConcreteOption = SocketOptionWrapper<TProtocol, DecayedOption>;
        option_.reset(new ConcreteOption(std::move(option)));
    }

    int level(const Protocol& p) const {return option_->level(p);}

    int name(const Protocol& p) const {return option_->name(p);}

    const void* data(const Protocol& p) const {return option_->data(p);}

    size_t size(const Protocol& p) const {return option_->size(p);}

private:
    typename SocketOptionBase<TProtocol>::Ptr option_;
};

//------------------------------------------------------------------------------
// Generic socket option container
//------------------------------------------------------------------------------
template <typename TProtocol>
class SocketOptionList
{
public:
    using Protocol = TProtocol;
    using Option = SocketOption<Protocol>;

    template <typename TOption>
    void add(TOption&& option)
    {
        options_.emplace_back(std::forward<TOption>(option));
    }

    template <typename TSocket>
    void applyTo(TSocket& socket) const
    {
        for (const auto& opt: options_)
            socket.set_option(opt);
    }

private:
    std::vector<Option> options_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_SOCKETOPTIONS_HPP
