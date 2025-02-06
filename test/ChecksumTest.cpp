#include <catch2/catch_test_macros.hpp>
#include "MiddleWare.h"

using namespace rgc;


uint16_t calculate_checksum(const uint8_t *data, size_t length) {
    uint32_t sum = 0;

    // Sum all 16-bit words
    for (size_t i = 0; i < length; i += 2) {
        uint16_t word = (data[i] << 8) + (i + 1 < length ? data[i + 1] : 0);
        sum += word;
        
        // Add the carry if any
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    // Return the one's complement of the sum
    return ~sum & 0xFFFF;
}


TEST_CASE( "For provided data, we get the expected checksum" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    REQUIRE(MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf)) == 0xebef);

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
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xeb, 0xef };

    REQUIRE(MiddleWare::verifyChecksum(myBuf, sizeof(myBuf)));

}

TEST_CASE( "For provided manipulated data, we get the false checksum verify" )
{
    // Likely passes
    uint8_t myBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x05, 0x07, 0x08, 0xeb, 0xef };

    REQUIRE(MiddleWare::verifyChecksum(myBuf, sizeof(myBuf)) == false);

}


TEST_CASE( "foo" )
{
    // Likely passes
    uint8_t myBuf[] = { 0, 1, 0, 0, 116, 101, 115, 116, 49 };
    checksum_t checksum = MiddleWare::rfc1071Checksum(myBuf, sizeof(myBuf));
    checksum_t checksum2 = calculate_checksum(myBuf, sizeof(myBuf));

    REQUIRE(checksum == 0x24e7);
    REQUIRE(checksum == checksum2);


    uint8_t myBuf2[] = { 0, 1, 0, 0, 116, 101, 115, 116, 49, 0x24, 0xe7 };
    REQUIRE(MiddleWare::verifyChecksum(myBuf2, sizeof(myBuf2)));
}