#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/currency.h>
#include <comet/variant.h>

using comet::currency_t;
using comet::variant_t;

BOOST_AUTO_TEST_SUITE( currency_tests )

BOOST_AUTO_TEST_CASE( conversion )
{
    {
        currency_t c1(12.3);
        currency_t c2 = 123;
        currency_t c3 = 123L;
    }

    {
        currency_t c1(12.3);
        BOOST_CHECK_EQUAL(c1, 12.3);
        c1 = 12.3;
        BOOST_CHECK_EQUAL(c1, 12.3);

        currency_t c2(123);
        BOOST_CHECK_EQUAL(c2, 123);
        c2 = 123;
        BOOST_CHECK_EQUAL(c2, 123);

        currency_t c3(123L);
        BOOST_CHECK_EQUAL(c3, 123L);
        c3 = 123L;
        BOOST_CHECK_EQUAL(c3, 123L);
    }

    currency_t c( 123.456 );
    variant_t v = c;

    BOOST_CHECK_EQUAL(v, 123.456);
}

BOOST_AUTO_TEST_SUITE_END()