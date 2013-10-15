/** \file
  * IStream adapters.
  */
/*
 * Copyright (C) 2013 Alexander Lamaison <alexander.lamaison@gmail.com>
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

#ifndef COMET_STREAM_H
#define COMET_STREAM_H

#include <comet/config.h>

#include <comet/error.h> // com_error
#include <comet/handle_except.h> // COMET_CATCH_CLASS_INTERFACE_BOUNDARY
#include <comet/server.h> // simple_object

#include <cassert> // assert
#include <istream>
#include <ostream>

namespace comet {

namespace impl {

    // These do_read/write helper functions dispatch the stream-type specific code
    // so that we only need one class to implement all the stream adapters

    inline void do_istream_read(
        std::istream& stream, void* buffer, ULONG buffer_size_in_bytes,
        ULONG& bytes_read_out)
    {
        typedef std::istream stream_type;

        bytes_read_out = 0U;

        // Using stream buffer directly for unformatted I/O as stream.write()
        // doesn't give us the number of characters written.
        //
        // We use sputc rather than sputn as the latter is allowed to throw
        // an exception (from inner call to `underflow` among, presumably,
        // others: http://stackoverflow.com/a/19105858/67013) part way through
        // the transfer, which prevents us getting the number of characters
        // read in the error case.  The following code keeps bytes_read_out
        // updated so that it is correct even if sbumpc throws an exception
        // later.
        stream_type::sentry stream_sentry(stream);
        if (stream_sentry)
        {
            while (bytes_read_out < buffer_size_in_bytes)
            {
                stream_type::traits_type::int_type result =
                    stream.rdbuf()->sbumpc();

                if (stream_type::traits_type::eq_int_type(
                    result, stream_type::traits_type::eof()))
                {
                    break;
                }
                else
                {
                    // arithmetic to find position is done in bytes and only
                    // converted to character type when dereferenced.
                    unsigned char* pos =
                        reinterpret_cast<unsigned char*>(buffer) +
                        bytes_read_out;

                    *reinterpret_cast<stream_type::char_type*>(pos) =
                        stream_type::traits_type::to_char_type(result);

                    bytes_read_out =
                        bytes_read_out + sizeof(stream_type::char_type);
                }
            }
        }

        //return stream.rdbuf()->sgetn(
        //    reinterpret_cast<char*>(buffer),
        //    buffer_size / sizeof(std::istream::char_type));
    }

    inline void do_ostream_write(
        std::ostream& stream, const void* buffer, ULONG buffer_size_in_bytes,
        ULONG& bytes_written_out)
    {
        typedef std::ostream stream_type;

        bytes_written_out = 0U;

        // Using stream buffer directly for unformatted I/O as stream.write()
        // doesn't give us the number of characters written.
        //
        // We use sputc rather than sputn as the latter is allowed to throw
        // an exception (from inner call to `overflow` among, presumably,
        // others: http://stackoverflow.com/a/19105858/67013) part way through
        // the transfer, which prevents us getting the number of characters
        // written in the error case. The following code keeps bytes_written_out
        // updated so that it is correct even if sputc throws an exception
        // later.
        stream_type::sentry stream_sentry(stream);
        if (stream_sentry)
        {
            while (bytes_written_out < buffer_size_in_bytes)
            {
                // arithmetic to find position is done in bytes and only
                // converted to character type when dereferenced.
                const unsigned char* pos =
                    reinterpret_cast<const unsigned char*>(buffer) +
                    bytes_written_out;

                stream_type::traits_type::int_type result =
                    stream.rdbuf()->sputc(
                        *reinterpret_cast<const stream_type::char_type*>(pos));

                if (stream_type::traits_type::eq_int_type(
                    result, stream_type::traits_type::eof()))
                {
                    break;
                }
                else
                {
                    bytes_written_out =
                        bytes_written_out + sizeof(stream_type::char_type);
                }
            }
        }

        //return stream.rdbuf()->sputn(
        //    reinterpret_cast<const char*>(buffer),
        //    buffer_size / sizeof(std::ostream::char_type));
    }

    template<typename Stream, bool is_istream, bool is_ostream>
    class stream_traits;

    template<typename Stream>
    class stream_traits<Stream, true, false>
    {
    public:
        void do_read(
            Stream& stream, void* buffer, ULONG buffer_size_in_bytes,
            ULONG& bytes_read_out)
        {
            do_istream_read(stream, buffer, buffer_size_in_bytes, bytes_read_out);
        }

        void do_write(
            Stream& /*stream*/, const void* /*buffer*/,
            ULONG /*buffer_size_in_bytes*/, ULONG& bytes_written_out)
        {
            bytes_written_out = 0U;
            throw com_error(
                "std::istream does not support writing", STG_E_ACCESSDENIED);
        }
    };

    template<typename Stream>
    class stream_traits<Stream, false, true>
    {
    public:

        void do_read(
            Stream& /*stream*/, void* /*buffer*/,
            ULONG /*buffer_size_in_bytes*/, ULONG& bytes_read_out)
        {
            bytes_read_out = 0U;
            throw com_error(
                "std::ostream does not support reading", STG_E_ACCESSDENIED);
        }

        void do_write(
            Stream& stream, const void* buffer,
            ULONG buffer_size_in_bytes, ULONG& bytes_written_out)
        {
            do_ostream_write(
                stream, buffer, buffer_size_in_bytes, bytes_written_out);
        }
    };

    template<typename Stream>
    class stream_traits<Stream, true, true>
    {
    private:

        enum last_stream_operation
        {
            read,
            write
        };

    public:

        stream_traits() : m_last_op(read) {}

        void do_read(
            Stream& stream, void* buffer, ULONG buffer_size_in_bytes,
            ULONG& bytes_read_out)
        {
            bytes_read_out = 0U;

            // sync reading position with writing position, which was the last
            // one used and is allowed to be different in C++ streams but
            // not COM IStreams
            if (m_last_op == write)
            {
                stream.seekg(stream.tellp());
                m_last_op = read;
            }
            assert(m_last_op == read);

            do_istream_read(stream, buffer, buffer_size_in_bytes, bytes_read_out);
        }

        void do_write(
            Stream& stream, const void* buffer,
            ULONG buffer_size_in_bytes, ULONG& bytes_written_out)
        {
            bytes_written_out = 0U;

            // sync writing position with reading position, which was the last
            // one used and is allowed to be different in C++ streams but
            // not COM IStreams
            if (m_last_op == read)
            {
                stream.seekp(stream.tellg());
                m_last_op = write;
            }
            assert(m_last_op == write);

            do_ostream_write(
                stream, buffer, buffer_size_in_bytes, bytes_written_out);
        }

    private:
        last_stream_operation m_last_op;
    };

    /**
     * Wrap COM IStream interface around C++ IOStream.
     *
     * Unlike C++ streams which may have separate read and write positions that
     * move independently, COM IStreams assume a single combined read/write head.
     * Therefore this wrapper always starts the next read or write operation
     * from the where the last operation finished, regardless of whether that
     * operation was a call to `Read` or `Write`.
     *
     * @note This only applies for as long as the read/write positions are
     *       modified only via this wrapper.  If the positions are modified by
     *       directly on the underlying IOStream, it is undefined whether the
     *       starting point for the next call to `Read`/`Write` is syncronised
     *       with the end of the previous operation.
     */
    template<typename Stream>
    class adapted_stream : public simple_object<IStream>
    {
    public:

        adapted_stream(Stream& stream) : m_stream(stream) {}

        virtual HRESULT STDMETHODCALLTYPE Read( 
            void* buffer, ULONG buffer_size, ULONG* read_count_out)
        {
            if (read_count_out)
            {
                *read_count_out = 0U;
            }

            try
            {
                if (!buffer)
                {
                    throw com_error("No buffer given", STG_E_INVALIDPOINTER);
                }

                // We use a dummy read count location if one is not given so
                // that do_read can keep it updated correctly even if
                // do_read throws an exception.
                ULONG dummy_read_count = 0U;
                if (!read_count_out)
                {
                    read_count_out = &dummy_read_count;
                }

                m_traits.do_read(
                    m_stream, buffer, buffer_size, *read_count_out);

                if (*read_count_out < buffer_size)
                {
                    return S_FALSE;
                }
                else
                {
                    assert(*read_count_out == buffer_size);
                    return S_OK;
                }
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Read", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Write(
            const void* buffer, ULONG buffer_size, ULONG* written_count_out)
        {
            if (written_count_out)
            {
                *written_count_out = 0U;
            }

            try
            {
                if (!buffer)
                {
                    throw com_error("Buffer not given", STG_E_INVALIDPOINTER);
                }

                // We use a dummy written count out location if one is not
                // given so that do_write can keep it updated correctly even if
                // do_write throws an exception.
                ULONG dummy_written_count = 0U;
                if (!written_count_out)
                {
                    written_count_out = &dummy_written_count;
                }

                m_traits.do_write(
                    m_stream, buffer, buffer_size, *written_count_out);

                if (*written_count_out < buffer_size)
                {
                    throw com_error(
                        "Unable to complete write operation", STG_E_MEDIUMFULL);
                }
                else
                {
                    assert(*written_count_out == buffer_size);
                    return S_OK;
                }
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Write", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Seek(
            LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER* new_position_out)
        {

            if (new_position_out)
            {
                new_position_out->QuadPart = 0U; 
            }
            try
            {


                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Seek", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER new_size)
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("SetSize", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE CopyTo( 
            IStream* destination, ULARGE_INTEGER amount,
            ULARGE_INTEGER* read_count_out, ULARGE_INTEGER* written_count_out)
        {
            if (read_count_out)
            {
                read_count_out->QuadPart = 0U;
            }

            if (written_count_out)
            {
                written_count_out->QuadPart = 0U;
            }

            try
            {

                if (!destination)
                {
                    throw com_error(
                        "Destination stream not given", STG_E_INVALIDPOINTER);
                }

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("CopyTo", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Commit(DWORD commit_flags)
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Commit", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Revert()
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Revert", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE LockRegion( 
            ULARGE_INTEGER offset, ULARGE_INTEGER extent, DWORD lock_type)
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("LockRegion", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE UnlockRegion(
            ULARGE_INTEGER offset, ULARGE_INTEGER extent, DWORD lock_type)
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("UnlockRegion", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Stat( 
            STATSTG* attributes_out, DWORD stat_flag)
        {

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Stat", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE Clone(IStream **stream_out)
        {
            if (stream_out)
            {
                *stream_out = NULL;
            }

            try
            {

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Clone", "adapted_stream");
        }

    private:

        Stream& m_stream;
        impl::stream_traits<
            Stream,
            type_traits::super_sub_class<std::istream, Stream>::result,
            type_traits::super_sub_class<std::ostream, Stream>::result
        > m_traits;
    };
}

template<typename Stream>
inline com_ptr<IStream> adapt_stream(Stream& stream)
{
    return new impl::adapted_stream<Stream>(stream);
}

}

#endif
