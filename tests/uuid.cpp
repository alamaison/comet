#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/bstr.h>
#include <comet/uuid.h>

using comet::bstr_t;
using comet::uuid_t;

BOOST_AUTO_TEST_SUITE( uuid_tests )

BOOST_AUTO_TEST_CASE( streaming )
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

BOOST_AUTO_TEST_CASE( from_bstr_t )
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

BOOST_AUTO_TEST_SUITE_END()