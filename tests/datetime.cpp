#include <boost/mpl/vector.hpp>
#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/datetime.h>

#include <ctime>

#include <Windows.h> // DATE

using comet::datetime_t;

using std::localtime;

BOOST_AUTO_TEST_SUITE( datime_tests )

BOOST_AUTO_TEST_CASE( from_zero )
{
    DATE d1 = 0;
    datetime_t d2( d1 );
    BOOST_CHECK(d2.zero());
}

BOOST_AUTO_TEST_CASE( construct_directly )
{
    datetime_t d(2013, 1, 3, 23, 59, 58, 999);
    BOOST_CHECK(d.valid());
    BOOST_CHECK_EQUAL(d.year(), 2013);
    BOOST_CHECK_EQUAL(d.month(), 1);
    BOOST_CHECK_EQUAL(d.day(), 3);
    BOOST_CHECK_EQUAL(d.hour(), 23);
    BOOST_CHECK_EQUAL(d.minute(), 59);
    BOOST_CHECK_EQUAL(d.second(), 58);
    BOOST_CHECK_EQUAL(d.millisecond(), 999);
}

// The next few tests make sure that datetime_t correctly deals with
// the DATE format's crazy semantics.  The value when treated as a sortable
// double, (a continuous measure of the days since (or before) the epoch)
// is different from the underlying double DATE value because the fractional
// part is treated as unsigned in a DATE.
//
// See http://goo.gl/H4I4T

BOOST_AUTO_TEST_CASE( from_oledate_post_epoch )
{
    DATE oledate = 1.75;
    datetime_t d(oledate);

    BOOST_CHECK(d.valid());
    BOOST_CHECK_EQUAL(d.year(), 1899);
    BOOST_CHECK_EQUAL(d.month(), 12);
    BOOST_CHECK_EQUAL(d.day(), 31);
    BOOST_CHECK_EQUAL(d.hour(), 18);
    BOOST_CHECK_EQUAL(d.minute(), 0);
    BOOST_CHECK_EQUAL(d.second(), 0);
    BOOST_CHECK_EQUAL(d.millisecond(), 0);

    // Post epoch DATEs come out clean as a double
    BOOST_CHECK_EQUAL(d.get(), 1.75);
    BOOST_CHECK_EQUAL(d.as_sortable_double(), 1.75);
}

BOOST_AUTO_TEST_CASE( from_oledate_pre_epoch )
{
    DATE oledate = -1.75;
    datetime_t d(oledate);

    BOOST_CHECK(d.valid());
    BOOST_CHECK_EQUAL(d.year(), 1899);
    BOOST_CHECK_EQUAL(d.month(), 12);
    BOOST_CHECK_EQUAL(d.day(), 29);
    BOOST_CHECK_EQUAL(d.hour(), 18);
    BOOST_CHECK_EQUAL(d.minute(), 0);
    BOOST_CHECK_EQUAL(d.second(), 0);
    BOOST_CHECK_EQUAL(d.millisecond(), 0);

    // Pre epoch DATEs are not simply the days before the epoch as a negative
    BOOST_CHECK_EQUAL(d.get(), -1.75);
    BOOST_CHECK_EQUAL(d.as_sortable_double(), -0.25);
}

// The discontinuous DATE ranges meet at 0.75 and -0.75
BOOST_AUTO_TEST_CASE( from_oledate_magic_overlap )
{
    DATE oledate = 0.75;
    datetime_t d(oledate);

    BOOST_CHECK(d.valid());
    BOOST_CHECK_EQUAL(d.year(), 1899);
    BOOST_CHECK_EQUAL(d.month(), 12);
    BOOST_CHECK_EQUAL(d.day(), 30);
    BOOST_CHECK_EQUAL(d.hour(), 18);
    BOOST_CHECK_EQUAL(d.minute(), 0);
    BOOST_CHECK_EQUAL(d.second(), 0);
    BOOST_CHECK_EQUAL(d.millisecond(), 0);

    BOOST_CHECK_EQUAL(d.get(), 0.75);
    BOOST_CHECK_EQUAL(d.as_sortable_double(), 0.75);

    oledate = -0.75;
    datetime_t d2(oledate);

    BOOST_CHECK(d2.valid());
    BOOST_CHECK_EQUAL(d2.year(), 1899);
    BOOST_CHECK_EQUAL(d2.month(), 12);
    BOOST_CHECK_EQUAL(d2.day(), 30);
    BOOST_CHECK_EQUAL(d2.hour(), 18);
    BOOST_CHECK_EQUAL(d2.minute(), 0);
    BOOST_CHECK_EQUAL(d2.second(), 0);
    BOOST_CHECK_EQUAL(d2.millisecond(), 0);

    BOOST_CHECK_EQUAL(d2.get(), -0.75);
    BOOST_CHECK_EQUAL(d2.as_sortable_double(), 0.75);
}

void datetime_systemtime_match(const datetime_t& d, const SYSTEMTIME& st)
{
    BOOST_CHECK_EQUAL(d.year(), st.wYear);
    BOOST_CHECK_EQUAL(d.month(), st.wMonth);
    BOOST_CHECK_EQUAL(d.day(), st.wDay);
    BOOST_CHECK_EQUAL(d.hour(), st.wHour);
    BOOST_CHECK_EQUAL(d.minute(), st.wMinute);
    BOOST_CHECK_EQUAL(d.second(), st.wSecond);
    BOOST_CHECK_EQUAL(d.millisecond(), st.wMilliseconds);
}

void datetime_filetime_match(const datetime_t& d, const FILETIME& ft)
{
    SYSTEMTIME st;
    BOOST_CHECK(::FileTimeToSystemTime(&ft, &st));
    datetime_systemtime_match(d, st);
}

void systemtime_match(const SYSTEMTIME& st1, const SYSTEMTIME& st2)
{
    BOOST_CHECK_EQUAL(st1.wYear, st2.wYear);
    BOOST_CHECK_EQUAL(st1.wMonth, st2.wMonth);
    BOOST_CHECK_EQUAL(st1.wDay, st2.wDay);
    BOOST_CHECK_EQUAL(st1.wHour, st2.wHour);
    BOOST_CHECK_EQUAL(st1.wMinute, st2.wMinute);
    BOOST_CHECK_EQUAL(st1.wSecond, st2.wSecond);
    BOOST_CHECK_EQUAL(st1.wMilliseconds, st2.wMilliseconds);
}

void filetime_match(const FILETIME& ft1, const FILETIME& ft2)
{
    // Could match directly but the SYSTEMTIME conversion gives us more useful
    // error info

    SYSTEMTIME st1;
    BOOST_CHECK(::FileTimeToSystemTime(&ft1, &st1));
    SYSTEMTIME st2;
    BOOST_CHECK(::FileTimeToSystemTime(&ft2, &st2));
    systemtime_match(st1, st2);
}

// The tests use more than one fixed test date to try to catch both sides
// of DST (if any) whatever the timezone these tests are running in.
// This maximises our chances of finding errors.
// We call them `early` and `late` as summer and winter vary with where
// you are

// These test times are UTC.  Unix time is always UTC but FILETIME and
// SYSTEMTIME can also be local times.  Conversion to and from local times
// happens outside these test time classes.

// Tue, 03 Jan 2013 01:11:14 -0000
class early_time
{
public:
    time_t unix_time() const
    {
        return 1357175474;
    }

    FILETIME utc_filetime() const
    {
        FILETIME ft;
        LARGE_INTEGER i;
        i.QuadPart = 130016490740000000;
        ft.dwLowDateTime = i.LowPart;
        ft.dwHighDateTime = i.HighPart;
        return ft;
    }

    SYSTEMTIME utc_systemtime() const
    {
        SYSTEMTIME st;
        st.wYear = 2013;
        st.wMonth = 1;
        st.wDayOfWeek = 4;
        st.wDay = 3;
        st.wHour = 1;
        st.wMinute = 11;
        st.wSecond = 14;
        st.wMilliseconds = 0;

        return st;
    }

    int year() const
    {
        return 2013;
    }

    int month() const
    {
        return 1;
    }

    int day() const
    {
        return 3;
    }

    int hour() const
    {
        return 1;
    }

    int minute() const
    {
        return 11;
    }

    int second() const
    {
        return 14;
    }

    int millisecond() const
    {
        return 0;
    }
};

// Fri, 02 Aug 2013 16:43:56 -0000
class late_time
{
public:
    time_t unix_time() const
    {
        return 1375461836;
    }

    FILETIME utc_filetime() const
    {
        FILETIME ft;
        LARGE_INTEGER i;
        i.QuadPart = 130199354360000000;
        ft.dwLowDateTime = i.LowPart;
        ft.dwHighDateTime = i.HighPart;
        return ft;
    }

    SYSTEMTIME utc_systemtime()
    {
        SYSTEMTIME st;
        st.wYear = 2013;
        st.wMonth = 8;
        st.wDayOfWeek = 5;
        st.wDay = 2;
        st.wHour = 16;
        st.wMinute = 43;
        st.wSecond = 56;
        st.wMilliseconds = 0;

        return st;
    }

    int year() const
    {
        return 2013;
    }

    int month() const
    {
        return 8;
    }

    int day() const
    {
        return 2;
    }

    int hour() const
    {
        return 16;
    }

    int minute() const
    {
        return 43;
    }

    int second() const
    {
        return 56;
    }

    int millisecond() const
    {
        return 0;
    }
};

template<typename UtcTime>
void check_date_matches(const datetime_t& d, const UtcTime& time)
{
    BOOST_CHECK_EQUAL(d.year(), time.year());
    BOOST_CHECK_EQUAL(d.month(), time.month());
    BOOST_CHECK_EQUAL(d.day(), time.day());
    BOOST_CHECK_EQUAL(d.hour(), time.hour());
    BOOST_CHECK_EQUAL(d.minute(), time.minute());
    BOOST_CHECK_EQUAL(d.second(), time.second());
    BOOST_CHECK_EQUAL(d.millisecond(), time.millisecond());
}

template<typename UtcTime>
class unix_time_fixture
{
public:
    time_t get_time()
    {
        return m_time.unix_time();
    }

    /**
     * Tests the given datetime_t as though it were a local time.
     */
    void check_date_matches_local(const datetime_t& d)
    {
        time_t unix_time = get_time();

#pragma warning(push)
#pragma warning(disable:4996)
        struct tm* timeinfo = localtime(&unix_time);
#pragma warning(pop)

        BOOST_CHECK_EQUAL(d.year(), timeinfo->tm_year + 1900);
        BOOST_CHECK_EQUAL(d.month(), timeinfo->tm_mon + 1);
        BOOST_CHECK_EQUAL(d.day(), timeinfo->tm_mday);
        BOOST_CHECK_EQUAL(d.hour(), timeinfo->tm_hour);
        BOOST_CHECK_EQUAL(d.minute(), timeinfo->tm_min);
        BOOST_CHECK_EQUAL(d.second(), timeinfo->tm_sec);
        BOOST_CHECK_EQUAL(d.millisecond(), 0);
    }

    void check_foreign_matches_utc(time_t tin)
    {
        BOOST_CHECK_EQUAL(tin, get_time());
    }

    void assign_to_datetime(
        time_t t, datetime_t& d, datetime_t::utc_convert_mode::type mode)
    {
        d.from_unixtime(t, mode);
    }

    void assign_to_datetime(time_t t, datetime_t& d)
    {
        d.from_unixtime(t);
    }

    time_t to_foreign(const datetime_t& d)
    {
        time_t t;
        BOOST_CHECK(d.to_unixtime(&t));
        return t;
    }

    time_t to_foreign(
        const datetime_t& d, datetime_t::utc_convert_mode::type mode)
    {
        time_t t;
        BOOST_CHECK(d.to_unixtime(&t, mode));
        return t;
    }

    UtcTime utc_test_time()
    {
        return m_time;
    }

private:
    UtcTime m_time;
};

template<typename UtcTime>
class filetime_fixture
{
public:

    FILETIME get_time()
    {
        return m_time.utc_filetime();
    }

    FILETIME get_local_time()
    {
        // Not using FileTimeToLocalFileTime because it doesn't handle DST
        SYSTEMTIME st = m_time.utc_systemtime();
        SYSTEMTIME lst;
        BOOST_REQUIRE(::SystemTimeToTzSpecificLocalTime(NULL, &st, &lst));

        FILETIME lft;
        BOOST_REQUIRE(::SystemTimeToFileTime(&lst, &lft));

        return lft;
    }

    /**
     * Tests the given datetime_t as though it were a local time.
     */
    void check_date_matches_local(const datetime_t& d)
    {
        FILETIME local_filetime = get_local_time();
        datetime_filetime_match(d, local_filetime);
    }

    void check_foreign_matches_utc(const FILETIME& ftin)
    {
        FILETIME utc_filetime = get_time();
        filetime_match(ftin, utc_filetime);
    }

    void check_foreign_matches_local(const FILETIME& ftin)
    {
        FILETIME utc_filetime = get_local_time();
        filetime_match(ftin, utc_filetime);
    }

    void assign_to_datetime(
        const FILETIME& ft, datetime_t& d,
        datetime_t::utc_convert_mode::type mode)
    {
        d.from_filetime(ft, mode);
    }

    void assign_to_datetime(const FILETIME& ft, datetime_t& d)
    {
        d.from_filetime(ft);
    }

    FILETIME to_foreign(const datetime_t& d)
    {
        FILETIME ft;
        BOOST_CHECK(d.to_filetime(&ft));
        return ft;
    }
/*
    FILETIME to_foreign(
        const datetime_t& d, datetime_t::utc_convert_mode::type mode)
    {
        FILETIME ft;
        BOOST_ERROR("to_filetime() has no conversion argument");
        //BOOST_CHECK(d.to_filetime(&ft, mode));
        return ft;
    }
    */

    UtcTime utc_test_time()
    {
        return m_time;
    }

private:

    UtcTime m_time;
};

template<typename UtcTime>
class systemtime_fixture
{
public:
    SYSTEMTIME get_time()
    {
        return m_time.utc_systemtime();
    }

    SYSTEMTIME get_local_time()
    {
        SYSTEMTIME st = m_time.utc_systemtime();
        SYSTEMTIME lst;
        BOOST_REQUIRE(::SystemTimeToTzSpecificLocalTime(NULL, &st, &lst));

        return lst;
    }

    /**
     * Tests the given datetime_t as though it were a local time.
     */
    void check_date_matches_local(const datetime_t& d)
    {
        SYSTEMTIME st = get_local_time();
        datetime_systemtime_match(d, st);
    }

    void check_foreign_matches_utc(const SYSTEMTIME& stin)
    {
        SYSTEMTIME st = get_time();
        systemtime_match(stin, st);
    }

    void check_foreign_matches_local(const SYSTEMTIME& stin)
    {
        SYSTEMTIME st = get_local_time();
        systemtime_match(stin, st);
    }

    void assign_to_datetime(
        const SYSTEMTIME& st, datetime_t& d,
        datetime_t::utc_convert_mode::type mode)
    {
        d.from_systemtime(st, mode);
    }

    void assign_to_datetime(const SYSTEMTIME& st, datetime_t& d)
    {
        d.from_systemtime(st);
    }

    SYSTEMTIME to_foreign(const datetime_t& d)
    {
        SYSTEMTIME st;
        BOOST_CHECK(d.to_systemtime(&st));
        return st;
    }
/*
    SYSTEMTIME to_foreign(
        const datetime_t& d, datetime_t::utc_convert_mode::type mode)
    {
        SYSTEMTIME st;
        BOOST_ERROR("to_systemtime() has no conversion argument");
        //BOOST_CHECK(d.to_systemtime(&st, mode));
        return st;
    }
    */

    UtcTime utc_test_time()
    {
        return m_time;
    }

private:
    UtcTime m_time;
};

typedef boost::mpl::vector<
    unix_time_fixture<early_time>, unix_time_fixture<late_time>,
    filetime_fixture<early_time>, filetime_fixture<late_time>,
    systemtime_fixture<early_time>, systemtime_fixture<late_time>>
    date_conversion_fixtures;

// Unix time_t cannot be in local timezone whereas the others can.  These
// fixtures have the additional get_local_time() method to tests with a local
// source date
typedef boost::mpl::vector<
    filetime_fixture<early_time>, filetime_fixture<late_time>,
    systemtime_fixture<early_time>, systemtime_fixture<late_time>>
    localable_date_conversion_fixtures;

BOOST_AUTO_TEST_CASE_TEMPLATE(
    foreign_format_constructor_default_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time());

    // TODO: decide what we actually want the defaults to be
    f.check_date_matches_local(d);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    foreign_format_constructor_no_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    foreign_format_constructor_local_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::utc_to_local);
    f.check_date_matches_local(d);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    foreign_format_constructor_utc_conversion, F,
    localable_date_conversion_fixtures )
{
    F f;
    datetime_t d(
        f.get_local_time(), datetime_t::utc_convert_mode::local_to_utc);
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    explicit_conversion_to_local, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);
    check_date_matches(d, f.utc_test_time());

    datetime_t d2 = d.utc_to_local();
    f.check_date_matches_local(d2);
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    explicit_conversion_to_utc, F, localable_date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_local_time(), datetime_t::utc_convert_mode::none);
    f.check_date_matches_local(d);

    datetime_t d2 = d.local_to_utc();
    check_date_matches(d2, f.utc_test_time());
    f.check_date_matches_local(d);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    from_foreign_default_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d;
    f.assign_to_datetime(f.get_time(), d);

    // TODO: decide what we actually want the defaults to be
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    from_foreign_no_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d;
    f.assign_to_datetime(f.get_time(), d, datetime_t::utc_convert_mode::none);
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    from_foreign_convert_to_local, F, date_conversion_fixtures )
{
    F f;
    datetime_t d;
    f.assign_to_datetime(
        f.get_time(), d, datetime_t::utc_convert_mode::utc_to_local);
    f.check_date_matches_local(d);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    from_foreign_convert_to_utc, F, localable_date_conversion_fixtures )
{
    F f;
    datetime_t d;
    f.assign_to_datetime(
        f.get_local_time(), d, datetime_t::utc_convert_mode::local_to_utc);
    check_date_matches(d, f.utc_test_time());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    to_foreign_default_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);

    // TODO: decide what we actually want the defaults to be
    f.check_foreign_matches_utc(f.to_foreign(d));
}

BOOST_AUTO_TEST_CASE( to_foreign_no_conversion )
{
    unix_time_fixture<early_time> f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);
    f.check_foreign_matches_utc(
        f.to_foreign(d, datetime_t::utc_convert_mode::none));
}

/*
BOOST_AUTO_TEST_CASE_TEMPLATE(
    to_foreign_no_conversion, F, date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);
    f.check_foreign_matches_utc(
        f.to_foreign(d, datetime_t::utc_convert_mode::none));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    to_foreign_convert_to_local, F, localable_date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_time(), datetime_t::utc_convert_mode::none);
    f.check_foreign_matches_local(
        f.to_foreign(d, datetime_t::utc_convert_mode::utc_to_local));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    to_foreign_convert_to_utc, F, localable_date_conversion_fixtures )
{
    F f;
    datetime_t d(f.get_local_time(), datetime_t::utc_convert_mode::none);
    f.check_foreign_matches_utc(
        f.to_foreign(d, datetime_t::utc_convert_mode::local_to_utc));
}
*/


BOOST_AUTO_TEST_SUITE_END()

