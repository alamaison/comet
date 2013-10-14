/*
 * Copyright 2013 Alexander Lamaison <alexander.lamaison@gmail.com>
 *
 * This material is provided "as is", with absolutely no warranty
 * expressed or implied. Any use is at your own risk. Permission to
 * use or copy this software for any purpose is hereby granted without
 * fee, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 *
 * This header is part of Comet version 2.
 * https://github.com/alamaison/comet
 */
#include <boost/test/unit_test.hpp>

#include <comet/stream.h> // test subject

#include <comet/ptr.h> // com_ptr

#include <exception>
#include <sstream> // istringstream, ostringstream, stringstream
#include <string>
#include <vector>

using comet::adapt_stream;
using comet::com_ptr;

using std::exception;
using std::istream;
using std::istringstream;
using std::ostream;
using std::ostringstream;
using std::string;
using std::stringstream;
using std::vector;

BOOST_AUTO_TEST_SUITE( stream_adapter_tests )

BOOST_AUTO_TEST_CASE( read_from_stl_istream_empty )
{
    istringstream stl_stream;

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(10);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Read(&buffer[0], buffer.size(), &count), S_FALSE);
    BOOST_CHECK_EQUAL(count, 0U);
    BOOST_CHECK(stl_stream.str().empty());
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_exact )
{
    istringstream stl_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(stl_stream.str().size());
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Read(&buffer[0], buffer.size(), &count), S_OK);
    BOOST_CHECK_EQUAL(count, buffer.size());

    string expected = "gobbeldy gook";

    BOOST_CHECK_EQUAL(stl_stream.str(), expected);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        buffer.begin(), buffer.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_under )
{
    istringstream stl_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(stl_stream.str().size() - 3);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Read(&buffer[0], buffer.size(), &count), S_OK);
    BOOST_CHECK_EQUAL(count, buffer.size());

    string expected = "gobbeldy gook";

    BOOST_CHECK_EQUAL(stl_stream.str(), expected);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        buffer.begin(), buffer.end(), expected.begin(), expected.end() - 3);
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_over )
{
    istringstream stl_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(stl_stream.str().size() + 3);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Read(&buffer[0], buffer.size(), &count), S_FALSE);
    BOOST_CHECK_EQUAL(count, buffer.size() - 3);

    string expected = "gobbeldy gook";

    BOOST_CHECK_EQUAL(stl_stream.str(), expected);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        buffer.begin(), buffer.begin() + count,
        expected.begin(), expected.end());
}

/**
 * Throws, rather than returning next character, when reaching given
 * position in given string.
 */
class max_size_throwing_underflow_stringstreambuf : public std::streambuf
{
public:
    max_size_throwing_underflow_stringstreambuf(
        size_t throwing_size, string str) :
      m_throwing_size(throwing_size), m_pos(0), m_str(str) {}

    string str()
    {
        return m_str;
    }

private:

    virtual int_type underflow()
    {
        if (m_pos >= m_throwing_size)
        {
            throw std::exception("Surprise");
        }
        else if (m_pos >= m_str.size())
        {
            return traits_type::eof();
        }
        else
        {
            return traits_type::to_int_type(m_str[m_pos]);
        }
    }

    virtual int_type uflow()
    {
        // MS implementation of uflow doesn't like NULL gptr - tries to
        // dereference it
        int_type result = underflow();
        ++m_pos;
        return result;
    }

    size_t m_throwing_size;
    size_t m_pos;
    string m_str;

};

BOOST_AUTO_TEST_CASE( read_from_stl_istream_buffer_throws )
{
    max_size_throwing_underflow_stringstreambuf buf(3, "gobbeldy gook");
    istream stl_stream(&buf);

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(buf.str().size());
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Read(&buffer[0], buffer.size(), &count), E_FAIL);
    BOOST_CHECK_EQUAL(count, 3U);

    string expected = "gob";

    BOOST_CHECK_EQUAL_COLLECTIONS(
        buffer.begin(), buffer.begin() + count,
        expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE( write_to_stl_istream_fails )
{
    istringstream stl_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(stl_stream.str().size() + 3);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(
        s->Write(&buffer[0], buffer.size(), &count), STG_E_ACCESSDENIED);
    BOOST_CHECK_EQUAL(count, 0U);
    BOOST_CHECK_EQUAL(stl_stream.str(), "gobbeldy gook");
}

// IStream write is always exact unless there is an error part way through
// writing, in which case it may return short
BOOST_AUTO_TEST_CASE( write_to_stl_ostream )
{
    ostringstream stl_stream;

    com_ptr<IStream> s = adapt_stream(stl_stream);

    string data("gobbeldy gook");
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write(&data[0], data.size(), &count), S_OK);
    BOOST_CHECK_EQUAL(count, data.size());

    BOOST_CHECK_EQUAL(stl_stream.str(), "gobbeldy gook");
}

BOOST_AUTO_TEST_CASE( read_from_stl_ostream_fails )
{
    ostringstream stl_stream;

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(10);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(
        s->Read(&buffer[0], buffer.size(), &count), STG_E_ACCESSDENIED);
    BOOST_CHECK_EQUAL(count, 0U);
}

/**
 * Model an output buffer that might cause writing to end prematurely.
 * This allows us to test that we report the status of these writes
 * correctly.
 */
class max_size_stringstreambuf : public std::streambuf
{
public:
    max_size_stringstreambuf(size_t max_size) : m_max_size(max_size) {}

    string str()
    {
        return m_str;
    }

private:

    virtual int_type overflow(int_type c)
    {
        if (c != traits_type::eof())
        {

            if (m_str.size() < m_max_size)
            {
                m_str += traits_type::to_char_type(c);
            }
            else
            {
                return traits_type::eof();
            }
        }

        return c;
    }

    size_t m_max_size;
    string m_str;
   
};

BOOST_AUTO_TEST_CASE( write_to_stl_ostream_eof )
{
    max_size_stringstreambuf buf(3);
    ostream stl_stream(&buf);

    com_ptr<IStream> s = adapt_stream(stl_stream);

    string data("gobbeldy gook");
    ULONG count = 99;
    BOOST_CHECK_EQUAL(
        s->Write(&data[0], data.size(), &count), STG_E_MEDIUMFULL);
    BOOST_CHECK_EQUAL(count, 3U);

    BOOST_CHECK_EQUAL(buf.str(), "gob");
}

class max_size_throwing_stringstreambuf : public std::streambuf
{
public:
    max_size_throwing_stringstreambuf(size_t throwing_size) :
      m_throwing_size(throwing_size) {}

    string str()
    {
        return m_str;
    }

private:

    virtual int_type overflow(int_type c)
    {
        if (c != traits_type::eof())
        {

            if (m_str.size() < m_throwing_size)
            {
                m_str += traits_type::to_char_type(c);
            }
            else
            {
                throw std::exception("No-write surprise");
            }
        }

        return c;
    }

    size_t m_throwing_size;
    string m_str;

};

BOOST_AUTO_TEST_CASE( write_to_stl_ostream_error )
{
    max_size_throwing_stringstreambuf buf(3);
    ostream stl_stream(&buf);

    com_ptr<IStream> s = adapt_stream(stl_stream);

    string data("gobbeldy gook");
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write(&data[0], data.size(), &count), E_FAIL);
    BOOST_CHECK_EQUAL(count, 3U);

    BOOST_CHECK_EQUAL(buf.str(), "gob");
}

BOOST_AUTO_TEST_SUITE_END()