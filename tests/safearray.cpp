#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/assert.h> // assert_failed
#include <comet/bstr.h>
#include <comet/currency.h>
#include <comet/datetime.h>
#include <comet/ptr.h>
#include <comet/safearray.h>
#include <comet/variant.h>

using comet::assert_failed;
using comet::bstr_t;
using comet::currency_t;
using comet::com_ptr;
using comet::datetime_t;
using comet::safearray_t;
using comet::variant_bool_t;
using comet::variant_t;

BOOST_AUTO_TEST_SUITE( safearray_tests )

BOOST_AUTO_TEST_CASE( iteration )
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
        std::cout << std::endl;
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

BOOST_AUTO_TEST_CASE( references )
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

BOOST_AUTO_TEST_CASE( sort )
{
    safearray_t<variant_t> a;
    a.push_back( L"Foo" );
    a.push_back( L"Bar" );
    std::sort( a.begin(), a.end() );
}

BOOST_AUTO_TEST_SUITE_END()


// Compile-time tests:

// 1. http://groups.yahoo.com/group/tlb2h/message/25
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
    static void check() { T *a = &safearray_t<T>()[0]; (void*)a; }
};

template<typename T, typename S>
struct check_array_special
{
    static void check() { T *a = &safearray_t<S>()[0]; (void*)a; }
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