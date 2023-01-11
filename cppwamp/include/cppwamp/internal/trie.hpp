/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TRIE_HPP
#define CPPWAMP_INTERNAL_TRIE_HPP

#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <tuple>
#include <type_traits>
#include <vector>

#ifdef CPPWAMP_WITHOUT_BUNDLED_TESSIL_HTRIE
#include <tsl/htrie_map.h>
#else
#include "../bundled/tessil_htrie/htrie_map.h"
#endif

#include "../uri.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <class CharT, class T, class Hash = tsl::ah::str_hash<CharT>,
          class KeySizeT = std::uint16_t>
using TrieMap = tsl::htrie_map<CharT, T, Hash, KeySizeT>;


//------------------------------------------------------------------------------
template <typename T>
struct WildcardTrieNode
{
    using Value = T;
    using StringType = typename SplitUri::value_type;
    using Tree = std::map<StringType, WildcardTrieNode>;
    using TreeIterator = typename Tree::iterator;

    WildcardTrieNode() : iter(children.end()) {}

    template <typename... Us>
    WildcardTrieNode(WildcardTrieNode* parent, bool isTerminal, Us&&... args)
        : value(std::forward<Us>(args)...),
          parent(parent),
          isTerminal(isTerminal)
    {}

    template <typename... Us>
    TreeIterator addTerminal(StringType label, Us&&... args)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(this, true, std::forward<Us>(args)...));
        assert(result.second);
        result.first->second.iter = result.first;
        return result.first;
    }

    template <typename... Us>
    TreeIterator addLink(StringType label)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(this, false));
        result.first->second.iter = result.first;
        return result.first;
    }

    template <typename... Us>
    void setValue(Us&&... args)
    {
        value = Value(std::forward<Us>(args)...);
        isTerminal = true;
    }

    void clear()
    {
        value = Value();
        isTerminal = false;
    }

    void swap(WildcardTrieNode& other) noexcept
    {
        children.swap(other.children);
        using std::swap;
        swap(value, other.value);
        swap(isTerminal, other.isTerminal);
    }

    bool isRoot() const {return parent == nullptr;}

    bool isLeaf() const {return children.empty();}

    Tree children;
    Value value = {};
    TreeIterator iter = {};
    WildcardTrieNode* parent = nullptr;

    bool isTerminal = false;
};


//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieContext
{
    using Node = WildcardTrieNode<T>;
    using TreeIterator = typename Node::TreeIterator;
    using Key = SplitUri;
    using Level = typename Key::size_type;

    static WildcardTrieContext begin(Node& root)
    {
        return WildcardTrieContext(root, root.children.begin());
    }

    static WildcardTrieContext end(Node& root)
    {
        return WildcardTrieContext(root, root.children.end());
    }

    WildcardTrieContext() = default;

    template <typename... Us>
    bool set(bool clobber, Key key, Us&&... args)
    {
        if (key.empty())
            return false;

        bool placed = false;
        const auto tokenCount = key.size();

        for (SplitUri::size_type i = 0; i < tokenCount - 1; ++i)
        {
            auto& token = key[i];
            iter = parent->addLink(std::move(token));
            parent = iter->second.parent;
        }

        auto& token = key[tokenCount - 1];
        iter = parent->children.find(token);
        if (iter == parent->children.end())
        {
            placed = true;
            iter = parent->addTerminal(std::move(token),
                                       std::forward<Us>(args)...);
        }
        else if (clobber)
        {
            iter->second.setValue(std::forward<Us>(args)...);
        }
        parent = iter->second.parent;

        return placed;
    }

    void locate(const Key& key)
    {
        if (key.empty())
            return end;

        Node* root = parent;
        const TreeIterator end = parent->children.end();
        bool found = true;

        const auto labelCount = key.size();
        for (typename Key::size_type i = 0; i<labelCount; ++i)
        {
            const auto& label = key[i];
            iter = parent->children.find(label);
            if (iter == parent->children.end())
            {
                found = false;
                break;
            }
            parent = iter->second.parent;
        }

        found = found && iter->second.isTerminal;
        if (!found)
        {
            parent = root;
            iter = end;
        }
    }

    void advanceToFirstTerminal()
    {
        if (!isTerminal())
            advanceToNextTerminal();
    }

    void advanceToNextTerminal()
    {
        while (!atEnd())
        {
            advanceDepthFirst();
            if (isTerminal())
                break;
        }
    }

    Level matchFirst(const Key& key)
    {
        Level level = 0;
        if (key.empty())
        {
            iter = parent->children.end();
        }
        else if (!isMatch(key, 0))
        {
            level = matchNext(key, 0);
        }
        return level;
    }

    Level matchNext(const Key& key, Level level)
    {
        while (!atEnd())
        {
            level = tryNextMatchCandidate(key, level);
            if (isMatch(key, level))
                break;
        }
        return level;
    }

    bool atEnd() const
    {
        return parent->isRoot() && (iter == parent->children.end());
    }

    bool operator==(const WildcardTrieContext& rhs) const
    {
        return std::tie(parent, iter) == std::tie(rhs.parent, rhs.iter);
    }

    bool operator!=(const WildcardTrieContext& rhs) const
    {
        return std::tie(parent, iter) != std::tie(rhs.parent, rhs.iter);
    }

    Node* parent = nullptr;
    TreeIterator iter = {};

private:
    WildcardTrieContext(Node& root, TreeIterator iter)
        : parent(root),
          iter(iter)
    {}

    bool isTerminal() const
    {
        return (iter != parent->children.end()) && iter->second.isTerminal;
    }

    bool isMatch(const Key& key, Level level) const
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;
        if ((level != maxLevel) || (iter == parent->children.end()))
            return false;

        const auto& label = iter->first;
        auto& node = iter->second;
        assert(level < key.size());
        return node.isTerminal && (label.empty() || label == key[level]);
    }

    void advanceDepthFirst()
    {
        if (iter != parent->children.end())
        {
            if (!iter->second.isLeaf())
            {
                auto& child = iter->second;
                parent = &child;
                iter = child.children.begin();
            }
            else
            {
                ++iter;
            }
        }
        else if (!parent->isRoot())
        {
            iter = parent->iter;
            parent = parent->parent;
            ++iter;
        }
    }

    Level tryNextMatchCandidate(const Key& key, Level level)
    {
        const Level maxLevel = key.size() - 1;
        if (iter != parent->children.end())
        {
            assert(level < key.size());
            const auto& label = iter->first;
            const auto& expectedLabel = key[level];
            auto& node = iter->second;
            bool canDescend = !node.isLeaf() && (level < maxLevel) &&
                              (label.empty() || label == expectedLabel);
            if (canDescend)
            {
                ++level;
                parent = &node;
                iter = node.children.begin();
            }
            else
            {
                iter = parent->children.find(expectedLabel);
            }
        }
        else if (!parent->isRoot())
        {
            assert(level > 0);
            --level;
            iter = parent->iter;
            parent = parent->parent;
            if (iter != parent->children.end())
                iter = parent->children.find(key[level]);
        }
    }
};


//------------------------------------------------------------------------------
template <typename T, bool IsMutable>
class WildcardTrieIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer           = std::conditional<IsMutable, T*, const T*>;
    using reference         = std::conditional<IsMutable, T&, const T&>;

    WildcardTrieIterator() = default;

    // Allow construction of const iterator from mutable iterator
    template <bool M,
              typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieIterator(const WildcardTrieIterator<T, M>& rhs)
        : context_(rhs.context_)
    {}

    // Allow assignment of const iterator from mutable iterator
    template <bool M,
             typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieIterator& operator=(const WildcardTrieIterator<T, M>& rhs)
    {
        context_ = rhs.context_;
        return *this;
    }

    reference value() {return context_.iter->second.value;}

    const value_type& value() const {return context_.iter->second.value;}

    reference operator*() {return value();}

    const value_type& operator*() const {return value();}

    pointer operator->() {return &(value());}

    const value_type* operator->() const {return &(value());}

    WildcardTrieIterator& operator++() // Prefix
    {
        context_.advanceToNextMatch();
        return *this;
    }

    WildcardTrieIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    template <bool LM, bool RM>
    friend bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                           const WildcardTrieIterator<T, RM>& rhs)
    {
        return lhs.context_ == rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                           const WildcardTrieIterator<T, RM>& rhs)
    {
        return lhs.context_ != rhs.context_;
    };

private:
    using Context = WildcardTrieContext<value_type>;

    explicit WildcardTrieIterator(Context context)
        : context_(context)
    {}

    Context context_;

    template <typename> friend class WildcardTrie;
};

//------------------------------------------------------------------------------
template <typename T, bool IsMutable>
class WildcardTrieMatchIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer           = std::conditional<IsMutable, T*, const T*>;
    using reference         = std::conditional<IsMutable, T&, const T&>;

    WildcardTrieMatchIterator() = default;

    // Allow construction of const iterator from mutable iterator
    template <bool M,
             typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieMatchIterator(const WildcardTrieMatchIterator<T, M>& rhs)
        : context_(rhs.iter_)
    {}

    // Allow assignment of const iterator from mutable iterator
    template <bool M,
             typename std::enable_if<!IsMutable && M, int>::type = 0>
    WildcardTrieMatchIterator&
    operator=(const WildcardTrieMatchIterator<T, M>& rhs)
    {
        context_ = rhs.iter_;
        return *this;
    }

    reference value() {return context_->second.value;}

    const value_type& value() const {return context_->second.value;}

    reference operator*() {return value();}

    const value_type& operator*() const {return value();}

    pointer operator->() {return &(value());}

    const value_type* operator->() const {return &(value());}

    WildcardTrieMatchIterator& operator++() // Prefix
    {
        context_.advanceToNextMatch();
        return *this;
    }

    WildcardTrieMatchIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    template <bool LM, bool RM>
    friend bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                           const WildcardTrieMatchIterator<T, RM>& rhs)
    {
        return lhs.context_ == rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                           const WildcardTrieMatchIterator<T, RM>& rhs)
    {
        return lhs.context_ != rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                           const WildcardTrieIterator<T, RM>& rhs)
    {
        return lhs.context_ == rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                           const WildcardTrieMatchIterator<T, RM>& rhs)
    {
        return lhs.context_ == rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                           const WildcardTrieIterator<T, RM>& rhs)
    {
        return lhs.context_ != rhs.context_;
    };

    template <bool LM, bool RM>
    friend bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                           const WildcardTrieMatchIterator<T, RM>& rhs)
    {
        return lhs.context_ != rhs.context_;
    };

private:
    using Context = WildcardTrieContext<value_type>;

    explicit WildcardTrieMatchIterator(Context endContext)
        : context_(endContext)
    {}

    explicit WildcardTrieMatchIterator(Context beginContext, SplitUri labels_)
        : labels_(std::move(labels_)),
          context_(beginContext)
    {
        level_ = context_.advanceToFirstMatch();
    }

    SplitUri labels_;
    Context context_;
    typename SplitUri::size_type level_ = 0;

    template <typename> friend class WildcardTrie;
};

//------------------------------------------------------------------------------
template <typename T>
class WildcardTrie
{
private:
    using Tree = typename WildcardTrieNode<T>::Tree;

public:
    using key_type = SplitUri;
    using string_type = typename SplitUri::value_type;
    using mapped_type = T;
    using value_type = std::pair<const key_type, mapped_type>;
    using size_type = typename WildcardTrieNode<T>::Tree::size_type;
    using iterator = WildcardTrieIterator<T, true>;
    using const_iterator = WildcardTrieIterator<T, false>;
    using match_iterator = WildcardTrieMatchIterator<T, true>;
    using const_match_iterator = WildcardTrieMatchIterator<T, false>;

    WildcardTrie() = default;

    WildcardTrie(const WildcardTrie&) = default;

    WildcardTrie(WildcardTrie&& rhs)
        : root_(std::move(rhs.root_)),
          size_(rhs.size_)
    {
        rhs.root_.clear();
        rhs.size_ = 0;
    }

    template <typename TInputPairIterator>
    WildcardTrie(TInputPairIterator first, TInputPairIterator last)
        : root_(typename Node::RootTag{})
    {
        insert(first, last);
    }

    WildcardTrie(std::initializer_list<value_type> list)
        : WildcardTrie(list.begin(), list.end())
    {}

    WildcardTrie& operator=(const WildcardTrie& rhs)
    {
        WildcardTrie temp(rhs);
        (*this) = std::move(temp);
        return *this;
    }

    WildcardTrie& operator=(WildcardTrie&& rhs)
        noexcept(std::is_nothrow_move_assignable<Tree>::value)
    {
        root_ = std::move(rhs.root_);
        size_ = rhs.size_;
        rhs.root_.clear();
        rhs.size_ = 0;
        return *this;
    }

    WildcardTrie& operator=(std::initializer_list<value_type> list)
    {
        WildcardTrie temp(list);
        *this = std::move(temp);
        return *this;
    }

    // Element access

    mapped_type& at(const key_type& key)
    {
        auto node = locate(key);
        if (node == nullptr)
            throw std::out_of_range("wamp::WildcardTrie::at key out of range");
        return node->value;
    }

    const mapped_type& at(key_type&& key) const
    {
        auto node = locate(key);
        if (node == nullptr)
            throw std::out_of_range("wamp::WildcardTrie::at key out of range");
        return node->value;
    }

    mapped_type& operator[](const key_type& key)
    {
        return *(add(key).first);
    }

    mapped_type& operator[](key_type&& key)
    {
        return *(add(std::move(key)).first);
    }

    mapped_type& operator[](const string_type& uri)
    {
        return this->operator[](tokenizeUri(uri));
    }


    // Iterators

    iterator begin() {return iterator{firstTerminal()};}

    const_iterator begin() const {return cbegin();}

    iterator end() {return iterator{Context::end(root_)};}

    const_iterator end() const {return cend();}

    const_iterator cbegin() const {return const_iterator{firstTerminal()};}

    const_iterator cend() const {return const_iterator{Context::end(root_)};}


    // Capacity

    bool empty() const noexcept {return size_ == 0;}

    size_type size() const noexcept {return size_;}


    // Modifiers

    void clear()
    {
        root_.children.clear();
        size_ = 0;
    }

    std::pair<iterator, bool> insert(const value_type& kv)
    {
        return add(kv.first, kv.second);
    }

    std::pair<iterator, bool> insert(value_type&& kv)
    {
        return add(std::move(kv.first), std::move(kv.second));
    }

    template <typename P,
             typename std::enable_if<
                 std::is_constructible<value_type, P&&>::value /* TODO &&
                     !std::is_same<
                         value_type,
                         typename std::remove_cv<
                             typename std::remove_reference<P>::type
                             >::type>::value */,
                 int>::type = 0>
    std::pair<iterator, bool> insert(P&& kv)
    {
        return insert(value_type(std::forward<P>(kv)));
    }

    template <typename TInputPairIterator>
    void insert(TInputPairIterator first, TInputPairIterator last)
    {
        for (; first != last; ++last)
            add(first->first, first->second);
    }

    void insert(std::initializer_list<value_type> list)
    {
        insert(list.begin(), list.end());
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& arg)
    {
        return set(true, key, std::forward<M>(arg));
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& arg)
    {
        return set(true, std::move(key), std::forward<M>(arg));
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(const string_type& uri, M&& arg)
    {
        return insert_or_assign(tokenizeUri(uri), std::forward<M>(arg));
    }

    template <typename... Us>
    std::pair<iterator, bool> emplace(Us&&... args)
    {
        return insert(value_type(std::forward<Us>(args)...));
    }

    template <typename... Us>
    std::pair<iterator, bool> try_emplace(const key_type& key, Us&&... args)
    {
        return add(key, std::forward<Us>(args)...);
    }

    template <typename... Us>
    std::pair<iterator, bool> try_emplace(key_type&& key, Us&&... args)
    {
        return add(std::move(key), std::forward<Us>(args)...);
    }

    template <typename... Us>
    std::pair<iterator, bool> try_emplace(const string_type& uri, Us&&... args)
    {
        return add(tokenizeUri(uri), std::forward<Us>(args)...);
    }

    size_type erase(const key_type& key)
    {
        auto ctx = locate(key);
        bool found = !ctx.atEnd();
        if (found)
        {
            // TODO: Erase unanchored nodes up the chain
        }
        return found ? 1 : 0;
    }

    size_type erase(const string_type& uri)
    {
        return erase(tokenizeUri(uri));
    }

    void swap(WildcardTrie& other)
        noexcept(std::is_nothrow_swappable<Tree>::value)
    {
        root_.swap(other.root_);
        std::swap(size_, other.size_);
    }


    // Lookup

    size_type count(const key_type& key) const
    {
        return locate(key) == nullptr ? 0 : 1;
    }

    size_type count(const string_type& uri) const
    {
        return count(tokenizeUri(uri));
    }

    iterator find(const key_type& key) {return iterator{locate(key)};}

    const_iterator find(const key_type& key) const
    {
        return const_iterator{locate(key)};
    }

    iterator find(const string_type& uri)
    {
        return find(tokenizeUri(uri));
    }

    const_iterator find(const string_type& uri) const
    {
        return find(tokenizeUri(uri));
    }

    bool contains(const key_type& key) const {return find(key) != cend();}

    bool contains(const string_type& uri) const
    {
        return contains(tokenizeUri(uri));
    }

    std::pair<match_iterator, match_iterator> match_range(const key_type& key)
    {
        return getMatchRange<match_iterator>(key);
    }

    std::pair<const_match_iterator, const_match_iterator>
    match_range(const key_type& key) const
    {
        auto self = const_cast<WildcardTrie&>(*this);
        return self.template getMatchRange<const_match_iterator>(key);
    }

    std::pair<match_iterator, match_iterator>
    match_range(const string_type& uri)
    {
        return match_range(tokenizeUri(uri));
    }

    std::pair<match_iterator, match_iterator>
    match_range(const string_type& uri) const
    {
        return match_range(tokenizeUri(uri));
    }

private:
    using Node = WildcardTrieNode<T>;
    using Context = WildcardTrieContext<T>;

    Context firstTerminal()
    {
        auto ctx = Context::begin(root_);
        ctx.advanceToFirstTerminal();
        return ctx;
    }

    Context firstTerminal() const
    {
        return const_cast<WildcardTrie&>(*this).firstTerminal();
    }

    Context locate(const key_type& key)
    {
        auto ctx = Context::begin(root_);
        ctx.locate(key);
        return ctx;
    }

    Context locate(const key_type& key) const
    {
        return const_cast<WildcardTrie&>(*this).locate();
    }

    template <typename I>
    std::pair<I, I> getMatchRange(const key_type& key) const
    {
        if (key.empty())
            return {I{Context::end(root_)}, I{Context::end(root_)}};

        return {I{Context::begin(root_), key}, I{Context::end(root_)}};
    }

    template <typename... Us>
    std::pair<iterator, bool> add(key_type key, Us&&... args)
    {
        return set(false, std::move(key), std::forward<Us>(args)...);
    }

    template <typename... Us>
    std::pair<iterator, bool> set(bool clobber, key_type key, Us&&... args)
    {
        auto ctx = Context::begin(root_);
        bool placed = ctx.set(clobber, std::move(key),
                              std::forward<Us>(args)...);
        return {iterator{ctx}, placed};
    }

    Node root_;
    size_type size_ = 0;
};

template <typename T>
void swap(WildcardTrie<T>& a, WildcardTrie<T>& b) noexcept
{
    a.swap(b);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TRIE_HPP
