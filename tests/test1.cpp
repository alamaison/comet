#define COMET_ASSERT_THROWS_ALWAYS

#include <boost/test/unit_test.hpp>
#include <boost/mpl/vector.hpp>

#include <comet/comet.h>
#include <comet/enum.h>
#include <comet/smart_enum.h>
#include <comet/server.h>
#include <comet/datetime.h>
#include <comet/safearray.h>
#include <comet/lw_lock.h>
#include <comet/uuid.h>
#include <iostream>
#include <sstream>
#include <exception>

using namespace std;
using namespace comet;

// An interface declaration used by some of the tests
struct IDummyInterface : public IUnknown
{
};

template<>
struct comet::comtype<IDummyInterface>
{
    typedef comet::nil base;
    static const IID& uuid() {
        static const GUID iid =
        { 0x546aefac, 0x6ecc, 0x44bf, { 0x8d, 0x17, 0x13, 0x83, 0x46, 0x24, 0xd4, 0x15 } };
        return iid;
    }
};

// Compile-time tests:
// 1. com_ptr<IForward> should be usable even though IForward is not defined.
struct IForward;

com_ptr<IForward> return_forward();

void accept_forward(const com_ptr<IForward>&);

#if 0
Visual C++ lets you compile this:
struct astruct
{
    com_ptr<IForward> forward_;
};

// but Intel does not -
// it would be nice if you could as it would mean that you
// could arbitrarily replace raw pointers with smart
// pointers.

// I'm not sure which compiler is correct here.
#endif

// 2. http://groups.yahoo.com/group/tlb2h/message/25
// There was an error in the definition of impl::sa_traits
// for char and unsigned char instantiations.
// While we are at it, check for the other safearray
// traits too.

// NOTE: This function is never meant to be
// executed - the test is purely to make sure that
// it compiles with no warnings or errors.

template<typename T>
struct check_array
{
    static void check() { T *a = &safearray_t<T>()[0]; }
};
template<typename T, typename S>
struct check_array_special
{
    static void check() { T *a = &safearray_t<S>()[0]; }
};

void check_safearray_traits()
{
    check_array<char>::check();
    check_array<unsigned char>::check();
    check_array<long>::check();
    check_array<unsigned long>::check();
    check_array<short>::check();
    check_array<unsigned short>::check();
    check_array<float>::check();
    check_array<double>::check();
    check_array<com_ptr<::IUnknown> >::check();
    check_array<com_ptr<::IDispatch> >::check();
    check_array<currency_t>::check();
    check_array<datetime_t>::check();
    check_array<variant_bool_t>::check();
    check_array_special<variant_bool_t, bool>::check();
}

// 3. http://groups.yahoo.com/group/tlb2h/message/18
void check_upcast()
{
    com_ptr<IDummyInterface> d;
    com_ptr<IUnknown> unknown;
    unknown = d;
}

// This tests for a reference counting bug encountered in Comet A5 -
// the destructor for com_ptr<> was missing so Release() was never called.
BOOST_AUTO_TEST_CASE( com_ptr_destruction )
{
    long count_before = comet::module().rc();
    bool destructor_executed;
    {
        struct A : public simple_object<IDummyInterface>
        {
            bool &destructor_executed_;
            A(bool &destructor_executed) :
                destructor_executed_(destructor_executed) {}
            ~A() { destructor_executed_ = true; }
        };

        com_ptr<IDummyInterface> pA = new A(destructor_executed);
        BOOST_CHECK_MESSAGE(
            comet::module().rc() == count_before + 1,
            "Reference count not incremented!");
    }

    BOOST_CHECK_MESSAGE(destructor_executed, "Destructor not executed!");
    BOOST_CHECK_MESSAGE(
        comet::module().rc() == count_before, "Reference count left changed!");
}


// This tests for a bug we found where the assignment operator for key_base
// was missing.
BOOST_AUTO_TEST_CASE( regkey_assignment )
{
    regkey k1 = regkey(HKEY_LOCAL_MACHINE).open(
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ );
    regkey k2;
    k2 = k1;
    k1.close();
    // This will fail if it is the exact same bug that we ran into before.
    k2.flush();
}

// This tests for a VC++ 6.0 bug where the compiler-generated assignment
// operator for name_iterator was incorrect.
BOOST_AUTO_TEST_CASE( name_iterator_assignment )
{
    regkey::info_type::subkeys_type::iterator it;
    {
        regkey key = regkey(HKEY_LOCAL_MACHINE).open(
            "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            KEY_READ);
        regkey::subkeys_type subkeys = key.enumerate().subkeys();

        // Invoke assignment operator for name_iterator
        it = subkeys.begin();
        key.close();
        BOOST_CHECK_MESSAGE(
            it != subkeys.end(),
            "There are no subkeys - this is a very strange Windows "
            "installation!");
    }
    // Dereference the key iterator after everything else has been closed.
    *it;
}


// This tests for this bug:
// http://groups.yahoo.com/group/tlb2h/message/27
// Note however, if it's the exact same bug as before,
// it will access violate rather than throwing an exception.
BOOST_AUTO_TEST_CASE( null_bstr_t_to_std_string )
{
    BSTR v = 0;
    bstr_t s(v);
    BOOST_CHECK_EQUAL(s.w_str().length(), 0U);
    BOOST_CHECK_EQUAL(s.s_str().length(), 0U);
}


// This one doesn't actually get to run the check - it just access violates
BOOST_AUTO_TEST_CASE( uninitialised_bstr_comparison )
{
    bstr_t g;
    BOOST_CHECK_MESSAGE(!(g != 0), "operator!= busted");
}


BOOST_AUTO_TEST_CASE( typelist_index_of )
{
    typedef make_list< int, long, int >::result LIST;
    BOOST_CHECK_EQUAL((typelist::index_of< LIST, long >::value), 1);
}

struct string_formats_fixture
{
    static variant_t foo() { return L"foo";  }

    static void s1(const std::string&) {}
    static void s2(const std::wstring&) {}
    static void s3(const bstr_t&) {}
    static void s4(const wchar_t*) {}
};

BOOST_FIXTURE_TEST_CASE( bstr_t_conversion, string_formats_fixture )
{
    bstr_t s = foo();
    bstr_t s1( foo() );
    std::wstring t;

    bstr_t s0( foo() );
    s = foo();

    t = s.w_str();
}


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

BOOST_AUTO_TEST_CASE( com_ptr_com_cast )
{
    IStorage* itf2 = 0;

    com_ptr<IPersist> itf1( com_cast(itf2) );

    itf1 = com_cast(itf1);

}

BOOST_AUTO_TEST_CASE( variant_t_wstring_conversion )
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

BOOST_AUTO_TEST_CASE( variant_t_bstr_t_conversion )
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

BOOST_AUTO_TEST_CASE( datetime_t_from_zero )
{
    DATE d1 = 0;
    datetime_t d2( d1 );
    BOOST_CHECK(d2.zero());
}

BOOST_AUTO_TEST_CASE( currency_t_conversion )
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

BOOST_AUTO_TEST_CASE( safearray_t_iteration )
{
    safearray_t<long> sa(6, 1);
    {
        sa[1] = 2;
        sa[2] = 6;
        sa[3] = 4;
        sa[4] = 3;
        sa[5] = 1;
        sa[6] = 5;
    }
#ifdef _DEBUG
    // Check safearray debug iterators.

    safearray_t<long> sa_other(1,1);

    try {
        if (sa_other.begin() == sa.begin()) { }
        BOOST_ERROR(
            "safearray_t: debug iterator busted - comparison between "
            "foreign iterators");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        if (sa_other.begin() < sa.begin()) { }
        BOOST_ERROR(
            "safearray_t: debug iterator busted - comparison between "
            "foreign iterators");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        if (sa_other.begin() > sa.begin()) { }
        BOOST_ERROR(
            "safearray_t: debug iterator busted - comparison between "
            "foreign iterators");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        if (sa_other.begin() <= sa.begin()) { }
        BOOST_ERROR(
            "safearray_t: debug iterator busted - comparison between "
            "foreign iterators");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        if (sa_other.begin() >= sa.begin()) { }
        BOOST_ERROR(
            "safearray_t: debug iterator busted - comparison between "
            "foreign iterators");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        sa_other[2] = 0;
        BOOST_ERROR("safearray_t: debug iterator busted - out of range access");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        --sa_other.begin();
        BOOST_ERROR(
            "safearray_t: debug iterator busted - iterator out of range");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        *sa_other.end() = 0;
        BOOST_ERROR("safearray_t: debug iterator busted - assignment to end()");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        ++sa_other.end();
        BOOST_ERROR(
            "safearray_t: debug iterator busted - iterate beyond end()");
    } catch (const assert_failed &) { /*Expected*/ }
    try {
        sa_other.begin()+3;
        BOOST_ERROR("safearray_t: debug iterator busted - assign beyond end()");
    } catch (const assert_failed &) { /*Expected*/ }

#endif

    std::sort(sa.begin(), sa.end());

    safearray_t<long> sa2 = sa;
    {

        if (sa2[1] != 1 ||
            sa2[2] != 2 ||
            sa2[3] != 3 ||
            sa2[4] != 4 ||
            sa2[5] != 5 ||
            sa2[6] != 6) throw std::runtime_error("safearray_t is busted!");
    }

    std::sort(sa2.rbegin(), sa2.rend());
    if (sa2[1] != 6 ||
        sa2[2] != 5 ||
        sa2[3] != 4 ||
        sa2[4] != 3 ||
        sa2[5] != 2 ||
        sa2[6] != 1)
    {
        throw std::runtime_error("safearray_t reverse sort is busted!");
    }

    sa2.erase(sa2.begin() +2);
    if (sa2.size() != 5 ||
        sa2[1] != 6 ||
        sa2[2] != 5 ||
        sa2[3] != 3 ||
        sa2[4] != 2 ||
        sa2[5] != 1)
    {
        std::cout << sa2.size()<< ':';
        for (safearray_t<long>::const_iterator it = sa2.begin();
            it != sa2.end(); ++it)
        {
            std::cout << *it << ',';
        }
        std::cout << endl;
        throw std::runtime_error("safearray_t erase(it) is busted!");
    }

    sa2.pop_back();
    if (sa2.size() != 4 ||
        sa2[1] != 6 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 )
    {
        throw std::runtime_error("safearray_t pop_back is busted!");
    }
    sa2.pop_front();
    if (sa2.size() != 3 ||
        sa2[1] != 5 || sa2[2] != 3 || sa2[3] != 2 )
    {
        throw std::runtime_error("safearray_t pop_front is busted!");
    }
    sa2.push_front(9);
    if (sa2.size() != 4 ||
        sa2[1] != 9 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 )
    {
        throw std::runtime_error("safearray_t push_front is busted!");
    }
    sa2.push_back(9);
    if (sa2.size() != 5 ||
        sa2[1] != 9 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 || sa2[5] != 9)
    {
        throw std::runtime_error("safearray_t push_back is busted!");
    }

    sa2.erase(sa2.begin(), sa2.end());
    if (sa2.size() != 0)
    {
        throw std::runtime_error("safearray_t erase(begin,end) is busted!");
    }
    try
    {
        sa2.pop_back();
    }
    catch(...)
    {
        throw std::runtime_error(
            "safearray_t pop_back of empty list is busted!");
    }

    safearray_t<bool> sab(5,2);
    sab[6] = true;
    sab[5] = false;
    sab[4] = false;
    sab[3] = false;
    sab[2] = true;

    if ( !sab[2] || sab[3] || sab[4] || sab[5] || !sab[6] )
        throw std::runtime_error("safearray_t<bool> is busted");

    std::ostringstream os;
    /*
    for (safearray_t<long>::reverse_iterator it = sa.rbegin();
         it != sa.rend(); ++it)
        os << *it << std::endl;
    */
}

BOOST_AUTO_TEST_CASE( safearray_t_references )
{
    safearray_t<bstr_t> sa( 2, 1 );
    sa[1] = L"String no. 1";
    sa[2] = L"String no. 2";

    SAFEARRAY* psa = sa.detach();

    {
        safearray_t<bstr_t>& sa2 = safearray_t<bstr_t>::create_reference(psa);

        sa2[1] = L"FOO";
    }

    {
        safearray_t<bstr_t>& sa2 = safearray_t<bstr_t>::create_reference(psa);

        if (sa2[1] != L"FOO")
            throw std::runtime_error(
                "safearray_t<>::create_reference is broken");
    }
}

BOOST_AUTO_TEST_CASE( thread_lock )
{
    struct MyThread : public thread
    {
        const lw_lock& lock_;

        MyThread(const lw_lock& lock)
            : lock_(lock)
        {}

        DWORD thread_main()
        {
            auto_reader_lock arl(lock_);
            Sleep(100);
            return 0;
        }
    };

    lw_lock lock;

    MyThread t(lock);
    t.start();
    {
        auto_writer_lock awl(lock);
        Sleep(100);
    }

    if (WaitForSingleObject(t.handle(), 5000) != WAIT_OBJECT_0)
        throw std::runtime_error("class thread is broken");
}

BOOST_AUTO_TEST_CASE( bstr_t_converted_length )
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

BOOST_AUTO_TEST_CASE( bstr_t_embedded_nulls )
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

BOOST_AUTO_TEST_CASE( bstr_t_from_variant_reference )
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

BOOST_AUTO_TEST_CASE( bstr_t_sort )
{
    bstr_t s = L"Sofus Mortensen";
    std::sort(s.begin(), s.end());
    BOOST_CHECK(s == L" MSeefnnoorsstu");
}

BOOST_AUTO_TEST_CASE( bstr_t_sort_empty )
{
    bstr_t s;
    std::sort(s.begin(), s.end());
    BOOST_CHECK(s.empty());
}

BOOST_AUTO_TEST_CASE( bstr_t_assign )
{
    std::wstring s1 = L"Sofus Mortensen";
    bstr_t s2( s1.begin(), s1.end() );

    s2.assign( s1.begin(), s1.end() );
    BOOST_CHECK(s1 == L"Sofus Mortensen");
    BOOST_CHECK(s2 == L"Sofus Mortensen");
}

BOOST_AUTO_TEST_CASE( bstr_t_construct_by_repetition )
{
    bstr_t s(80,  L' ');
    BOOST_CHECK(
        s == L"                                        "
        L"                                        ");
}

BOOST_AUTO_TEST_CASE( bstr_t_assignment )
{
    bstr_t s = L"foo";
    bstr_t t;

    t = s;
    BOOST_CHECK_MESSAGE(
        s.begin() != t.begin(),
        "default assignment operator is not copying the string");
}

BOOST_AUTO_TEST_CASE( bstr_t_conversion2 )
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

BOOST_AUTO_TEST_CASE( variant_t_equality )
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

BOOST_AUTO_TEST_CASE( uuid_t_streaming )
{
    uuid_t x = IID_IUnknown;

    {
        uuid_t y ;
        std::stringstream f;
        f << x; f >> y;
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");
        if (x != y)
            throw std::exception("uuid_t: streaming broken");
    }

    {
        uuid_t y ;
        std::wstringstream f;
        f << x; f >> y;
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");
        if (x != y)
            throw std::exception("uuid_t: streaming broken");
    }

    {
        uuid_t y ;
        std::wstringstream f;
        f << x; f >> y >> y;
        if (!f.eof())
            throw std::exception("uuid_t: streaming broken - stream should be eof");
        if (x != y)
            throw std::exception("uuid_t: streaming broken");
    }

    {
        uuid_t y ;
        std::stringstream f;
        f << " \t\n" << x; f >> y;
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");
        if (x != y)
            throw std::exception("uuid_t: streaming broken - no ws skipping");
    }

    {
        uuid_t y ;
        std::wstringstream f;
        f << L" \t\n"<< x; f >> y;
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");
        if (x != y)
            throw std::exception("uuid_t: streaming broken - no ws skipping");
    }

    {
        uuid_t y ;
        std::stringstream f;
        f << x << ',';
        f >> y;
        if (x != y)
            throw std::exception("uuid_t: streaming broken");
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");

        char c = 0;
        f >> c;
        if (c != ',')
            throw std::exception(
                "uuid_t: streaming broken - ate too much from stream");
    }

    {
        uuid_t y ;
        std::wstringstream f;
        f << x << L","; f >> y;
        if (x != y)
            throw std::exception("uuid_t: streaming broken");
        if (!f)
            throw std::exception("uuid_t: streaming broken - stream is bad");

        wchar_t c = 0;
        f >> c;
        if (c != L',')
            throw std::exception(
                "uuid_t: streaming broken - ate too much from stream");
    }

    {
        uuid_t y;
        std::stringstream f;
        f.exceptions(std::ios::badbit);

        f << "00";
        try {
            f >> y;
            throw std::exception(
                "uuid_t: streaming broken - should have thrown exception "
                "- ill format");
        }
        catch (std::ios::failure&)
        {
            // ok
        }

    }

    {
        uuid_t y;
        std::wstringstream f;
        f.exceptions(std::ios::badbit);

        f << L"123123123123123123123123123123123123123";
        try {
            f >> y;
            throw std::exception(
                "uuid_t: streaming broken - should have thrown exception "
                "- ill format");
        }
        catch (std::ios::failure&)
        {
            // ok
        }

    }

}

BOOST_AUTO_TEST_CASE( uuid_t_from_bstr_t )
{
    uuid_t u1 = uuid_t::create();

    bstr_t s1( u1 );

    {
        uuid_t u2( s1 );
        if (u1 != u2)
            throw std::exception("uuid_t <-> bstr_t doesn't work");
    }

    bstr_t s2(u1, true); // Braces

    {
        uuid_t u2(s2);
        if (u1 != u2)
            throw std::exception(
                "uuid_t does not support UUID in curly brackets.");
    }
}

BOOST_AUTO_TEST_CASE( safearray_t_sort )
{
    safearray_t<variant_t> a;
    a.push_back( L"Foo" );
    a.push_back( L"Bar" );
    std::sort( a.begin(), a.end() );
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

template<> struct comet::comtype<IEnumUnknown>
{
    static const IID& uuid() throw() { return IID_IEnumUnknown; }
    typedef IUnknown base;
};

template<> struct comet::impl::type_policy<IUnknown*>
{
    template<typename S>
    static void init(IUnknown*& p, const S& s)
    {  p = s.get(); p->AddRef(); }

    static void clear(IUnknown*& p) { p->Release(); }
};

void empty_enum_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punk;
    ULONG count = 12;
    HRESULT hr = e->Next(1, &punk, &count);
    if (hr != S_FALSE)
        throw runtime_error("HRESULT is not S_FALSE");
    if (count != 0)
        throw runtime_error("Incorrect return count");
}

/**
* Empty stl_enumeration_t.
*/
BOOST_AUTO_TEST_CASE( stl_enumeration_t_empty )
{
    typedef std::vector< com_ptr<IUnknown> > collection_type;
    collection_type coll;
    com_ptr<IEnumUnknown> e = new stl_enumeration_t<
        IEnumUnknown, collection_type, IUnknown*>(coll);
    empty_enum_test(e);
}

/**
* Empty smart_enumeration.
*/
BOOST_AUTO_TEST_CASE( smart_enumeration_empty )
{
    typedef std::auto_ptr< std::vector< com_ptr<IUnknown> > > smart_collection;
    smart_collection coll(new std::vector< com_ptr<IUnknown> >());
    com_ptr<IEnumUnknown> e = new smart_enumeration<
        IEnumUnknown, smart_collection, IUnknown*>(coll);
    coll.reset(); // force enum to be the only owner (should happen
    // regardless with an auto_ptr)
    empty_enum_test(e);
}

class test_obj : public simple_object<nil> {};

vector< com_ptr<IUnknown> > test_collection()
{
    vector< com_ptr<IUnknown> > collection;
    collection.push_back(new test_obj());
    collection.push_back(new test_obj());
    collection.push_back(new test_obj());
    return collection;
}

void enum_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punks[3];

    // Test validity
    for (int i = 0; i < 3; ++i)
    {
        IUnknown* punk;
        ULONG count = 12;
        HRESULT hr = e->Next(1, &punk, &count);
        if (hr != S_OK)
            throw runtime_error("HRESULT is not S_OK");

        if (count != 1)
            throw runtime_error("Incorrect return count");

        if (punk == NULL)
            throw runtime_error("NULL returned instead of object");
        punk->AddRef();
        punk->Release();

        punks[i] = punk; // store for uniqueness test later
    }

    empty_enum_test(e);

    // Test uniqueness
    if (punks[0] == punks[1])
       throw runtime_error("Same object returned twice");
    if (punks[0] == punks[2])
       throw runtime_error("Same object returned twice");
    if (punks[2] == punks[1])
       throw runtime_error("Same object returned twice");
}

void enum_chunk_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punk[4];
    ULONG count = 12;
    HRESULT hr = e->Next(4, punk, &count); // request more than expected
    if (hr != S_FALSE)
        throw runtime_error("HRESULT is not S_FALSE");

    if (count != 3)
        throw runtime_error("Incorrect return count");

    // Test validity
    for (int i = 0; i < 3; ++i)
    {
        if (punk[i] == NULL)
            throw runtime_error("NULL returned instead of object");
        punk[i]->AddRef();
        punk[i]->Release();
    }

    empty_enum_test(e);

    // Test uniqueness
    if (punk[0] == punk[1])
       throw runtime_error("Same object returned twice");
    if (punk[0] == punk[2])
       throw runtime_error("Same object returned twice");
    if (punk[2] == punk[1])
       throw runtime_error("Same object returned twice");
}

/**
 * Populated stl_enumeration_t.
 */
BOOST_AUTO_TEST_CASE( stl_enumeration_t_non_empty )
{
    typedef std::vector< com_ptr<IUnknown> > collection_type;
    collection_type coll = test_collection();
    com_ptr<IEnumUnknown> e = new stl_enumeration_t<
        IEnumUnknown, collection_type, IUnknown*>(coll);
    enum_test(e);
    e->Reset();
    enum_chunk_test(e);
}


/**
 * Populated smart_enumeration.
 */
BOOST_AUTO_TEST_CASE( smart_enumeration_non_empty )
{
    typedef std::auto_ptr< std::vector< com_ptr<IUnknown> > > smart_collection;
    smart_collection coll(
        new std::vector< com_ptr<IUnknown> >(test_collection()));
    com_ptr<IEnumUnknown> e = new smart_enumeration<
        IEnumUnknown, smart_collection, IUnknown*>(coll);
    coll.reset(); // force enum to be the only owner (should happen
    // regardless with an auto_ptr)
    enum_test(e);
    e->Reset();
    enum_chunk_test(e);
}

