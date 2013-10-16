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

#include <cstdio> // tmpnam, remove
#include <exception>
#include <iterator> // istreambuf_iterator
#include <fstream>
#include <sstream> // istringstream, ostringstream, stringstream
#include <string>
#include <vector>

using comet::adapt_stream;
using comet::com_ptr;

using std::exception;
using std::fstream;
using std::ifstream;
using std::istream;
using std::istreambuf_iterator;
using std::ofstream;
using std::ostream;
using std::string;
using std::vector;

namespace {

    template<typename Stream>
    void do_stream_seek_test(
        Stream& stl_stream, DWORD origin, int amount,
        HRESULT expected_success, unsigned long long expected_new_pos)
    {
        com_ptr<IStream> s = adapt_stream(stl_stream);

        LARGE_INTEGER move = {amount};
        ULARGE_INTEGER new_pos = {42U}; // set to random first

        BOOST_CHECK_EQUAL(s->Seek(move, origin, &new_pos), expected_success);
        BOOST_CHECK_EQUAL(new_pos.QuadPart, expected_new_pos);
    }

    class file_stream_fixture
    {
    public:

#pragma warning(push)
#pragma warning(disable:4996)
        file_stream_fixture() : m_temp_name(std::tmpnam(NULL)) {}
#pragma warning(pop)

        ~file_stream_fixture()
        {
            std::remove(m_temp_name.c_str());
        }
        
        typedef ifstream test_input_stream;
        typedef ofstream test_output_stream;
        typedef fstream test_io_stream;

        test_input_stream input_stream(const string& contents=string())
        {
            create_with_contents(contents);
            return ifstream(m_temp_name.c_str());
        }

        test_output_stream output_stream(const string& contents=string())
        {
            create_with_contents(contents);
            // specify in to prevent truncating file
            return ofstream(m_temp_name.c_str(), std::ios_base::in);
        }

        test_io_stream io_stream(const string& contents=string())
        {
            create_with_contents(contents);
            return fstream(m_temp_name.c_str());
        }

        void check_stream_contains(const string& contents)
        {
            ifstream s(m_temp_name.c_str());
            BOOST_CHECK_EQUAL_COLLECTIONS(
                istreambuf_iterator<char>(s), istreambuf_iterator<char>(),
                contents.begin(), contents.end());
        }

        void do_empty_stream_seek_test(
            DWORD origin, int amount, HRESULT expected_success,
            unsigned long long expected_new_pos=0U)
        {
            test_io_stream stl_stream = io_stream();
            do_stream_seek_test(
                stl_stream, origin, amount, expected_success,
                expected_new_pos);
        }

    private:

        void create_with_contents(const string& contents)
        {
            ofstream s(m_temp_name.c_str());
            if (!contents.empty())
            {
                s.write(&contents[0], contents.size());
            }
        }

        string m_temp_name;
    };

    template<typename Collection>
    void check_read_results_in(
        com_ptr<IStream> stream, size_t buffer_size,
        const Collection& expected_valid_bytes, HRESULT expected_error_code)
    {
        vector<char> buffer(buffer_size);

        // i.e. not already the expected count before call
        ULONG count = ((ULONG)expected_valid_bytes.size()) + 99;

        HRESULT hr = stream->Read(&buffer[0], buffer.size(), &count);

        BOOST_CHECK_EQUAL_COLLECTIONS(
            buffer.begin(), buffer.begin() + count,
            expected_valid_bytes.begin(), expected_valid_bytes.end());

        BOOST_CHECK_EQUAL(hr, expected_error_code);
    }

    template<typename Collection>
    void check_read_to_end(
        com_ptr<IStream> stream, const Collection& expected_valid_bytes)
    {
        // buffer one bigger than expected should trigger S_FALSE if working
        // correctly
        check_read_results_in(
            stream, expected_valid_bytes.size() + 1, expected_valid_bytes,
            S_FALSE);
    }
}

BOOST_FIXTURE_TEST_SUITE( stream_adapter_tests, file_stream_fixture )

BOOST_AUTO_TEST_CASE( read_from_stl_istream_empty )
{
    test_input_stream stl_stream = input_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_results_in(s, 10, string(), S_FALSE);
    check_stream_contains(string());
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_exact )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_results_in(s, 13, string("gobbeldy gook"), S_OK);

    check_stream_contains("gobbeldy gook");
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_under )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_results_in(s, 10, string("gobbeldy g"), S_OK);
    check_stream_contains("gobbeldy gook");
}

BOOST_AUTO_TEST_CASE( read_from_stl_istream_over )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_results_in(s, 16, string("gobbeldy gook"), S_FALSE);
    check_stream_contains("gobbeldy gook");
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

    check_read_results_in(s, 13, string("gob"), E_FAIL);
}

BOOST_AUTO_TEST_CASE( write_to_stl_istream_fails )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    vector<char> buffer(16);
    ULONG count = 99;
    BOOST_CHECK_EQUAL(
        s->Write(&buffer[0], buffer.size(), &count), STG_E_ACCESSDENIED);
    BOOST_CHECK_EQUAL(count, 0U);

    check_stream_contains("gobbeldy gook");
}

// IStream write is always exact unless there is an error part way through
// writing, in which case it may return short
BOOST_AUTO_TEST_CASE( write_to_stl_ostream )
{
    test_output_stream stl_stream = output_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    string data("gobbeldy gook");
    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write(&data[0], data.size(), &count), S_OK);
    BOOST_CHECK_EQUAL(count, data.size());

    stl_stream.flush();
    check_stream_contains("gobbeldy gook");
}

BOOST_AUTO_TEST_CASE( read_from_stl_ostream_fails )
{
    test_output_stream stl_stream = output_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_results_in(s, 10, vector<char>(), STG_E_ACCESSDENIED);

    check_stream_contains(string());
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

BOOST_AUTO_TEST_CASE( read_then_write_stl_stream )
{
    string data = "gobbeldy gook";
    test_io_stream stl_stream = io_stream(data);

    com_ptr<IStream> s = adapt_stream(stl_stream);

    unsigned int hop = 4;
    check_read_results_in(s, hop, string("gobb"), S_OK);


    check_stream_contains(data);

    ULONG count;
    BOOST_CHECK_EQUAL(s->Write("bob", 3, &count), S_OK);
    BOOST_CHECK_EQUAL(count, 3U);

    stl_stream.flush();
    check_stream_contains("gobbboby gook");
}

BOOST_AUTO_TEST_CASE( write_then_read_stl_stream )
{
    string data = "gobbeldy gook";
    test_io_stream stl_stream = io_stream(data);

    com_ptr<IStream> s = adapt_stream(stl_stream);

    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write("bob", 3, &count), S_OK);
    BOOST_CHECK_EQUAL(count, 3U);

    check_read_results_in(s, data.size(), string("beldy gook"), S_FALSE);

    check_stream_contains("bobbeldy gook");
}

BOOST_AUTO_TEST_CASE( seek_empty_relative )
{
    do_empty_stream_seek_test(STREAM_SEEK_CUR, 0, S_OK);
}

BOOST_AUTO_TEST_CASE( seek_empty_relative_undershoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_CUR, -1, E_FAIL);
}

BOOST_AUTO_TEST_CASE( seek_empty_relative_overshoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_CUR, 1, S_OK, 1);
}

BOOST_AUTO_TEST_CASE( seek_empty_absolute )
{
    do_empty_stream_seek_test(STREAM_SEEK_SET, 0, S_OK);
}

BOOST_AUTO_TEST_CASE( seek_empty_absolute_undershoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_SET, -1, E_FAIL);
}

BOOST_AUTO_TEST_CASE( seek_empty_absolute_overshoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_SET, 1, S_OK, 1);
}

BOOST_AUTO_TEST_CASE( seek_empty_end )
{
    do_empty_stream_seek_test(STREAM_SEEK_END, 0, S_OK);
}

BOOST_AUTO_TEST_CASE( seek_empty_end_undershoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_END, -1, E_FAIL);
}

BOOST_AUTO_TEST_CASE( seek_empty_end_overshoot )
{
    do_empty_stream_seek_test(STREAM_SEEK_END, 1, S_OK, 1);
}

namespace {

    // We seek twice to make sure the seek is relative not absolute.  As single
    // seek relative to 0 is indistinguishable from an absolute seek
    // Assumes stream is at origin
    void two_hop_relative_seek(
        com_ptr<IStream> s, int first_hop, int second_hop,
        HRESULT expected_final_error_code, unsigned int expected_final_position)
    {
        LARGE_INTEGER move;
        move.QuadPart = first_hop;

        ULARGE_INTEGER new_pos = {42U}; // set to random first

        BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_CUR, &new_pos), S_OK);
        BOOST_CHECK_EQUAL(new_pos.QuadPart, first_hop);

        move.QuadPart = second_hop;

        BOOST_CHECK_EQUAL(
            s->Seek(move, STREAM_SEEK_CUR, &new_pos),
            expected_final_error_code);

        BOOST_CHECK_EQUAL(new_pos.QuadPart, expected_final_position);
    }

}

BOOST_AUTO_TEST_CASE( seek_relative )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 4, S_OK, 7U);

    check_read_to_end(s, string("y gook"));
}

BOOST_AUTO_TEST_CASE( seek_relative_undershoot )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, -4, E_FAIL, 3U);

    check_read_to_end(s, string("beldy gook"));
}

BOOST_AUTO_TEST_CASE( seek_relative_overshoot )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 40, S_OK, 43U);

    check_read_results_in(s, 1, string(), S_FALSE);
}

BOOST_AUTO_TEST_CASE( seek_absolute )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move = {6};
    ULARGE_INTEGER new_pos = {42U}; // set to random first

    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_absolute_end )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move;
    move.QuadPart = -2;
    ULARGE_INTEGER new_pos = {42U}; // set to random first

    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_END, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 11U);

    check_read_to_end(s, string("ok"));
}

BOOST_AUTO_TEST_CASE( seek_relative_out )
{
    test_output_stream stl_stream = output_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 4, S_OK, 7U);

    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write("zx", 2, &count), S_OK);
    BOOST_CHECK_EQUAL(count, 2U);

    stl_stream.flush();
    check_stream_contains("gobbeldzxgook");
}

BOOST_AUTO_TEST_CASE( seek_relative_out_overshoot )
{
    test_output_stream stl_stream = output_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 14, S_OK, 17U);

    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write("zx", 2, &count), S_OK);
    BOOST_CHECK_EQUAL(count, 2U);

    stl_stream.flush();
    check_stream_contains(string("gobbeldy gook\0\0\0\0zx", 19));
}

BOOST_AUTO_TEST_CASE( seek_relative_inout )
{
    test_io_stream stl_stream = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 4, S_OK, 7U);

    ULONG count = 99;
    BOOST_CHECK_EQUAL(s->Write("zx", 2, &count), S_OK);
    BOOST_CHECK_EQUAL(count, 2U);

    stl_stream.flush();
    check_stream_contains("gobbeldzxgook");

    check_read_to_end(s, string("gook"));
}

BOOST_AUTO_TEST_CASE( seek_no_newpos )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move = {6};

    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, NULL), S_OK);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_back_from_eof_caused_by_read )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string("gobbeldy gook"));

    LARGE_INTEGER move = {6};

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_back_from_absolute_seek_to_end )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string("gobbeldy gook"));

    LARGE_INTEGER move = {13};

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 13U);

    move.QuadPart = 6;

    new_pos.QuadPart = 42U;
    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_back_from_seek_straight_to_end )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move = {0};

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_END, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 13U);

    move.QuadPart = 6;

    new_pos.QuadPart = 42U;
    BOOST_CHECK_EQUAL(s->Seek(move, STREAM_SEEK_SET, &new_pos), S_OK);
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_SUITE_END()