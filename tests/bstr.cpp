#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/bstr.h>
#include <comet/variant.h>

using comet::bstr_t;
using comet::variant_t;

BOOST_AUTO_TEST_SUITE( bstr_tests )

// This tests for this bug:
// http://groups.yahoo.com/group/tlb2h/message/27
// Note however, if it's the exact same bug as before,
// it will access violate rather than throwing an exception.
BOOST_AUTO_TEST_CASE( null_to_std_string )
{
    BSTR v = 0;
    bstr_t s(v);
    BOOST_CHECK_EQUAL(s.w_str().length(), 0U);
    BOOST_CHECK_EQUAL(s.s_str().length(), 0U);
}

// This one doesn't actually get to run the check - it just access violates
BOOST_AUTO_TEST_CASE( uninitialised_comparison )
{
    bstr_t g;
    BOOST_CHECK_MESSAGE(!(g != 0), "operator!= busted");
}

struct string_formats_fixture
{
    static variant_t foo() { return L"foo";  }

    static void s1(const std::string&) {}
    static void s2(const std::wstring&) {}
    static void s3(const bstr_t&) {}
    static void s4(const wchar_t*) {}
};

BOOST_FIXTURE_TEST_CASE( conversion, string_formats_fixture )
{
    bstr_t s = foo();
    bstr_t s1( foo() );
    std::wstring t;

    bstr_t s0( foo() );
    s = foo();

    t = s.w_str();
}

BOOST_AUTO_TEST_CASE( converted_length )
{
    bstr_t bs = L"Sofus";
    size_t l = bs.length();

    std::string t = "Sofus";
    size_t l1 = t.length();

    std::string s = bs.s_str();
    size_t l2 = s.length();

    BOOST_CHECK_EQUAL(l1, l2);
    BOOST_CHECK_EQUAL(s, t);
}

BOOST_AUTO_TEST_CASE( embedded_nulls )
{
    wchar_t ws1[] = L"foo\0bar"; // 7 characters terminated with a null

    std::wstring ws2( ws1, 7 );

    if (ws2.length() != 7) throw std::logic_error("Test is broken");

    bstr_t bs1 = ws2;

    if (bs1.length() != 7)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (failed converting "
            "from std::string)");

    if (bs1 != ws2)
        throw std::runtime_error("bstr_t does not support embedded nulls.");

    const char s1[] = "foo\0bar"; // 7 characters terminated with a null

    std::string s2( s1, 7 );

    if (s2.length() != 7) throw std::logic_error("Test is broken");

    bstr_t bs2 = s2;

    if (bs2.length() != 7)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (failed converting "
            "from std::wstring)");

    if (bs2 != ws2)
        throw std::runtime_error("bstr_t does not support embedded nulls.");

    ws2[6] = 'z'; // change string to L"foo\0baz"
    if (bs1 == ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null).");
    if (bs2 == ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null)");

    if (bs1 >= ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null).");
    if (bs2 >= ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null).");

    ws2[4] = 'a'; // change string to L"foo\0aaz"

    if (bs1 <= ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null).");
    if (bs2 <= ws2)
        throw std::runtime_error(
            "bstr_t does not support embedded nulls (comparison ignores "
            "characters after null).");
}

BOOST_AUTO_TEST_CASE( from_variant_reference )
{
    BSTR s = SysAllocString(L"test");
    VARIANT raw_v;
    VariantInit(&raw_v);
    raw_v.vt = VT_BSTR | VT_BYREF;
    raw_v.pbstrVal = &s;

    {
        const variant_t& v = variant_t::create_reference(raw_v);

        bstr_t bs = v;
    }

    HRESULT hr = VariantClear(&raw_v);
}

BOOST_AUTO_TEST_CASE( sort )
{
    bstr_t s = L"Sofus Mortensen";
    std::sort(s.begin(), s.end());
    BOOST_CHECK(s == L" MSeefnnoorsstu");
}

BOOST_AUTO_TEST_CASE( sort_empty )
{
    bstr_t s;
    std::sort(s.begin(), s.end());
    BOOST_CHECK(s.empty());
}

BOOST_AUTO_TEST_CASE( assign )
{
    std::wstring s1 = L"Sofus Mortensen";
    bstr_t s2( s1.begin(), s1.end() );

    s2.assign( s1.begin(), s1.end() );
    BOOST_CHECK(s1 == L"Sofus Mortensen");
    BOOST_CHECK(s2 == L"Sofus Mortensen");
}

BOOST_AUTO_TEST_CASE( construct_by_repetition )
{
    bstr_t s(80,  L' ');
    BOOST_CHECK(
        s == L"                                        "
        L"                                        ");
}

BOOST_AUTO_TEST_CASE( copy_assignment )
{
    bstr_t s = L"foo";
    bstr_t t;

    t = s;
    BOOST_CHECK_MESSAGE(
        s.begin() != t.begin(),
        "default assignment operator is not copying the string");
}

BOOST_AUTO_TEST_CASE( conversion2 )
{
    bstr_t s = "foo";
    if (s.length() != 3)
        throw std::exception("conversion from MBCS to wide char is broken");

    size_t l = s.length();

    std::string s1 = s;
    if (s1.length() != 3)
        throw std::exception(
            "conversion from wide char to narrow char is broken");

    std::wstring s2 = s;
    if (s2.length() != 3)
        throw std::exception("conversion from bstr_t to wstring");
}

BOOST_AUTO_TEST_SUITE_END()