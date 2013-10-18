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

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp> // current_path
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>

#include <comet/stream.h> // test subject

#include <comet/error.h> // com_error_from_interface
#include <comet/ptr.h> // com_ptr

#include <cstdio> // tmpnam, remove
#include <exception>
#include <iterator> // istreambuf_iterator
#include <fstream>
#include <memory> // auto_ptr
#include <sstream> // istringstream, ostringstream, stringstream
#include <string>
#include <vector>

using boost::filesystem::path;
using boost::filesystem::current_path;
using boost::shared_ptr;
using boost::test_tools::predicate_result;

using comet::adapt_stream;
using comet::adapt_stream_pointer;
using comet::com_ptr;
using comet::com_error_from_interface;

using std::auto_ptr;
using std::exception;
using std::fstream;
using std::ifstream;
using std::istream;
using std::istreambuf_iterator;
using std::istringstream;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::string;
using std::vector;
using std::wstring;

namespace {

    template<typename Itf>
    predicate_result has_hresult(
        HRESULT hr, com_ptr<Itf> iface, HRESULT expected_hr)
    {
        if (hr == expected_hr)
        {
            return true;
        }
        else
        {
            predicate_result result = false;
            result.message() <<
                "\nWrong error code: " <<
                std::hex << hr << " != " << expected_hr <<
                ": " << com_error_from_interface(iface, hr).what();
            return result;
        }
    }

    template<typename Itf>
    predicate_result is_s_ok(HRESULT hr, com_ptr<Itf> iface)
    {
        if (hr == S_OK)
        {
            return true;
        }
        else
        {
            predicate_result result = false;
            result.message() <<
                "\nError code not S_OK: (" << std::hex << hr <<
                ") " << com_error_from_interface(iface, hr).what();
            return result;
        }
    }

    template<typename Stream>
    void do_stream_seek_test(
        Stream& stl_stream, DWORD origin, int amount,
        HRESULT expected_success, unsigned long long expected_new_pos)
    {
        com_ptr<IStream> s = adapt_stream(stl_stream);

        LARGE_INTEGER move = {amount};
        ULARGE_INTEGER new_pos = {42U}; // set to random first

        BOOST_CHECK(
            has_hresult(s->Seek(move, origin, &new_pos), s, expected_success));
        BOOST_CHECK_EQUAL(new_pos.QuadPart, expected_new_pos);
    }

    path temp_path()
    {
#pragma warning(push)
#pragma warning(disable:4996)
        string name = std::tmpnam(NULL);
#pragma warning(pop)

        if (!name.empty() && name[0] == '\\')
        {
            return current_path() / name;
        }
        else
        {
            return name;
        }
    }

    class file_stream_fixture
    {
    public:

        file_stream_fixture() : m_temp_name(temp_path().string()) {}

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

        auto_ptr<test_io_stream> io_stream_pointer(
            const string& contents=string())
        {
            create_with_contents(contents);
            return auto_ptr<test_io_stream>(new fstream(m_temp_name.c_str()));
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
            BOOST_REQUIRE(s);
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

        // makes static cast safe
        BOOST_REQUIRE(buffer.size() <= (std::numeric_limits<ULONG>::max)());

        HRESULT hr = stream->Read(
            &buffer[0], static_cast<ULONG>(buffer.size()), &count);

        BOOST_CHECK_EQUAL_COLLECTIONS(
            buffer.begin(), buffer.begin() + count,
            expected_valid_bytes.begin(), expected_valid_bytes.end());

        BOOST_CHECK(has_hresult(hr, stream, expected_error_code));
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
    BOOST_CHECK(
        has_hresult(
            s->Write(&buffer[0], static_cast<ULONG>(buffer.size()), &count), s,
            STG_E_ACCESSDENIED));
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
    BOOST_CHECK(
        is_s_ok(
            s->Write(&data[0], static_cast<ULONG>(data.size()), &count), s));
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
    BOOST_CHECK(
        has_hresult(
            s->Write(&data[0], static_cast<ULONG>(data.size()), &count), s,
            STG_E_MEDIUMFULL));
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
    BOOST_CHECK(
        has_hresult(
            s->Write(&data[0], static_cast<ULONG>(data.size()), &count), s,
            E_FAIL));

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
    BOOST_CHECK(is_s_ok(s->Write("bob", 3, &count), s));
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
    BOOST_CHECK(is_s_ok(s->Write("bob", 3, &count), s));
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

        BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_CUR, &new_pos), s));
        BOOST_CHECK_EQUAL(new_pos.QuadPart, first_hop);

        move.QuadPart = second_hop;

        BOOST_CHECK(
            has_hresult(
                s->Seek(move, STREAM_SEEK_CUR, &new_pos), s,
                expected_final_error_code));

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

    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_absolute_undershoot )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move;
    move.QuadPart = -1;

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK(
        has_hresult(s->Seek(move, STREAM_SEEK_SET, &new_pos), s, E_FAIL));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 0U);

    check_read_to_end(s, string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( seek_absolute_end )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move;
    move.QuadPart = -2;
    ULARGE_INTEGER new_pos = {42U}; // set to random first

    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_END, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 11U);

    check_read_to_end(s, string("ok"));
}

BOOST_AUTO_TEST_CASE( seek_relative_out )
{
    test_output_stream stl_stream = output_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    two_hop_relative_seek(s, 3, 4, S_OK, 7U);

    ULONG count = 99;
    BOOST_CHECK(is_s_ok(s->Write("zx", 2, &count), s));
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
    BOOST_CHECK(is_s_ok(s->Write("zx", 2, &count), s));
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
    BOOST_CHECK(is_s_ok(s->Write("zx", 2, &count), s));
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

    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, NULL), s));

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_back_from_eof_caused_by_read )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string("gobbeldy gook"));

    LARGE_INTEGER move = {6};

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
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
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 13U);

    move.QuadPart = 6;

    new_pos.QuadPart = 42U;
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( seek_back_from_seek_straight_to_end )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move = {0};

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_END, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 13U);

    move.QuadPart = 6;

    new_pos.QuadPart = 42U;
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 6U);

    check_read_to_end(s, string("dy gook"));
}

BOOST_AUTO_TEST_CASE( set_size_ostream )
{
    test_output_stream stl_stream = output_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    // This seek combined with the later read-to-end check that the seek
    // position is not modified
    LARGE_INTEGER move = {1};
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, NULL), s));

    ULARGE_INTEGER new_size = {17};

    BOOST_CHECK(is_s_ok(s->SetSize(new_size), s));

    stl_stream.flush();
    check_stream_contains(string("gobbeldy gook\0\0\0\0", 17));
}

BOOST_AUTO_TEST_CASE( set_size_iostream )
{
    test_io_stream stl_stream = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    // This seek combined with the later read-to-end check that the seek
    // position is not modified
    LARGE_INTEGER move = {1};
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, NULL), s));

    ULARGE_INTEGER new_size = {17};

    BOOST_CHECK(is_s_ok(s->SetSize(new_size), s));

    check_read_to_end(s, string("obbeldy gook\0\0\0\0", 16));

    stl_stream.flush();
    check_stream_contains(string("gobbeldy gook\0\0\0\0", 17));
}

BOOST_AUTO_TEST_CASE( set_size_istream )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    ULARGE_INTEGER new_size = {17};

    BOOST_CHECK(has_hresult(s->SetSize(new_size), s, STG_E_ACCESSDENIED));

    check_read_to_end(s, string("gobbeldy gook"));

    check_stream_contains(string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( copy_istream_exact )
{
    test_input_stream source = input_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {13};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 13U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 13U);

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( copy_istream_under )
{
    test_input_stream source = input_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {12};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 12U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 12U);

    // must advance the seek pointer so have just one byte more to read
    check_read_to_end(s, string("k"));

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy goo"));
}

BOOST_AUTO_TEST_CASE( copy_istream_over )
{
    test_input_stream source = input_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {14};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 13U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 13U);

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( copy_istream_no_count_vars )
{
    test_input_stream source = input_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {14};
    BOOST_CHECK(is_s_ok(s->CopyTo(d.in(), amount, NULL, NULL), s));

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( copy_istream_large_exact_multiple )
{
    // tests an exact multiple of goes round the copy chunk
    unsigned int size = 2048;
    test_input_stream source = input_stream(string(size, 'g'));
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {size + 1};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, size);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, size);

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string(size, 'g'));
    BOOST_CHECK_EQUAL(dest.str(), string(size, 'g'));
}

BOOST_AUTO_TEST_CASE( copy_istream_large_non_exact_multiple )
{
    unsigned int size = 2049;
    test_input_stream source = input_stream(string(size, 'g'));
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {size + 1};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, size);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, size);

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string(size, 'g'));
    BOOST_CHECK_EQUAL(dest.str(), string(size, 'g'));
}

BOOST_AUTO_TEST_CASE( copy_iostream_exact )
{
    test_io_stream source = io_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {13};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 13U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 13U);

    // must advance the seek pointer so have no more to read
    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( copy_ostream )
{
    test_output_stream source = output_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    ULARGE_INTEGER amount = {13};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        has_hresult(
            s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s,
            STG_E_ACCESSDENIED));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 0U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 0U);

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string());
}

BOOST_AUTO_TEST_CASE( commit_inout )
{
    test_io_stream stl_stream = io_stream("abcd");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    ULONG count = 99;
    BOOST_CHECK(is_s_ok(s->Write("zx", 2, &count), s));
    BOOST_CHECK_EQUAL(count, 2U);

    check_stream_contains("abcd");
    BOOST_CHECK(is_s_ok(s->Commit(STGC_DEFAULT), s));
    check_stream_contains("zxcd");
}

BOOST_AUTO_TEST_CASE( commit_out )
{
    test_output_stream stl_stream = output_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    ULONG count = 99;
    BOOST_CHECK(is_s_ok(s->Write("zx", 2, &count), s));
    BOOST_CHECK_EQUAL(count, 2U);

    check_stream_contains("");
    BOOST_CHECK(is_s_ok(s->Commit(STGC_DEFAULT), s));
    check_stream_contains("zx");
}

BOOST_AUTO_TEST_CASE( commit_in )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_stream_contains("gobbeldy gook");
    BOOST_CHECK(has_hresult(s->Commit(STGC_DEFAULT), s, STG_E_ACCESSDENIED));
    check_stream_contains("gobbeldy gook");
}

// The following A_after_B tests try to verify that failures of the wrapped
// stream caused by a wrapper method are not visible in the stream state after the
// method returns and, more importantly, do not affect the result of subsequent
// calls.

BOOST_AUTO_TEST_CASE( seek_after_failed_seek )
{
    test_input_stream stl_stream = input_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(stl_stream);

    LARGE_INTEGER move;
    move.QuadPart = -1;

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK(has_hresult(s->Seek(move, STREAM_SEEK_SET, &new_pos), s, E_FAIL));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 0U);

    move.QuadPart = 1;

    new_pos.QuadPart = 42U;
    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 1U);

    check_read_to_end(s, string("obbeldy gook"));
}

BOOST_AUTO_TEST_CASE( seek_after_failed_read )
{
    test_input_stream stl_stream = input_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string());

    LARGE_INTEGER move = {0};
    ULARGE_INTEGER new_pos = {42U};

    BOOST_CHECK(is_s_ok(s->Seek(move, STREAM_SEEK_SET, &new_pos), s));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 0U);
}

BOOST_AUTO_TEST_CASE( read_after_failed_read )
{
    test_input_stream stl_stream = input_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string());
    check_read_to_end(s, string());
}

BOOST_AUTO_TEST_CASE( commit_after_failed_read )
{
    test_io_stream stl_stream = io_stream();

    com_ptr<IStream> s = adapt_stream(stl_stream);

    check_read_to_end(s, string());

    BOOST_CHECK(is_s_ok(s->Commit(STGC_DEFAULT), s));
}

BOOST_AUTO_TEST_CASE( copy_after_failed_read )
{
    test_io_stream source = io_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    check_read_to_end(s, string("gobbeldy gook"));

    ULARGE_INTEGER amount = {13};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 0U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 0U);

    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string());
}

BOOST_AUTO_TEST_CASE( copy_after_failed_seek )
{
    test_io_stream source = io_stream("gobbeldy gook");
    ostringstream dest;

    com_ptr<IStream> s = adapt_stream(source);
    com_ptr<IStream> d = adapt_stream(dest);

    LARGE_INTEGER move;
    move.QuadPart = -1;

    ULARGE_INTEGER new_pos = {42U};
    BOOST_CHECK(
        has_hresult(s->Seek(move, STREAM_SEEK_SET, &new_pos), s, E_FAIL));
    BOOST_CHECK_EQUAL(new_pos.QuadPart, 0U);

    ULARGE_INTEGER amount = {13};
    ULARGE_INTEGER bytes_read = {0};
    ULARGE_INTEGER bytes_written = {0};
    BOOST_CHECK(
        is_s_ok(s->CopyTo(d.in(), amount, &bytes_read, &bytes_written), s));
    BOOST_CHECK_EQUAL(bytes_read.QuadPart, 13U);
    BOOST_CHECK_EQUAL(bytes_written.QuadPart, 13U);

    check_read_to_end(s, string());

    check_stream_contains(string("gobbeldy gook"));
    BOOST_CHECK_EQUAL(dest.str(), string("gobbeldy gook"));
}

BOOST_AUTO_TEST_CASE( stat_request_name_has_no_name )
{
    test_io_stream source = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(source);

    STATSTG stg = STATSTG();
    BOOST_REQUIRE(is_s_ok(s->Stat(&stg, STATFLAG_DEFAULT), s));
    BOOST_REQUIRE(stg.pwcsName);
    shared_ptr<OLECHAR> name(stg.pwcsName, ::CoTaskMemFree);

    BOOST_CHECK(wstring(name.get()) == OLESTR(""));

    BOOST_CHECK_EQUAL(stg.cbSize.QuadPart, 13U);
    BOOST_CHECK_EQUAL(stg.type, static_cast<DWORD>(STGTY_STREAM));
}

BOOST_AUTO_TEST_CASE( stat_request_name_has_name )
{
    test_io_stream source = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(source, L"clever trevor");

    STATSTG stg = STATSTG();
    BOOST_REQUIRE(is_s_ok(s->Stat(&stg, STATFLAG_DEFAULT), s));
    BOOST_REQUIRE(stg.pwcsName);
    shared_ptr<OLECHAR> name(stg.pwcsName, ::CoTaskMemFree);

    BOOST_CHECK(wstring(name.get()) == OLESTR("clever trevor"));

    BOOST_CHECK_EQUAL(stg.cbSize.QuadPart, 13U);
    BOOST_CHECK_EQUAL(stg.type, static_cast<DWORD>(STGTY_STREAM));
}

BOOST_AUTO_TEST_CASE( stat_no_request_name_has_no_name )
{
    test_io_stream source = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(source);

    STATSTG stg = STATSTG();
    BOOST_REQUIRE(is_s_ok(s->Stat(&stg, STATFLAG_NONAME), s));
    BOOST_CHECK(!stg.pwcsName);
    shared_ptr<OLECHAR> name(stg.pwcsName, ::CoTaskMemFree); // just in case

    BOOST_CHECK_EQUAL(stg.cbSize.QuadPart, 13U);
    BOOST_CHECK_EQUAL(stg.type, static_cast<DWORD>(STGTY_STREAM));
}

BOOST_AUTO_TEST_CASE( stat_no_request_name_has_name )
{
    test_io_stream source = io_stream("gobbeldy gook");

    com_ptr<IStream> s = adapt_stream(source, L"clever trevor");

    STATSTG stg = STATSTG();
    BOOST_REQUIRE(is_s_ok(s->Stat(&stg, STATFLAG_NONAME), s));
    BOOST_CHECK(!stg.pwcsName);
    shared_ptr<OLECHAR> name(stg.pwcsName, ::CoTaskMemFree); // just in case

    BOOST_CHECK_EQUAL(stg.cbSize.QuadPart, 13U);
    BOOST_CHECK_EQUAL(stg.type, static_cast<DWORD>(STGTY_STREAM));
}

BOOST_AUTO_TEST_CASE( adapt_stream_adopt_ownership )
{
    com_ptr<IStream> s =
        adapt_stream_pointer(io_stream_pointer("gobbeldy gook"));

    check_read_to_end(s, string("gobbeldy gook"));
}

BOOST_AUTO_TEST_SUITE_END()