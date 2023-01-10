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
    using ChildIterator = typename Tree::iterator;

    WildcardTrieNode() = default;

    template <typename... Us>
    explicit WildcardTrieNode(Us&&... args)
        : value(std::forward<Us>(args)...),
          isTerminal(true)
    {}

    template <typename... Us>
    ChildIterator addTerminal(StringType token, Us&&... args)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(token)),
            std::forward_as_tuple(std::forward<Us>(args)...));
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    ChildIterator addLink(StringType token)
    {
        auto result = children.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::move(token)),
            std::forward_as_tuple());
        assert(result.second);
        return result.first;
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

    bool isLeaf() const {return children.empty();}

    Tree children;
    Value value;
    bool isTerminal = false;
};


//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieContext
{
public:
    static constexpr bool isConst = std::is_const<T>::value;

    using Value = typename std::remove_cv<T>::type;
    using Reference = T&;
    using Pointer = T*;

    using Node = WildcardTrieNode<Value>;
    using NodePointer =
        typename std::conditional<isConst, const Node*, Node*>::type;

    using Tree = typename Node::Tree;
    using ChildIterator =
        typename std::conditional<isConst,
                                  typename Tree::const_iterator,
                                  typename Tree::iterator>::type;

    using StringType = typename Node::StringType;

    explicit WildcardTrieContext(NodePointer parent)
        : WildcardTrieContext(parent, parent->children.begin())
    {}

    WildcardTrieContext(NodePointer parent, ChildIterator child)
        : parent_(parent),
          child_(child)
    {}

    void advance() {++child_;}

    bool find(const StringType& label)
    {
        if (isWildcard())
            return true;
        return findExact(label);
    }

    bool findExact(const StringType& label)
    {
        child_ = parent_->children.find(label);
        return !atEnd();
    }

    bool atEnd() const {return child_ == parent_->children.end();}

    NodePointer childNode() {return &child_->second;}

    const Node* childNode() const {return &child_->second;}

    bool operator==(const WildcardTrieContext& rhs) const
    {
        return std::tie(parent_, child_) == std::tie(rhs.parent_, rhs.child_);
    }

    bool operator!=(const WildcardTrieContext& rhs) const
    {
        return std::tie(parent_, child_) != std::tie(rhs.parent_, rhs.child_);
    }

private:
    bool atFront() const
    {
        return child_ == parent_->children.begin() &&
               child_ != parent_->children.end();
    }

    bool isWildcard() const {return atFront() && childToken().empty();}

    const StringType& childToken() const {return child_->first;}

    NodePointer parent_;
    ChildIterator child_;

    template <typename> friend class WildcardTrieStack;
};


//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieStack
{
public:
    using Value = typename std::remove_cv<T>::type;
    using Context = WildcardTrieContext<T>;
    using NodePointer = typename Context::NodePointer;
    using ChildIterator = typename Context::ChildIterator;
    using SizeType = typename std::vector<Context>::size_type;

    WildcardTrieStack() = default;

    explicit WildcardTrieStack(NodePointer parent)
        : stack_({Context{parent}})
    {}

    void advanceToNextTerminal()
    {
        context().advance();
        findNextTerminal();
    }

    void findNextTerminal()
    {
        // Depth-first search of next terminal node
        while (!stack_.empty())
        {
            auto& context = stack_.back();
            if (context.atEnd())
            {
                popAndAdvance();
            }
            else
            {
                auto child = context.childNode();
                if (child->isTerminal)
                {
                    break;
                }
                else
                {
                    assert(!child->isLeaf());
                    push(child);
                }
            }
        }
    }

    void findNextWildcardMatch(const SplitUri& key)
    {
        while (!empty())
        {
            auto level = depth();
            assert(level <= key.size());
            const auto& label = key[level];
            auto& ctx = context();
            if (ctx.find(label))
            {
                auto child = ctx.childNode();
                if (level < key.size())
                {
                    if (!child->isLeaf())
                        push(child);
                }
                else if (child->isTerminal)
                {
                    break;
                }
            }
            else
            {
                // Backtrack and find the next label match in the parent
                pop();
            }
        }
    }

    void pop()
    {
        assert(!stack_.empty());
        stack_.pop_back();
    }

    void eraseChildAndPop()
    {
        assert(!stack_.empty());
        auto& ctx = context();
        ctx.parent_->children.erase(ctx.child_);
        stack_.pop_back();
    }

    void push(NodePointer parent) {stack_.emplace_back(parent);}

    void push(NodePointer parent, ChildIterator child)
    {
        stack_.emplace_back(Context{parent, child});
    }

    void clear() {stack_.clear();}

    bool empty() const {return stack_.empty();}

    SizeType depth() const
    {
        // Do not include root node in depth
        assert(!empty());
        return stack_.size() - 1;
    }

    Context& context()
    {
        assert(!empty());
        return stack_.back();
    }

    bool operator==(const WildcardTrieStack& rhs) const
    {
        return stack_ == rhs.stack_;
    }

    bool operator!=(const WildcardTrieStack& rhs) const
    {
        return stack_ != rhs.stack_;
    }

private:
    void popAndAdvance()
    {
        assert(!stack_.empty());
        stack_.pop_back();
        if (!stack_.empty())
            stack_.back().advance();
    }

    std::vector<Context> stack_;
};

//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer           = T*;
    using reference         = T&;

    WildcardTrieIterator() = default;

    reference operator*() {return stack_.context().childNode()->value;}

    const value_type& operator*() const
    {
        return stack_.context().childNode()->value();
    }

    pointer operator->() {return &(stack_.context().value());}

    const value_type* operator->() const {return &(stack_.context().value());}

    WildcardTrieIterator& operator++() // Prefix
    {
        assert(!stack_.empty());
        stack_.advanceToNextTerminal();
    }

    WildcardTrieIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    friend bool operator==(const WildcardTrieIterator& lhs,
                           const WildcardTrieIterator& rhs)
    {
        return lhs.stack_ == rhs.stack_;
    };

    friend bool operator!=(const WildcardTrieIterator& lhs,
                           const WildcardTrieIterator& rhs)
    {
        return lhs.stack_ != rhs.stack_;
    };

private:
    using Stack = WildcardTrieStack<T>;
    using NodePointer = typename Stack::NodePointer;
    using ChildIterator = typename Stack::ChildIterator;

    explicit WildcardTrieIterator(NodePointer root)
        : stack_({root})
    {
        stack_.findNextTerminal();
    }

    explicit WildcardTrieIterator(Stack&& stack)
        : stack_(std::move(stack))
    {}

    void push(NodePointer parent, ChildIterator child)
    {
        stack_.push(parent, child);
    }

    Stack stack_;

    template <typename> friend class WildcardTrie;
};

//------------------------------------------------------------------------------
template <typename T>
class WildcardTrieMatchIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = typename std::remove_cv<T>::type;
    using pointer           = T*;
    using reference         = T&;

    WildcardTrieMatchIterator() = default;

    reference operator*() {return stack_.context().childNode()->value;}

    const value_type& operator*() const
    {
        return stack_.context().childNode()->value();
    }

    pointer operator->() {return &(stack_.context().value());}

    const value_type* operator->() const {return &(stack_.context().value());}

    WildcardTrieMatchIterator& operator++() // Prefix
    {
        assert(!stack_.empty());
        stack_.findNextWildcardMatch(labels_);
        return *this;
    }

    WildcardTrieMatchIterator operator++(int) // Postfix
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }

    friend bool operator==(const WildcardTrieMatchIterator& lhs,
                           const WildcardTrieMatchIterator& rhs)
    {
        return lhs.stack_ == rhs.stack_;
    };

    friend bool operator!=(const WildcardTrieMatchIterator& lhs,
                           const WildcardTrieMatchIterator& rhs)
    {
        return lhs.stack_ != rhs.stack_;
    };

    friend bool operator==(const WildcardTrieMatchIterator& lhs,
                           const WildcardTrieIterator<T>& rhs)
    {
        return lhs.stack_ == rhs.stack_;
    };

    friend bool operator!=(const WildcardTrieMatchIterator& lhs,
                           const WildcardTrieIterator<T>& rhs)
    {
        return lhs.stack_ != rhs.stack_;
    };

    friend bool operator==(const WildcardTrieIterator<T>& lhs,
                           const WildcardTrieMatchIterator& rhs)
    {
        return lhs.stack_ == rhs.stack_;
    };

    friend bool operator!=(const WildcardTrieIterator<T>& lhs,
                           const WildcardTrieMatchIterator& rhs)
    {
        return lhs.stack_ != rhs.stack_;
    };

private:
    using Stack = WildcardTrieStack<T>;
    using NodePointer = typename Stack::NodePointer;
    using ChildIterator = typename Stack::ChildIterator;

    explicit WildcardTrieMatchIterator(NodePointer root, SplitUri labels)
        : stack_({root}),
          labels_(std::move(labels))
    {
        stack_.findNextWildcardMatch(labels_);
    }

    Stack stack_;
    SplitUri labels_;

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
    using iterator = WildcardTrieIterator<T>;
    using const_iterator = WildcardTrieIterator<const T>;
    using match_iterator = WildcardTrieMatchIterator<T>;
    using const_match_iterator = WildcardTrieMatchIterator<const T>;

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
    {
        insert(list);
    }

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

    iterator begin()
    {
        if (empty())
            return end();
        return iterator{&root_};
    }

    const_iterator begin() const {return cbegin();}

    iterator end() {return iterator{};}

    const_iterator end() const {return cend();}

    const_iterator cbegin() const
    {
        if (empty())
            return cend();
        return const_iterator{&root_};
    }

    const_iterator cend() const {return const_iterator{};}


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
                 std::is_constructible<value_type, P&&>::value &&
                     !std::is_same<
                         value_type,
                         typename std::remove_cv<
                             typename std::remove_reference<P>::type
                             >::type>::value,
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
        auto stack = exactSearch(key);
        bool found = !stack.empty();
        if (found)
        {
            auto& context = stack.context();
            context.childNode()->clear();

            // Erase dead links up the chain
            while (!stack.empty())
            {
                auto& context = stack.context();
                if (context.childNode()->isLeaf())
                    stack.eraseChildAndPop();
                else
                    break;
            }
        }
        return found;
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

    iterator find(const key_type& key) {return iterator{exactSearch(key)};}

    const_iterator find(const key_type& key) const
    {
        return const_iterator{exactSearch(key)};
    }

    iterator find(const string_type& uri)
    {
        return find(tokenizeUri(uri));
    }

    const_iterator find(const string_type& uri) const
    {
        return find(tokenizeUri(uri));
    }

    bool contains(const key_type& key) const
    {
        return locate(key) != nullptr;
    }

    bool contains(const string_type& uri) const
    {
        return contains(tokenizeUri(uri));
    }

    std::pair<match_iterator, match_iterator> match_range(const key_type& key)
    {
        return {match_iterator{&root_, key}, match_iterator{}};
    }

    std::pair<match_iterator, match_iterator>
    match_range(const key_type& key) const
    {
        return {const_match_iterator{&root_, key}, const_match_iterator{}};
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

    template <typename... Us>
    std::pair<iterator, bool> add(key_type key, Us&&... args)
    {
        return set(false, std::move(key), std::forward<Us>(args)...);
    }

    template <typename... Us>
    std::pair<iterator, bool> set(bool clobber, key_type key, Us&&... args)
    {
        iterator where;
        bool placed = false;
        Node* node = &root_;
        const auto tokenCount = key.size();
        for (SplitUri::size_type i = 0; i < tokenCount; ++i)
        {
            auto& token = key[i];
            auto iter = node->children.find(token);
            bool needsNewBranchOrLeaf = iter == node->children.end();
            bool lastToken = i+1 == tokenCount;
            if (needsNewBranchOrLeaf)
            {
                placed = lastToken;
                iter = lastToken ? node->addTerminal(std::move(token),
                                                     std::forward<Us>(args)...)
                                 : node->addLink(std::move(token));
            }
            else if (clobber && lastToken)
            {
                mapped_type x(std::forward<Us>(args)...);
                iter->second.value = std::move(x);
                iter->second.isTerminal = true;
            }
            node = &(iter->second);
            where.push(node, iter);
        }

        if (placed)
        {
            assert(size_ < std::numeric_limits<size_type>::max());
            ++size_;
        }

        return {where, placed};
    }

    WildcardTrieStack<T> exactSearch(const key_type& key)
    {
        return doExactSearch<WildcardTrieStack<T>>(*this, key);
    }

    WildcardTrieStack<const T> exactSearch(const key_type& key) const
    {
        return doExactSearch<WildcardTrieStack<const T>>(*this, key);
    }

    template <typename TStack, typename TSelf>
    static TStack doExactSearch(TSelf& self, const key_type& key)
    {
        TStack stack;
        if (!key.empty())
            stack.push(&self.root_);

        for (const auto& label: key)
        {
            auto& context = stack.context();
            if (context.findExact(label))
            {
                stack.push(context.childNode());
            }
            else
            {
                stack.clear();
                break;
            }
        }
        return stack;
    }

    Node* locate(const key_type& key)
    {
        return doLocate<Node>(*this, key);
    }

    const Node* locate(const key_type& key) const
    {
        return doLocate<const Node>(*this, key);
    }

    template <typename TNode, typename TSelf>
    static TNode* doLocate(TSelf& self, const key_type& key)
    {
        const auto labelCount = key.size();
        TNode* node = &self.root_;
        for (typename key_type::size_type i = 0; i<labelCount; ++i)
        {
            const auto& label = key[i];
            auto found = node->children.find(label);
            if (found == node->children.cend())
                return nullptr;
            node = &(found->second);
            if (i+1 < labelCount)
            {
                if (node->isLeaf())
                    return nullptr;
            }
            else if (!node->isTerminal)
                return nullptr;
        }
        return node;
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
