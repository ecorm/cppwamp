/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <catch2/catch.hpp>
#include <cppwamp/internal/base64.hpp>

using namespace wamp;
using namespace wamp::internal;
namespace Matchers = Catch::Matchers;

//------------------------------------------------------------------------------
SCENARIO( "Valid Base64 Encoding/Decoding", "[Base64]" )
{
    struct TestVector
    {
        std::vector<uint8_t> binary;
        std::string base64;
        std::string base64Alt;
    };

    const std::vector<TestVector> testVectors =
    {
         {{},                                   "",          ""},
         {{0x66},                               "Zg==",      "Zg"},
         {{0x66, 0x6f},                         "Zm8=",      "Zm8"},
         {{0x66, 0x6f, 0x6f},                   "Zm9v",      "Zm9v"},
         {{0x66, 0x6f, 0x6f, 0x62},             "Zm9vYg==",  "Zm9vYg="},
         {{0x66, 0x6f, 0x6f, 0x62, 0x61},       "Zm9vYmE=",  "Zm9vYmE"},
         {{0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72}, "Zm9vYmFy",  "Zm9vYmFy"},
         {{0x00},                               "AA==",      "AA"},
         {{0x00, 0x00},                         "AAA=",      "AAA"},
         {{0x00, 0x00, 0x00},                   "AAAA",      "AAAA"},
         {{0x00, 0x00, 0x00, 0x00},             "AAAAAA==",  "AAAAAA="},
         {{0xff},                               "/w==",      "/w"},
         {{0xff, 0xff},                         "//8=",      "//8"},
         {{0xff, 0xff, 0xff},                   "////",      "////"},
         {{0xff, 0xff, 0xff, 0xff},             "/////w==",  "/////w="},
         {{0x00, 0x7f, 0x80, 0xff},             "AH+A/w==",  "AH+A/w"},
         {{0xff, 0x80, 0x7f, 0x00},             "/4B/AA==",  "/4B/AA"},
         {{0x65, 0xac, 0xf4, 0xf7, 0xef},       "Zaz09+8=",  "Zaz09+8"}
    };

    for (const auto& vec: testVectors)
    {
        INFO("With Base64 '" << vec.base64 << "'");
        std::string encoded;
        Base64::encode(vec.binary.data(), vec.binary.size(), encoded);
        CHECK( encoded == vec.base64 );

        std::vector<uint8_t> decoded;
        std::error_code ec;
        ec = Base64::decode(vec.base64.data(), vec.base64.size(), decoded);
        CHECK_FALSE( ec );
        CHECK_THAT( decoded, Matchers::Equals(vec.binary) );

        decoded.clear();
        ec = Base64::decode(vec.base64Alt.data(), vec.base64Alt.size(),
                            decoded);
        CHECK_FALSE( ec );
        CHECK_THAT( decoded, Matchers::Equals(vec.binary) );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Valid Base64Url Encoding/Decoding", "[Base64]" )
{
    struct TestVector
    {
        std::vector<uint8_t> binary;
        std::string base64;
        std::string base64Alt;
    };

    const std::vector<TestVector> testVectors =
    {
         {{},                                   "",          ""},
         {{0x66},                               "Zg",        "Zg=="},
         {{0x66, 0x6f},                         "Zm8",       "Zm8="},
         {{0x66, 0x6f, 0x6f},                   "Zm9v",      "Zm9v"},
         {{0x66, 0x6f, 0x6f, 0x62},             "Zm9vYg",    "Zm9vYg="},
         {{0x66, 0x6f, 0x6f, 0x62, 0x61},       "Zm9vYmE",   "Zm9vYmE="},
         {{0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72}, "Zm9vYmFy",  "Zm9vYmFy"},
         {{0x00},                               "AA",        "AA=="},
         {{0x00, 0x00},                         "AAA",       "AAA="},
         {{0x00, 0x00, 0x00},                   "AAAA",      "AAAA"},
         {{0x00, 0x00, 0x00, 0x00},             "AAAAAA",    "AAAAAA=="},
         {{0xff},                               "_w",        "_w="},
         {{0xff, 0xff},                         "__8",       "__8="},
         {{0xff, 0xff, 0xff},                   "____",      "____"},
         {{0xff, 0xff, 0xff, 0xff},             "_____w",    "_____w="},
         {{0x00, 0x7f, 0x80, 0xff},             "AH-A_w",    "AH-A_w=="},
         {{0xff, 0x80, 0x7f, 0x00},             "_4B_AA",    "_4B_AA="},
         {{0x65, 0xac, 0xf4, 0xf7, 0xef},       "Zaz09-8",   "Zaz09-8="}
    };

    for (const auto& vec: testVectors)
    {
        INFO("With Base64Url '" << vec.base64 << "'");
        std::string encoded;
        Base64Url::encode(vec.binary.data(), vec.binary.size(), encoded);
        CHECK( encoded == vec.base64 );

        std::vector<uint8_t> decoded;
        std::error_code ec;
        ec = Base64Url::decode(vec.base64.data(), vec.base64.size(), decoded);
        CHECK_FALSE( ec );
        CHECK_THAT( decoded, Matchers::Equals(vec.binary) );

        decoded.clear();
        ec = Base64Url::decode(vec.base64Alt.data(), vec.base64Alt.size(),
                               decoded);
        CHECK_FALSE( ec );
        CHECK_THAT( decoded, Matchers::Equals(vec.binary) );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Malformed Base64 Decoding", "[Base64]" )
{
    WHEN( "Pad optional" )
    {
        struct TestVector
        {
            std::string in;
            DecodingErrc errc;
        };

        using DE = DecodingErrc;

        const std::vector<TestVector> testVectors = {
            {"!m8=",     DE::badBase64Char},    // Invalid character
            {"Z@8=",     DE::badBase64Char},    // Invalid character
            {"Zm#=",     DE::badBase64Char},    // Invalid character
            {"Zm8%",     DE::badBase64Char},    // Invalid character
            {"Zm9vYmF!", DE::badBase64Char},    // Invalid character
            {"Zm8_",     DE::badBase64Char},    // Base64 URL character
            {"Zm8-",     DE::badBase64Char},    // Base64 URL character
            {"Z",        DE::badBase64Length},  // Incomplete codepoint
            {"Zm9vY",    DE::badBase64Length},  // Incomplete codepoint
            {"=Zm8",     DE::badBase64Padding}, // Invalid pad position
            {"Z=m8",     DE::badBase64Padding}, // Invalid pad position
            {"Zm=8",     DE::badBase64Padding}, // Invalid pad position
            {"Zm8=YmFy", DE::badBase64Padding}  // Pad before last quad
        };

        std::vector<uint8_t> decoded;
        for (const auto& vec: testVectors)
        {
            INFO("in = '" << vec.in << "'");
            decoded.clear();
            auto ec = Base64::decode(vec.in.data(), vec.in.size(), decoded);
            CHECK( ec == vec.errc );
        }
    }

    WHEN( "Pad required" )
    {
        using Base64Type = BasicBase64<false, true, true>;
        const std::vector<std::string> inputs = {"Zg", "Zg=", "Zm8", "Zm9vYmE"};

        std::vector<uint8_t> decoded;
        for (const auto& in: inputs)
        {
            INFO("in = '" << in << "'");
            decoded.clear();
            auto ec = Base64Type::decode(in.data(), in.size(), decoded);
            CHECK( ec == DecodingErrc::badBase64Length );
        }
    }

    WHEN( "Base 64 URL" )
    {
        const std::vector<std::string> inputs = {
            "Zm8+", // Invalid character
            "Zm8/"  // Invalid character
        };

        std::vector<uint8_t> decoded;
        for (const auto& in: inputs)
        {
            INFO("in = '" << in << "'");
            decoded.clear();
            auto ec = Base64Url::decode(in.data(), in.size(), decoded);
            CHECK( ec == DecodingErrc::badBase64Char );
        }
    }
}

