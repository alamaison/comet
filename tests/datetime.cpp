#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/datetime.h>

#include <Windows.h> // DATE

using comet::datetime_t;

BOOST_AUTO_TEST_SUITE( datime_tests )

BOOST_AUTO_TEST_CASE( datetime_t_from_zero )
{
    DATE d1 = 0;
    datetime_t d2( d1 );
    BOOST_CHECK(d2.zero());
}

BOOST_AUTO_TEST_SUITE_END()
