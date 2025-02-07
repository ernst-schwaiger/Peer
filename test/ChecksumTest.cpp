#include <catch2/catch_test_macros.hpp>
#include "MiddleWare.h"

using namespace rgc;




TEST_CASE( "For provided data, we get the expected checksum" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    REQUIRE(MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf)) == 0xefeb);

}

TEST_CASE( "For provided data, we check for consistent checksum" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    REQUIRE(MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf)) != 0xeaef); // Wrong Checksum -> Should fail

}

TEST_CASE( "For provided data, we get the expected checksum verify" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xef, 0xeb };

    REQUIRE(MiddleWare::verifyChecksum(myBuf, sizeof(myBuf)));

}

TEST_CASE( "For provided manipulated data, we get the false checksum verify" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x05, 0x07, 0x08, 0xef, 0xeb };

    REQUIRE(MiddleWare::verifyChecksum(myBuf, sizeof(myBuf)) == false);
    
}


TEST_CASE( "rfc1071" )
{
    // Does the example in https://www.rfc-editor.org/rfc/rfc1071, section 3
    uint8_t myBuf[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    checksum_t checksum = MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf));
    REQUIRE(checksum == static_cast<checksum_t>(~0xddf2));

    // Test an example with 'odd' length
    // https://www.rfc-editor.org/rfc/rfc1071, page 6: second group
    uint8_t oddBuf[] = {0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    checksum = MiddleWare::rfc1071Checksum(oddBuf, sizeof(oddBuf));
    REQUIRE(checksum == static_cast<checksum_t>(~0xf0eb));

}