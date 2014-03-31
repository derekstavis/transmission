#include "transmission.h"
#include "clients.h"

#include "libtransmission-test.h"

#define TEST_CLIENT(A, B) \
    tr_clientForId (buf, sizeof (buf), A); \
    check_streq (B, buf);

int
main (void)
{
    char buf[128];

    TEST_CLIENT ("-FC1013-", "FileCroc 1.0.1.3");
    TEST_CLIENT ("-MR1100-", "Miro 1.1.0.0");
    TEST_CLIENT ("-TR0006-", "Transmission 0.6");
    TEST_CLIENT ("-TR0072-", "Transmission 0.72");
    TEST_CLIENT ("-TR111Z-", "Transmission 1.11+");
    TEST_CLIENT ("O1008132", "Osprey 1.0.0");
    TEST_CLIENT ("TIX0193-", "Tixati 1.93");

    /* gobbledygook */
    TEST_CLIENT ("-IIO\x10\x2D\x04-", "-IIO%10-%04-");
    TEST_CLIENT ("-I\05O\x08\x03\x01-", "-I%05O%08%03%01-");

    TEST_CLIENT (
        "\x65\x78\x62\x63\x00\x38\x7A\x44\x63\x10\x2D\x6E\x9A\xD6\x72\x3B\x33\x9F\x35\xA9",
        "BitComet 0.56");
    TEST_CLIENT (
        "\x65\x78\x62\x63\x00\x38\x4C\x4F\x52\x44\x32\x00\x04\x8E\xCE\xD5\x7B\xD7\x10\x28",
        "BitLord 0.56");

    /* cleanup */
    return 0;
}

