#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/typelist.h>

using comet::make_list;
using comet::typelist::index_of;

BOOST_AUTO_TEST_SUITE( typelist_tests )

BOOST_AUTO_TEST_CASE( typelist_index_of )
{
    typedef make_list< int, long, int >::result LIST;
    BOOST_CHECK_EQUAL((index_of< LIST, long >::value), 1);
}

BOOST_AUTO_TEST_SUITE_END()