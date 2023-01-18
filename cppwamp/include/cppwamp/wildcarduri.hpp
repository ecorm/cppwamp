/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_URI_HPP
#define CPPWAMP_URI_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for processing URIs. */
//------------------------------------------------------------------------------

#include <ostream>
#include <string>
#include <type_traits>
#include <vector>
#include "erroror.hpp"
#include "tagtypes.hpp"
#include "tokentrie.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a URI split into its constituent labels.

    Provides a subset of vector-like operations, as well as additional
    functions for converting from/to URI strings. To access the complete set
    of vector operations, use the SplitUri::labels accessor. */
//------------------------------------------------------------------------------
class SplitUri
{
public:
    using uri_type         = std::string;
    using label_type       = uri_type;
    using char_type        = typename uri_type::value_type;
    using storage_type     = std::vector<std::string>;
    using value_type       = label_type;
    using allocator_type   = typename storage_type::allocator_type;
    using size_type        = typename storage_type::size_type;
    using difference_type  = typename storage_type::difference_type;
    using reference        = label_type&;
    using const_reference  = const label_type&;
    using pointer          = typename storage_type::pointer;
    using const_pointer    = typename storage_type::const_pointer;
    using iterator         = typename storage_type::iterator;
    using const_iterator   = typename storage_type::const_iterator;

    static constexpr char_type separator = '.';

    SplitUri()
        noexcept(std::is_nothrow_default_constructible<storage_type>::value) {}

    SplitUri(const uri_type& uri): labels_(tokenize(uri)) {}

    SplitUri(const char_type* uri): labels_(tokenize(uri)) {}

    SplitUri(const storage_type& labels): labels_(std::move(labels)) {}

    template <typename... Ts>
    SplitUri(in_place_t, Ts&&... args) : labels_(std::forward<Ts>(args)...) {}

    SplitUri& operator=(const uri_type& uri)
    {
        labels_ = tokenize(uri);
        return *this;
    }

    SplitUri& operator=(std::initializer_list<label_type> list)
    {
        labels_ = list;
        return *this;
    }

    /// @name Label Access
    /// @{
    reference at(size_type pos)                     {return labels_.at(pos);}
    const_reference at(size_type pos) const         {return labels_.at(pos);}
    reference operator[] (size_type pos)            {return labels_[pos];}
    const_reference operator[](size_type pos) const {return labels_[pos];}
    reference front()                               {return labels_.front();}
    const_reference front() const                   {return labels_.front();}
    reference back()                                {return labels_.back();}
    const_reference back() const                    {return labels_.back();}
    ///@}

    /// @name Iterators
    /// @{
    iterator begin() noexcept                       {return labels_.begin();}
    const_iterator begin() const noexcept           {return labels_.begin();}
    const_iterator cbegin() const noexcept          {return labels_.cbegin();}
    iterator end() noexcept                         {return labels_.end();}
    const_iterator end() const noexcept             {return labels_.end();}
    const_iterator cend() const noexcept            {return labels_.cend();}
    /// @}

    /// @name Capacity
    /// @{
    bool empty() const noexcept         {return labels_.empty();}
    size_type size() const noexcept     {return labels_.size();}
    size_type max_size() const noexcept {return labels_.max_size();}
    /// @}

    /// @name Modifiers
    /// @{
    void clear() noexcept               {labels_.clear();}
    void push_back(const label_type& s) {labels_.push_back(s);}
    void push_back(label_type&& s)      {labels_.push_back(std::move(s));}
    void swap(SplitUri& x)
        noexcept(std::is_nothrow_swappable<storage_type>::value)
        {labels_.swap(x.labels_);}
    /// @}

    /// @name Labels
    /// @{
    /** Return an URI string composed of this object's split labels. */
    ErrorOr<uri_type> unsplit() const;

    /** Accesses the underlying container of split labels. */
    storage_type& labels() noexcept {return labels_;}

    /** Accesses the underlying container of split labels. */
    const storage_type& labels() const noexcept {return labels_;}
    /// @}

    /// @name Non-member functions
    /// @{
    friend bool operator==(const SplitUri& a, const SplitUri& b) {return a.labels_ == b.labels_;}
    friend bool operator!=(const SplitUri& a, const SplitUri& b) {return a.labels_ != b.labels_;}
    friend bool operator< (const SplitUri& a, const SplitUri& b) {return a.labels_ <  b.labels_;}
    friend bool operator<=(const SplitUri& a, const SplitUri& b) {return a.labels_ <= b.labels_;}
    friend bool operator> (const SplitUri& a, const SplitUri& b) {return a.labels_ >  b.labels_;}
    friend bool operator>=(const SplitUri& a, const SplitUri& b) {return a.labels_ >= b.labels_;}

    friend void swap(SplitUri& a, SplitUri& b) {a.swap(b);}

    friend std::ostream& operator<<(const SplitUri& x, std::ostream& out)
    {
        return out << x.unsplit().value_or("<null>");
    }
    /// @}

private:
    static storage_type tokenize(const uri_type uri);

    static uri_type untokenize(const storage_type& labels);

    storage_type labels_;
};

//------------------------------------------------------------------------------
/** Obtains the URI wildcard label.
    @relates SplitUri */
//------------------------------------------------------------------------------
inline SplitUri::label_type wildcardLabel() {return SplitUri::label_type();}

//------------------------------------------------------------------------------
/** Determines if the given uri label is a wildcard.
    @relates SplitUri */
//------------------------------------------------------------------------------
inline bool isWildcardLabel(const SplitUri::label_type& label)
{
    return label.empty();
}

//------------------------------------------------------------------------------
/** Determines if the given SplitUri matches the given wildcard pattern.
    @relates SplitUri */
//------------------------------------------------------------------------------
bool wildcardMatches(const SplitUri& uri, const SplitUri& pattern);


//------------------------------------------------------------------------------
template <typename T>
using UriTrie = TokenTrie<SplitUri, T>;

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/wildcarduri.ipp"
#endif

#endif // CPPWAMP_URI_HPP
