#include <catch2/catch_test_macros.hpp>
#include "MiddleWare.h"

using namespace rgc;

TEST_CASE( "For provided data, we get the expected checksums" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    REQUIRE(MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf)) == 0xefeb);

}

TEST_CASE( "For provided data, we get the expected checksum verify" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xef, 0xeb };

    REQUIRE(MiddleWare::verifyChecksum(myBuf, sizeof(myBuf)));

}