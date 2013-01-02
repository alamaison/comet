#include <boost/mpl/vector.hpp>
#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/bstr.h>
#include <comet/variant.h>

#include <stdexcept>

using comet::bstr_t;
using comet::variant_t;

using std::runtime_error;

BOOST_AUTO_TEST_SUITE( variant_tests )

template<typename U>
void compile_operators(const U& y)
{
    variant_t x;

    x != y;
    x == y;
    x <  y;
    x <= y;
    x >  y;
    x >= y;

    y != x;
    y == x;
    y <  x;
    y <= x;
    y >  x;
    y >= x;

    variant_t v1(y);
    variant_t v2 = y;

    v1 = y;
}

template<typename U>
void compile_operators2(const U& y)
{
    variant_t x;

    x != y;
    x == y;
    x <  y;
    x <= y;
    x >  y;
    x >= y;

    y != x;
    y == x;
    y <  x;
    y <= x;
    y >  x;
    y >= x;

    x != U();
    x == U();
    x <  U();
    x <= U();
    x >  U();
    x >= U();

    U() != x;
    U() == x;
    U() <  x;
    U() <= x;
    U() >  x;
    U() >= x;

    variant_t v1(y);
    variant_t v2 = y;

    v1 = y;
}

BOOST_AUTO_TEST_CASE( variant_t_comparisons )
{
    variant_t v = 8;
    variant_t w = short(8);

    if (v != w) throw std::runtime_error("variant_t is broken");

    // Checking that variant_t comparisons can be compiled
    compile_operators2(variant_t());

    compile_operators2(short());
    compile_operators2(long());
    compile_operators2(int());
    compile_operators2(float());
    compile_operators2(double());

    // compile_operators2(bstr<true>());
    compile_operators2(bstr_t());
    compile_operators2(variant_t());
    compile_operators("");
    compile_operators(L"");

    compile_operators2(std::wstring());
    compile_operators2(std::string());
}

BOOST_AUTO_TEST_CASE( wstring_conversion )
{
    variant_t v1 = L"foo";
    variant_t v2( L"foo" );
    variant_t v3; v3 = L"foo";

    std::wstring s1 = v1;
    std::wstring s2( v2 );
    std::wstring s3; s3 = v3;

    BOOST_CHECK_MESSAGE(s1 == L"foo", "variant_t/wstring conversion is broken");
    BOOST_CHECK_MESSAGE(s2 == L"foo", "variant_t/wstring conversion is broken");
    BOOST_CHECK_MESSAGE(s3 == L"foo", "variant_t/wstring conversion is broken");

    variant_t v1_ = s1;
    variant_t v2_( s2 );
    variant_t v3_; v3_ = s3;

    BOOST_CHECK_MESSAGE(
        v1_ == L"foo", "wstring/variant_t conversion is broken");
    BOOST_CHECK_MESSAGE(
        v2_ == L"foo", "wstring/variant_t conversion is broken");
    BOOST_CHECK_MESSAGE(
        v3_ == L"foo", "wstring/variant_t conversion is broken");
}


void dummy_takes_bstr_t_arg(const bstr_t&) {}

BOOST_AUTO_TEST_CASE( bstr_t_conversion )
{
    variant_t v1 = L"foo";
    variant_t v2( L"foo" );
    variant_t v3; v3 = L"foo";

    bstr_t s1 = v1;
    bstr_t s2( v2 );
    bstr_t s3; s3 = v3;

    BOOST_CHECK_MESSAGE(s1 == L"foo", "variant_t/bstr_t conversion is broken");
    BOOST_CHECK_MESSAGE(s2 == L"foo", "variant_t/bstr_t conversion is broken");
    BOOST_CHECK_MESSAGE(s3 == L"foo", "variant_t/bstr_t conversion is broken");

    variant_t v1_ = s1;
    variant_t v2_( s2 );
    variant_t v3_; v3_ = s3;

    BOOST_CHECK_MESSAGE(
        v1_ == L"foo", "bstr_t/variant_t conversion is broken");
    BOOST_CHECK_MESSAGE(
        v2_ == L"foo", "bstr_t/variant_t conversion is broken");
    BOOST_CHECK_MESSAGE(
        v3_ == L"foo", "bstr_t/variant_t conversion is broken");

    dummy_takes_bstr_t_arg(v1);
}

BOOST_AUTO_TEST_CASE( equality )
{
    variant_t a(false), b(L"XYZ"), c(true);

    if( a == b || !(a != b) )
        throw std::exception("variant_t::operator== is broken");

    if( b == a || !(b != a) )
        throw std::exception("variant_t::operator== is broken");

    if( a == c || !(a != c) )
        throw std::exception("variant_t::operator== is broken");

    if( b == c || !(b != c) )
        throw std::exception("variant_t::operator== is broken");

    if( c == b || !(c != b) )
        throw std::exception("variant_t::operator== is broken");
}

void expect_equal(const variant_t& lhs, const variant_t& rhs)
{
    if (!(lhs == rhs))
        throw runtime_error("lhs == rhs not true");
    if (lhs != rhs)
        throw runtime_error("lhs != rhs");
    if (lhs < rhs)
        throw runtime_error("lhs < rhs");
    if (lhs > rhs)
        throw runtime_error("lhs > rhs");
}

void expect_less_than(const variant_t& lhs, const variant_t& rhs)
{
    if (!(lhs < rhs))
        throw runtime_error("lhs < rhs not true");
    if (lhs == rhs)
        throw runtime_error("lhs == rhs");
    if (!(lhs != rhs))
        throw runtime_error("lhs != rhs not true");
    if (lhs > rhs)
        throw runtime_error("lhs > rhs");
}

void expect_greater_than(const variant_t& lhs, const variant_t& rhs)
{
    if (!(lhs > rhs))
        throw runtime_error("lhs > rhs not true");
    if (lhs == rhs)
        throw runtime_error("lhs == rhs");
    if (!(lhs != rhs))
        throw runtime_error("lhs != rhs not true");
    if (lhs < rhs)
        throw runtime_error("lhs < rhs");
}

// I1, UI2, VT_UI4, UI8 and UINT

typedef boost::mpl::vector<
    char, unsigned char, short, unsigned short, int, unsigned int,
    long, unsigned long, LONGLONG, ULONGLONG> numeric_types;

BOOST_AUTO_TEST_CASE_TEMPLATE( variant_numeric_conversion, T, numeric_types )
{
    expect_equal(T(0), T(0));
    expect_less_than(T(0), T(1));
    expect_greater_than(T(1), T(0));
}

BOOST_AUTO_TEST_SUITE_END()