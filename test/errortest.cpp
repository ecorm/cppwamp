/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <cppwamp/errorcodes.hpp>
#include <catch2/catch.hpp>
#include <boost/asio/error.hpp>
#include <jsoncons/json_error.hpp>
#include <jsoncons_ext/cbor/cbor_error.hpp>
#include <jsoncons_ext/msgpack/msgpack_error.hpp>

using namespace wamp;

//------------------------------------------------------------------------------
SCENARIO( "MiscErrc error codes", "[Error]" )
{
    SECTION("Properties")
    {
        using T = std::underlying_type<MiscErrc>::type;
        for (T i=0; i<T(MiscErrc::count); ++i)
        {
            auto errc = MiscErrc(i);
            auto ec = make_error_code(errc);
            INFO("For error code " << ec);
            CHECK(ec.category() == genericCategory());
            CHECK_THAT(ec.category().name(),
                       Catch::Matchers::Equals("wamp::MiscCategory"));
            CHECK(!ec.message().empty());
            CHECK(ec == errc);
            CHECK(ec == make_error_condition(errc));
        }
    }

    SECTION("Equivalencies")
    {
        CHECK(make_error_code(MiscErrc::abandoned) != MiscErrc::success);
        CHECK(std::error_code{0, std::generic_category()} == MiscErrc::success);
        CHECK(make_error_code(std::errc::result_out_of_range) !=
              MiscErrc::success);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "WampErrc error codes", "[Error]" )
{
    SECTION("Properties")
    {
        using T = std::underlying_type<WampErrc>::type;
        for (T i=0; i<T(WampErrc::count); ++i)
        {
            auto errc = WampErrc(i);
            auto ec = make_error_code(errc);
            INFO("For error code " << ec);
            CHECK(ec.category() == wampCategory());
            CHECK_THAT(ec.category().name(),
                       Catch::Matchers::Equals("wamp::WampCategory"));
            CHECK(!ec.message().empty());
            CHECK(ec == errc);
            CHECK(ec == make_error_condition(errc));

            auto uri = errorCodeToUri(errc);
            CHECK(!uri.empty());
            auto roundTrippedErrc = errorUriToCode(uri);
            CHECK(roundTrippedErrc == errc);
        }
    }

    SECTION("Equivalences")
    {
        CHECK(make_error_code(WampErrc::unknown) != WampErrc::success);
        CHECK(make_error_code(WampErrc::success) == MiscErrc::success);
        CHECK(make_error_code(MiscErrc::success) == WampErrc::success);
        CHECK(std::error_code{0, std::generic_category()} == WampErrc::success);
        CHECK(make_error_code(MiscErrc::abandoned) != WampErrc::success);

        CHECK(make_error_code(WampErrc::goodbyeAndOut) == WampErrc::closedNormally);
        CHECK(make_error_code(WampErrc::closedNormally) == WampErrc::goodbyeAndOut);
        CHECK(make_error_code(WampErrc::timeout) == WampErrc::cancelled);
        CHECK(make_error_code(WampErrc::discloseMeDisallowed) ==
              WampErrc::optionNotAllowed);
    }

    SECTION("Unknown and alternate error URIs")
    {
        CHECK(errorUriToCode("") == WampErrc::unknown);
        CHECK(errorUriToCode("foo") == WampErrc::unknown);
        CHECK(errorUriToCode("wamp.error.close_realm") == WampErrc::closeRealm);
        CHECK(errorUriToCode("wamp.error.goodbye_and_out") ==
              WampErrc::goodbyeAndOut);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "DecodingErrc error codes", "[Error]" )
{
    SECTION("Properties")
    {
        using T = std::underlying_type<DecodingErrc>::type;
        for (T i=0; i<T(DecodingErrc::count); ++i)
        {
            auto errc = DecodingErrc(i);
            auto ec = make_error_code(errc);
            INFO("For error code " << ec);
            CHECK(ec.category() == decodingCategory());
            CHECK_THAT(ec.category().name(),
                       Catch::Matchers::Equals("wamp::DecodingCategory"));
            CHECK(!ec.message().empty());
            CHECK(ec == errc);
            CHECK(ec == make_error_condition(errc));
            if (i != 0)
                CHECK(ec == DecodingErrc::failed);
        }
    }

    SECTION("Equivalencies")
    {
        CHECK(make_error_code(DecodingErrc::failed) != DecodingErrc::success);
        CHECK(make_error_code(MiscErrc::success) == DecodingErrc::success);
        CHECK(make_error_code(MiscErrc::abandoned) != DecodingErrc::success);
        CHECK(make_error_code(jsoncons::json_errc::source_error) ==
              DecodingErrc::failed);
        CHECK(make_error_code(jsoncons::cbor::cbor_errc::source_error) ==
              DecodingErrc::failed);
        CHECK(make_error_code(jsoncons::msgpack::msgpack_errc::source_error) ==
              DecodingErrc::failed);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "TransportErrc error codes", "[Error]" )
{
    SECTION("Properties")
    {
        using T = std::underlying_type<TransportErrc>::type;
        for (T i=0; i<T(TransportErrc::count); ++i)
        {
            auto errc = TransportErrc(i);
            auto ec = make_error_code(errc);
            INFO("For error code " << ec);
            CHECK(ec.category() == transportCategory());
            CHECK_THAT(ec.category().name(),
                       Catch::Matchers::Equals("wamp::TransportCategory"));
            CHECK(!ec.message().empty());
            CHECK(ec == errc);
            CHECK(ec == make_error_condition(errc));
            if (errc >= TransportErrc::failed)
                CHECK(ec == TransportErrc::failed);
        }
    }

    SECTION("Equivalencies")
    {
        namespace asioerror = boost::asio::error;
        auto success = TransportErrc::success;
        auto failed = TransportErrc::failed;
        CHECK(make_error_code(TransportErrc::failed) != TransportErrc::success);
        CHECK(make_error_code(MiscErrc::success) == success);
        CHECK(make_error_code(MiscErrc::abandoned) != success);
        CHECK(std::error_code{0, std::generic_category()} == success);
        CHECK(std::error_code{1, std::generic_category()} == failed);
        CHECK(std::error_code{0, std::system_category()} == success);
        CHECK(std::error_code{1, std::system_category()} == failed);
        CHECK(std::error_code{0, boost::system::generic_category()} == success);
        CHECK(std::error_code{1, boost::system::generic_category()} == failed);
        CHECK(std::error_code{0, boost::system::system_category()} == success);
        CHECK(std::error_code{1, boost::system::system_category()} == failed);
        CHECK(make_error_code(asioerror::addrinfo_errors::service_not_found) ==
              failed);
        CHECK(make_error_code(asioerror::misc_errors::eof) == failed);
        CHECK(make_error_code(asioerror::netdb_errors::no_data) == failed);

        auto disconnected = TransportErrc::disconnected;
        CHECK(make_error_code(std::errc::connection_reset) == disconnected);
        CHECK(make_error_code(asioerror::connection_reset) == disconnected);
        CHECK(make_error_code(asioerror::eof) == disconnected);
    }
}
