/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TRIE_HPP
#define CPPWAMP_INTERNAL_TRIE_HPP

#include <algorithm>
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
    using Key = SplitUri;
    using StringType = typename Key::value_type;
    using Tree = std::map<StringType, WildcardTrieNode>;
    using TreeIterator = typename Tree::iterator;
    using Level = typename Key::size_type;

    WildcardTrieNode() : position(children.end()) {}

    template <typename... Us>
    WildcardTrieNode(bool isTerminal, Us&&... args)
        : value(std::forward<Us>(args)...),
          isTerminal(isTerminal)
    {}

    template <typename... Us>
    TreeIterator addTerminal(StringType label, Us&&... args)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(true, std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    void buildChain(Key&& key, Level level, Us&&... args)
    {
        const auto tokenCount = key.size();
        WildcardTrieNode* node = this;
        ++level;

        // Add intermediary link nodes
        for (; level < tokenCount - 1; ++level)
        {
            auto iter = node->buildLink(std::move(key[level]));
            node = &(iter->second);
        }

        // Add terminal node
        assert(level < key.size());
        node->addTerminal(std::move(key[level]), std::forward<Us>(args)...);
    }

    TreeIterator addChain(StringType&& label, WildcardTrieNode&& chain)
    {
        auto result = children.emplace(std::move(label), std::move(chain));
        assert(result.second);

        // Traverse down the emplaced chain and set the parent/position
        // fields to their proper values. Better to do this after emplacing
        // the chain to avoid invalid pointers/iterators.
        auto iter = result.first;
        auto node = this;
        while (!node->isLeaf())
        {
            WildcardTrieNode& child = iter->second;
            child.position = iter;
            child.parent = node;
            node = &child;
            iter = child.children.begin();
        }
        return node->position;
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

    Key generateKey() const
    {
        Key key;
        const WildcardTrieNode* node = this;
        while (!node->isRoot())
        {
            key.emplace_back(node->position->first);
            node = node->parent;
        }
        std::reverse(key.begin(), key.end());
        return key;
    }

    Tree children;
    Value value = {};
    TreeIterator position = {};
    WildcardTrieNode* parent = nullptr;

    bool isTerminal = false;

private:
    template <typename... Us>
    TreeIterator buildLink(StringType label)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(label)),
            std::forward_as_tuple(false));
        assert(result.second);
        return result.first;
    }
};


//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieContext
{
public:
    using Node = WildcardTrieNode<T>;
    using TreeIterator = typename Node::TreeIterator;
    using Key = SplitUri;
    using Value = typename Node::Value;
    using StringType = typename Key::value_type;
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

    void locate(const Key& key)
    {
        Node* root = parent;
        const TreeIterator end = parent->children.end();
        bool found = !key.empty();

        if (found)
        {
            for (typename Key::size_type i = 0; i<key.size(); ++i)
            {
                const auto& label = key[i];
                iter = parent->children.find(label);
                if (iter == parent->children.end())
                {
                    found = false;
                    break;
                }

                if (i < key.size() - 1)
                    parent = &(iter->second);
            }
            found = found && iter->second.isTerminal;
        }

        if (!found)
        {
            parent = root;
            iter = end;
        }
    }

    Key generateKey() const
    {
        if (iter == parent->children.end())
            return {};
        return iter->second.generateKey();
    }

    template <typename... Us>
    bool put(bool clobber, Key key, Us&&... args)
    {
        if (key.empty())
            return false;

        // To avoid dangling link nodes in the event of an exception,
        // build a sub-chain first with the new node, than attach it to the
        // existing tree using move semantics.

        bool placed = false;
        const auto tokenCount = key.size();

        // Find existing node from which to possibly attach a sub-chain with
        // the new node.
        Level level = 0;
        for (; level < tokenCount; ++level)
        {
            const auto& label = key[level];
            iter = parent->children.find(label);
            if (iter == parent->children.end())
                break;
            parent = &(iter->second);
        }

        // Check if node already exists at the destination level
        // in the existing tree.
        if (level == tokenCount)
        {
            auto& node = iter->second;
            parent = node.parent;
            placed = !node.isTerminal;
            if (placed || clobber)
                node.setValue(std::forward<Us>(args)...);
            return placed;
        }

        // Check if only a single terminal node needs to be added
        assert(level < tokenCount);
        if (tokenCount - level == 1)
        {
            iter = parent->addTerminal(key[level], std::forward<Us>(args)...);
            iter->second.position = iter;
            iter->second.parent = parent;
            return true;
        }

        // Build and attach the sub-chain containing the new node.
        Node chain;
        auto label = std::move(key[level]);
        chain.buildChain(std::move(key), level, std::forward<Us>(args)...);
        iter = parent->addChain(std::move(label), std::move(chain));
        parent = iter->second.parent;
        placed = true;

        return placed;
    }

    void eraseFromHere()
    {
        // Erase the terminal node, then all obsolete links up the chain until
        // we hit another terminal node.
        iter->second.isTerminal = false;
        while (iter->second.isTerminal && !parent->isRoot())
        {
            parent->children.erase(iter);
            iter = parent->position;
            parent = parent->parent;
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
            level = findNextMatchCandidate(key, level);
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
        : parent(&root),
          iter(iter)
    {}

    bool isTerminal() const
    {
        return (iter != parent->children.end()) && iter->second.isTerminal;
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
            iter = parent->position;
            parent = parent->parent;
            ++iter;
        }
    }

    bool isMatch(const Key& key, Level level) const
    {
        assert(!key.empty());
        const Level maxLevel = key.size() - 1;
        if ((level != maxLevel) || (iter == parent->children.end()))
            return false;

        // All nodes above the current level are matches. Only the bottom
        // level needs to be checked.
        assert(level < key.size());
        return iter->second.isTerminal && labelMatches(key[level]);
    }

    bool labelMatches(const StringType& expectedLabel) const
    {
        return iter->first.empty() || iter->first == expectedLabel;
    }

    Level findNextMatchCandidate(const Key& key, Level level)
    {
        const Level maxLevel = key.size() - 1;
        if (iter != parent->children.end())
        {
            assert(level < key.size());
            const auto& expectedLabel = key[level];
            bool canDescend = !iter->second.isLeaf() && (level < maxLevel) &&
                              labelMatches(expectedLabel);
            if (canDescend)
                level = descend(level);
            else
                findLabelInLevel(expectedLabel);
        }
        else if (!parent->isRoot())
        {
            level = ascend(level);
            if (iter != parent->children.end())
                findLabelInLevel(key[level]);
        }
        return level;
    }

    Level ascend(Level level)
    {
        assert(level > 0);
        iter = parent->position;
        parent = parent->parent;
        return level - 1;
    }

    Level descend(Level level)
    {
        auto& node = iter->second;
        parent = &node;
        iter = node.children.begin();
        return level + 1;
    }

    void findLabelInLevel(const StringType& label)
    {
        struct Compare
        {
            bool operator()(const typename TreeIterator::value_type& kv,
                            const StringType& s) const
            {
                return kv.first < s;
            }

            bool operator()(const StringType& s,
                            const typename TreeIterator::value_type& kv) const
            {
                return s < kv.first;
            }
        };

        if (iter == parent->children.begin())
        {
            iter = std::lower_bound(++iter, parent->children.end(), label,
                                    Compare{});
        }
        else
        {
            iter = parent->children.end();
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
    using key_type          = SplitUri;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer   = typename std::conditional<IsMutable, T*, const T*>::type;
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;

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

    SplitUri key() const {return context_.generateKey();}

    reference value() {return context_.iter->second.value;}

    const value_type& value() const {return context_.iter->second.value;}

    reference operator*() {return value();}

    const value_type& operator*() const {return value();}

    pointer operator->() {return &(value());}

    const value_type* operator->() const {return &(value());}

    WildcardTrieIterator& operator++() // Prefix
    {
        context_.advanceToNextTerminal();
        return *this;
    }

    WildcardTrieIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    using Context = WildcardTrieContext<value_type>;

    explicit WildcardTrieIterator(Context context)
        : context_(context)
    {}

    Context context_;

    template <typename> friend class WildcardTrie;

    template <typename U, bool LM, bool RM>
    friend bool operator==(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator!=(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieIterator<U, RM>& rhs);
};

template <typename T, bool LM, bool RM>
inline bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                       const WildcardTrieIterator<T, RM>& rhs)
{
    return lhs.context_ == rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                       const WildcardTrieIterator<T, RM>& rhs)
{
    return lhs.context_ != rhs.context_;
};

//------------------------------------------------------------------------------
template <typename T, bool IsMutable>
class WildcardTrieMatchIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using key_type          = SplitUri;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer   = typename std::conditional<IsMutable, T*, const T*>::type;
    using reference = typename std::conditional<IsMutable, T&, const T&>::type;

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

    SplitUri key() const {return context_.generateKey();}

    reference value() {return context_.iter->second.value;}

    const value_type& value() const {return context_.iter->second.value;}

    reference operator*() {return value();}

    const value_type& operator*() const {return value();}

    pointer operator->() {return &(value());}

    const value_type* operator->() const {return &(value());}

    WildcardTrieMatchIterator& operator++() // Prefix
    {
        context_.matchNext(key_, level_);
        return *this;
    }

    WildcardTrieMatchIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    using Context = WildcardTrieContext<value_type>;

    explicit WildcardTrieMatchIterator(Context endContext)
        : context_(endContext)
    {}

    explicit WildcardTrieMatchIterator(Context beginContext, SplitUri labels_)
        : key_(std::move(labels_)),
          context_(beginContext)
    {
        level_ = context_.matchFirst(key_);
    }

    SplitUri key_;
    Context context_;
    typename SplitUri::size_type level_ = 0;

    template <typename> friend class WildcardTrie;

    template <typename U, bool LM, bool RM>
    friend bool operator==(const WildcardTrieMatchIterator<U, LM>& lhs,
                           const WildcardTrieMatchIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator!=(const WildcardTrieMatchIterator<U, LM>& lhs,
                           const WildcardTrieMatchIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator==(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieMatchIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator!=(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieMatchIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator==(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieIterator<U, RM>& rhs);

    template <typename U, bool LM, bool RM>
    friend bool operator!=(const WildcardTrieIterator<U, LM>& lhs,
                           const WildcardTrieIterator<U, RM>& rhs);
};

template <typename T, bool LM, bool RM>
inline bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                       const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return lhs.context_ == rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                       const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return lhs.context_ != rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator==(const WildcardTrieMatchIterator<T, LM>& lhs,
                       const WildcardTrieIterator<T, RM>& rhs)
{
    return lhs.context_ == rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator==(const WildcardTrieIterator<T, LM>& lhs,
                       const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return lhs.context_ == rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator!=(const WildcardTrieMatchIterator<T, LM>& lhs,
                       const WildcardTrieIterator<T, RM>& rhs)
{
    return lhs.context_ != rhs.context_;
};

template <typename T, bool LM, bool RM>
inline bool operator!=(const WildcardTrieIterator<T, LM>& lhs,
                       const WildcardTrieMatchIterator<T, RM>& rhs)
{
    return lhs.context_ != rhs.context_;
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
        auto ctx = locate(key);
        if (ctx.atEnd())
            throw std::out_of_range("wamp::WildcardTrie::at key out of range");
        return ctx.iter->second.value;
    }

    const mapped_type& at(const key_type& key) const
    {
        auto ctx = locate(key);
        if (ctx.atEnd())
            throw std::out_of_range("wamp::WildcardTrie::at key out of range");
        return ctx.iter->second.value;
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

    iterator begin() noexcept {return iterator{firstTerminal()};}

    const_iterator begin() const noexcept {return cbegin();}

    iterator end() noexcept {return iterator{Context::end(root_)};}

    const_iterator end() const noexcept {return cend();}

    const_iterator cbegin() const noexcept
    {
        return const_iterator{firstTerminal()};
    }

    const_iterator cend() const noexcept
    {
        auto& root = const_cast<Node&>(root_);
        return const_iterator{Context::end(root)};
    }


    // Capacity

    bool empty() const noexcept {return size_ == 0;}

    size_type size() const noexcept {return size_;}


    // Modifiers

    void clear() noexcept
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
                 std::is_constructible<value_type, P&&>::value,
                 int>::type = 0>
    std::pair<iterator, bool> insert(P&& kv)
    {
        return insert(value_type(std::forward<P>(kv)));
    }

    template <typename TInputPairIterator>
    void insert(TInputPairIterator first, TInputPairIterator last)
    {
        for (; first != last; ++first)
            add(first->first, first->second);
    }

    void insert(std::initializer_list<value_type> list)
    {
        insert(list.begin(), list.end());
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& arg)
    {
        return put(true, key, std::forward<M>(arg));
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& arg)
    {
        return put(true, std::move(key), std::forward<M>(arg));
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
            ctx.eraseFromHere();
            --size_;
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
        return locate(key).atEnd() ? 0 : 1;
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
        return const_cast<WildcardTrie&>(*this).locate(key);
    }

    template <typename I>
    std::pair<I, I> getMatchRange(const key_type& key) const
    {
        auto& root = const_cast<Node&>(root_);
        if (key.empty())
            return {I{Context::end(root)}, I{Context::end(root)}};

        return {I{Context::begin(root), key}, I{Context::end(root)}};
    }

    template <typename... Us>
    std::pair<iterator, bool> add(key_type key, Us&&... args)
    {
        return put(false, std::move(key), std::forward<Us>(args)...);
    }

    template <typename... Us>
    std::pair<iterator, bool> put(bool clobber, key_type key, Us&&... args)
    {
        auto ctx = Context::begin(root_);
        bool placed = ctx.put(clobber, std::move(key),
                              std::forward<Us>(args)...);
        if (placed)
            ++size_;
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
