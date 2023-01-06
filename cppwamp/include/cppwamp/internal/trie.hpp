/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TRIE_HPP
#define CPPWAMP_INTERNAL_TRIE_HPP

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
    ChildIterator addLeaf(StringType token, Us&&... args)
    {
        auto result = children.emplace(std::move(token),
                                       std::forward<Us>(args)...);
        assert(result.second);
        return result.first;
    }

    template <typename... Us>
    ChildIterator addBranch(StringType token)
    {
        auto result = children.emplace(std::move(token));
        assert(result.second);
        return result.first;
    }

    Tree children;
    Value value;
    bool hasValue = false;
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
        child_ = parent_->children.find(label);
        return !atEnd();
    }

    bool atFront() const
    {
        return child_ == parent_->children.begin() &&
               child_ != parent_->children.end();
    }

    bool atEnd() const {return child_ == parent_->children.end();}

    bool isLeaf() const {return child_->second.children.empty();}

    bool hasValue() const {return child_->second.hasValue;}

    bool isWildcard() const {return atFront() && childToken().empty();}

    const StringType& childToken() const {return child_->first;}

    NodePointer childNode() {return &child_->second;}

    const Node* childNode() const {return &child_->second;}

    Reference value() {return child_->second.node.value;}

    const Value& value() const {return child_->second.node.value;}

    ChildIterator childIterator() const
    {
        return child_;
    }

private:
    NodePointer parent_;
    ChildIterator child_;
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

    WildcardTrieStack(NodePointer parent, ChildIterator child)
        : stack_({Context{parent, child}})
    {}

    void advanceToNextValue()
    {
        context().advance();
        findNextValue();
    }

    void findNextValue()
    {
        // Depth-first search of next node with value
        while (!stack_.empty())
        {
            auto& context = stack_.back();
            if (context.atEnd())
                popAndAdvance();
            else if (context.hasValue())
                break;
            else if (context.isLeaf())
                context.advance();
            else
                push(context.childNode());
        }
    }

    void popAndAdvance()
    {
        stack_.pop_back();
        if (!stack_.empty())
            stack_.back().advance();
    }

    void push(NodePointer parent) {stack_.emplace_back(parent);}

    void push(NodePointer parent, ChildIterator child)
    {
        stack_.emplace_back({parent, child});
    }

    bool empty() {return stack_.empty();}

    SizeType depth() {return stack_.size();}

    Context& context()
    {
        assert(!empty());
        return stack_.back();
    }

    const Context& context() const
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

    reference operator*() {return stack_.context().value();}

    const value_type& operator*() const {return stack_.context().value();}

    pointer operator->() {return &(stack_.context().value());}

    const value_type* operator->() const {return &(stack_.context().value());}

    WildcardTrieIterator& operator++() // Prefix
    {
        assert(!stack_.empty());
        stack_.advanceToNextValue();
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
        : WildcardTrieIterator(root, root->children.begin())
    {
        stack_.findNextValue();
    }

    WildcardTrieIterator(NodePointer node, ChildIterator iter)
        : stack_({node, iter})
    {}

    explicit WildcardTrieIterator(Stack&& stack)
        : stack_(std::move(stack))
    {}

    void push(NodePointer parent, ChildIterator child)
    {
        stack_.push(parent, child);
    }

    Stack stack_;

    // TODO: template <typename> friend class WildcardTrie;
    friend class WildcardTrie;
};

//------------------------------------------------------------------------------
//template <typename T> // TODO
class WildcardTrie
{
public: // TODO
    using T = int;

public:
    using key_type = SplitUri;
    using string_type = typename SplitUri::value_type;
    using mapped_type = T;
    using size_type = std::size_t;
    using iterator = WildcardTrieIterator<T>;
    using const_iterator = WildcardTrieIterator<const T>;

    template <typename... Us>
    std::pair<iterator, bool> emplace(key_type key, Us&&... args)
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
            if (needsNewBranchOrLeaf)
            {
                placed = true;
                bool lastToken = i+1 == tokenCount;
                iter = lastToken ? node->addLeaf(std::move(token),
                                                 std::forward<Us>(args)...)
                                 : node->addBranch(std::move(token));
            }
            node = &(iter->second);
            where.push(node, iter);
        }
        if (placed)
            ++size_;
        return {where, placed};
    }

    template <typename... Us>
    std::pair<iterator, bool> emplace(const string_type& uri, Us&&... args)
    {
        return emplace(tokenizeUri(uri), std::forward<Us>(args)...);
    }

    void clear()
    {
        root_.children.clear();
        size_ = 0;
    }

    iterator find(const key_type& key)
    {
        return doFind<WildcardTrie, iterator>(*this, key);
    }

    const_iterator find(const key_type& key) const
    {
        return doFind<const WildcardTrie, const_iterator>(*this, key);
    }

    size_type size() const {return size_;}

    bool empty() const {return size_ == 0;}

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

private:
    using Node = WildcardTrieNode<T>;

    template <typename TSelf, typename TIterator>
    static TIterator doFind(TSelf& self, const key_type& key)
    {
        using Stack = typename TIterator::Stack;

        Stack stack{&self.root_};

        while (true)
        {
            auto& context = stack.context();
            // TODO
        }

        return TIterator{std::move(stack)};
    }

    Node root_;
    size_type size_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TRIE_HPP
