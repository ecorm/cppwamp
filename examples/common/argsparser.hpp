/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_ARGSPARSER_HPP
#define CPPWAMP_EXAMPLES_ARGSPARSER_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

//------------------------------------------------------------------------------
class ArgsParserEntry
{
public:
    ArgsParserEntry(std::string name, std::string fallback)
        : name_(std::move(name)),
          value_(std::move(fallback))
    {}

    const std::string& name() const {return name_;}

    const std::string& value() const {return value_;}

    void setValue(std::string value) {value_ = std::move(value);}

private:
    std::string name_;
    std::string value_;
};

//------------------------------------------------------------------------------
class ArgsParser
{
public:
    using EntryList = std::vector<ArgsParserEntry>;

    ArgsParser(EntryList entries) : entries_(std::move(entries)) {}

    template <typename... Ts>
    bool parse(int argc, char* argv[], Ts&... args)
    {
        if (argc >= 2 && std::string{argv[1]} == "help")
        {
            showHelp(argv[0]);
            return false;
        }

        for (unsigned i=0; i<entries_.size(); ++i)
        {
            int argIndex = i + 1;
            if (argIndex >= argc)
                break;

            entries_[i].setValue(argv[argIndex]);
        }

        parseNext(0, args...);
        return true;
    }

private:
    template <typename T, typename... Tail>
    void parseNext(unsigned index, T& nextArg, Tail&... tailArgs)
    {
        parseArg(index, nextArg);
        parseNext(index + 1, tailArgs...);
    }

    template <typename T>
    void parseNext(unsigned index, T& lastArg)
    {
        parseArg(index, lastArg);
    }

    template <typename T>
    void parseArg(unsigned index, T& arg) const
    {
        const auto& entry = entries_.at(index);
        std::istringstream iss{entry.value()};
        iss >> arg;
        if (!iss)
        {
            throw std::runtime_error(
                std::string{"Failure parsing argument: "} + entry.name());
        }
    }

    void showHelp(const char* cmd) const
    {
        std::cout << "Usage: " << cmd;
        for (const auto& entry: entries_)
            std::cout << " [" << entry.name();
        for (unsigned i=0; i<entries_.size(); ++i)
            std::cout << ']';

        std::cout << "\nDefaults: ";
        for (unsigned i=0; i<entries_.size(); ++i)
        {
            if (i != 0)
                std::cout << ", ";
            const auto& entry = entries_[i];
            std::cout << entry.name() << "=" << entry.value();
        }
        std::cout << "\n";
    }

    EntryList entries_;
};

#endif // CPPWAMP_EXAMPLES_ARGSPARSER_HPP
