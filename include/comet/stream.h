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
#include <limits> // numeric_limits
#include <ostream>

namespace comet {

namespace impl {

    static const size_t COPY_CHUNK_SIZE = 512;

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
        else if (stream.bad())
        {
            // Not catching EOF case here as it seems reasonable that
            // the sentry might try to read something and reach EOF
            throw std::runtime_error("Unable to ready stream for reading");
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
        else if (stream.bad())
        {
            // Not catching EOF case here as it seems reasonable that
            // the sentry might try to write something and reach EOF
            throw std::runtime_error("Unable to ready stream for write");
        }

        //return stream.rdbuf()->sputn(
        //    reinterpret_cast<const char*>(buffer),
        //    buffer_size / sizeof(std::ostream::char_type));
    }

    /** Ensure read position updated even in case of exception */
    template<typename Stream>
    class read_position_finaliser
    {
    public:
        read_position_finaliser(
            Stream& stream, std::streampos& new_position_out)
            : m_stream(stream), m_new_position_out(new_position_out)
        {}

        ~read_position_finaliser()
        {
            // we still want the read position if previous op failed
            m_stream.clear();

            std::streampos new_position = m_stream.tellg();
            if (m_stream)
            {
                m_new_position_out = new_position;
            }
            else
            {
                m_new_position_out = std::streampos();
            }
        }

    private:

        read_position_finaliser(const read_position_finaliser&);
        read_position_finaliser& operator=(const read_position_finaliser&);

        Stream& m_stream;
        std::streampos& m_new_position_out;
    };

    /** Ensure write position updated even in case of exception */
    template<typename Stream>
    class write_position_finaliser
    {
    public:
        write_position_finaliser(
            Stream& stream, std::streampos& new_position_out)
            : m_stream(stream), m_new_position_out(new_position_out)
        {}

        ~write_position_finaliser()
        {
            // we still want the write position if previous op failed
            m_stream.clear();

            std::streampos new_position = m_stream.tellp();
            if (m_stream)
            {
                m_new_position_out = new_position;
            }
            else
            {
                m_new_position_out = std::streampos();
            }
        }

    private:
        write_position_finaliser(const write_position_finaliser&);
        write_position_finaliser& operator=(const write_position_finaliser&);

        Stream& m_stream;
        std::streampos& m_new_position_out;
    };

    /**
     * Copies `streampos` out-parameter to ULARGE_INTEGER out parameter
     * in the face of exceptions.
     *
     * Using this class means you can concentrate on keeping the `streampos`
     * variable updated, confident in the knowledge the the COM-interface
     * ULARGE_INTEGER parameter will eventually be updated to match it,
     * no matter what.
     *
     * Assumes that the source out parameter is guaranteed to be valid at
     * all times.  Valid doesn't necessarily mean correct but that it always
     * contains the value we want to set the out parameter to, even if that is
     * wrong (for instance failure while calculating correct position).
     */
    class position_out_converter
    {
    public:

        position_out_converter(
            std::streampos& source_out_parameter,
            ULARGE_INTEGER* destination_out_parameter)
            :
        m_source(source_out_parameter),
        m_destination(destination_out_parameter) {}

        ~position_out_converter()
        {
            if (m_destination)
            {
                try
                {
                    // Convert to streamoff because streampos may not
                    // be an integer.  streamoff is guaranteed to be.
                    std::streamoff offset_from_beginning = m_source;
                    if (offset_from_beginning < 0)
                    {
                        assert(!"negative offset from beginning?! screwed.");
                        m_destination->QuadPart = 0U;
                    }
                    else
                    {
                        m_destination->QuadPart = offset_from_beginning;
                    }
                }
                catch(const std::exception&)
                {
                    // Only way this can happen is if streampos refuses to
                    // convert to streamoff in which case we really are screwed.
                    m_destination->QuadPart = 0U;
                }
            }
        }

    private:
        position_out_converter(const position_out_converter&);
        position_out_converter& operator=(const position_out_converter&);

        std::streampos& m_source;
        ULARGE_INTEGER* m_destination;
    };

    /**
     * Increments a total counter with an increment are end of scope,
     * regardless of how that scope is ended.
     *
     * Assumes the increment parameter is valid at all times.
     */
    class byte_count_incrementer
    {
    public:
        byte_count_incrementer(
            ULARGE_INTEGER* total, ULONG& increment)
            : m_total(total), m_increment(increment) {}

        ~byte_count_incrementer()
        {
            if (m_total)
            {
                m_total->QuadPart += m_increment;
            }
        }

    private:

        byte_count_incrementer(const byte_count_incrementer&);
        byte_count_incrementer& operator=(const byte_count_incrementer&);

        ULARGE_INTEGER* m_total;
        ULONG& m_increment;
    };

    template<typename StreamTraits>
    class position_resetter
    {
    public:
        position_resetter(StreamTraits& traits, std::streampos position)
            :
        m_traits(traits), m_position(position) {}

        ~position_resetter()
        {
            try
            {
                std::streampos new_position;
                m_traits.do_seek(
                    m_position, std::ios_base::beg, new_position);

                assert(new_position == m_position);
            }
            catch (const std::exception&)
            {}
        }

    private:
        position_resetter(const position_resetter&);
        position_resetter& operator=(const position_resetter&);

        StreamTraits& m_traits;
        std::streampos m_position;
    };

    /**
     * Calculate offset of end of stream from start.
     */
    template<typename StreamTraits>
    inline std::streamoff stream_size(StreamTraits& traits)
    {
        position_resetter<StreamTraits>(traits, traits.do_tell());

        std::streampos new_position;
        traits.do_seek(0, std::ios_base::end, new_position);

        return new_position;
    }

    template<typename Stream, bool is_istream, bool is_ostream>
    class stream_traits;

    template<typename Stream>
    class stream_traits<Stream, true, false>
    {
    public:
        explicit stream_traits(Stream& stream) : m_stream(stream) {}

        void do_read(
            void* buffer, ULONG buffer_size_in_bytes,
            ULONG& bytes_read_out)
        {
            do_istream_read(
                m_stream, buffer, buffer_size_in_bytes, bytes_read_out);
        }

        void do_write(
            const void* /*buffer*/,
            ULONG /*buffer_size_in_bytes*/, ULONG& bytes_written_out)
        {
            bytes_written_out = 0U;
            throw com_error(
                "std::istream does not support writing", STG_E_ACCESSDENIED);
        }

        void do_seek(
            std::streamoff offset, std::ios_base::seekdir way,
            std::streampos& new_position_out)
        {
            read_position_finaliser<Stream> position_out_updater(
                m_stream, new_position_out);

            if (!m_stream.seekg(offset, way))
            {
                throw std::runtime_error("Unable to change read position");
            }
        }

        std::streampos do_tell()
        {
            std::streampos pos = m_stream.tellg();
            if (!m_stream)
            {
                throw std::runtime_error("Stream position unavailable");
            }

            return pos;
        }

    private:
        stream_traits(const stream_traits&);
        stream_traits& operator=(const stream_traits&);

        Stream& m_stream;
    };

    template<typename Stream>
    class stream_traits<Stream, false, true>
    {
    public:
        explicit stream_traits(Stream& stream) : m_stream(stream) {}

        void do_read(
            void* /*buffer*/,
            ULONG /*buffer_size_in_bytes*/, ULONG& bytes_read_out)
        {
            bytes_read_out = 0U;
            throw com_error(
                "std::ostream does not support reading", STG_E_ACCESSDENIED);
        }

        void do_write(
            const void* buffer,
            ULONG buffer_size_in_bytes, ULONG& bytes_written_out)
        {
            do_ostream_write(
                m_stream, buffer, buffer_size_in_bytes, bytes_written_out);
        }

        void do_seek(
            std::streamoff offset, std::ios_base::seekdir way,
            std::streampos& new_position_out)
        {
            write_position_finaliser<Stream> position_out_updater(
                m_stream, new_position_out);

            if (!m_stream.seekp(offset, way))
            {
                throw std::runtime_error("Unable to change write position");
            }
        }

        std::streampos do_tell()
        {
            std::streampos pos = m_stream.tellp();
            if (!m_stream)
            {
                throw std::runtime_error("Stream position unavailable");
            }

            return pos;
        }

    private:
        stream_traits(const stream_traits&);
        stream_traits& operator=(const stream_traits&);

        Stream& m_stream;
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
        explicit stream_traits(Stream& stream)
            : m_stream(stream), m_last_op(read) {}

        void do_read(
            void* buffer, ULONG buffer_size_in_bytes, ULONG& bytes_read_out)
        {
            bytes_read_out = 0U;

            // sync reading position with writing position, which was the last
            // one used and is allowed to be different in C++ streams but
            // not COM IStreams
            if (m_last_op == write)
            {
                m_stream.seekg(m_stream.tellp());
                // We ignore errors syncing the positions as even iostreams may
                // not be seekable at all

                m_last_op = read;
            }
            assert(m_last_op == read);

            do_istream_read(
                m_stream, buffer, buffer_size_in_bytes, bytes_read_out);
        }

        void do_write(
            const void* buffer, ULONG buffer_size_in_bytes,
            ULONG& bytes_written_out)
        {
            bytes_written_out = 0U;

            // sync writing position with reading position, which was the last
            // one used and is allowed to be different in C++ streams but
            // not COM IStreams
            if (m_last_op == read)
            {
                m_stream.seekp(m_stream.tellg());
                // We ignore errors syncing the positions as even iostreams may
                // not be seekable at all

                m_last_op = write;
            }
            assert(m_last_op == write);

            do_ostream_write(
                m_stream, buffer, buffer_size_in_bytes, bytes_written_out);
        }

        void do_seek(
            std::streamoff offset, std::ios_base::seekdir way,
            std::streampos& new_position_out)
        {

            // Unlike with do_read/do_write, we do not ignore errors when
            // trying to sync the read/write positions as, if the
            // first seek succeeded, we know the stream supports
            // seeking.  Therefore a later error is really an error.

            if (m_last_op == read)
            {
                read_position_finaliser<Stream> position_out_updater(
                    m_stream, new_position_out);

                if (!m_stream.seekg(offset, way))
                {
                    throw std::runtime_error("Unable to change read position");
                }
                else
                {
                    if (!m_stream.seekp(m_stream.tellg()))
                    {
                        throw std::runtime_error(
                            "Unable to synchronise write position");
                    }
                }
            }
            else
            {
                write_position_finaliser<Stream> position_out_updater(
                    m_stream, new_position_out);

                if (!m_stream.seekp(offset, way))
                {
                    throw std::runtime_error("Unable to change write position");
                }
                else
                {
                    if (!m_stream.seekg(m_stream.tellp()))
                    {
                        throw std::runtime_error(
                            "Unable to synchronise read position");
                    }
                }
            }
        }

        std::streampos do_tell()
        {
            std::streampos pos;
            if (m_last_op == read)
            {
                pos = m_stream.tellg();
            }
            else
            {
                pos = m_stream.tellp();
            }

            if (!m_stream)
            {
                throw std::runtime_error("Stream position unavailable");
            }

            return pos;
        }

    private:
        stream_traits(const stream_traits&);
        stream_traits& operator=(const stream_traits&);

        Stream& m_stream;
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
    private:

        typedef impl::stream_traits<
            Stream,
            type_traits::super_sub_class<std::istream, Stream>::result,
            type_traits::super_sub_class<std::ostream, Stream>::result
        > stream_traits_type;

    public:

        adapted_stream(Stream& stream) : m_stream(stream), m_traits(stream) {}

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

                m_traits.do_read(buffer, buffer_size, *read_count_out);

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

                m_traits.do_write(buffer, buffer_size, *written_count_out);

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
            LARGE_INTEGER offset, DWORD origin,
            ULARGE_INTEGER* new_position_out)
        {
            std::streampos new_stream_position_out = std::streampos();
            impl::position_out_converter out_param_guard(
                new_stream_position_out, new_position_out);

            try
            {
                std::ios_base::seekdir way;
                if (origin == STREAM_SEEK_CUR)
                {
                    way = std::ios_base::cur;
                }
                else if (origin == STREAM_SEEK_SET)
                {
                    way = std::ios_base::beg;
                }
                else if (origin == STREAM_SEEK_END)
                {
                    way = std::ios_base::end;
                }
                else
                {
                    throw std::invalid_argument(
                        "Unrecognised stream seek origin");
                }

                if (offset.QuadPart > 
                    (std::numeric_limits<std::streamoff>::max)())
                {
                    throw std::overflow_error("Seek offset too large");
                }
                else if (offset.QuadPart <
                    (std::numeric_limits<std::streamoff>::min)())
                {
                    throw std::underflow_error("Seek offset too small");
                }
                else
                {
                    m_traits.do_seek(
                        static_cast<std::streamoff>(offset.QuadPart),
                        way, new_stream_position_out);

                    return S_OK;
                }
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("Seek", "adapted_stream");
        }

        /**
         * Expand stream to given size.
         *
         * The will only increase a stream's size if it is smaller than the
         * given size.  If the stream size is already equal or bigger, it
         * remains unchanged.
         *
         * Not all streams support changing size.  In particular, `istream`s
         * do not support this method.
         */
        virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER new_size)
        {
            try
            {
                position_resetter<stream_traits_type> resetter(
                    m_traits, m_traits.do_tell());

                if (new_size.QuadPart > 0)
                {
                    ULARGE_INTEGER new_end_position;
                    new_end_position.QuadPart = new_size.QuadPart - 1;

                    std::streamoff new_offset;
                    if (new_end_position.QuadPart > 
                        (std::numeric_limits<std::streamoff>::max)())
                    {
                        throw com_error(
                            "Seek offset too large", STG_E_INVALIDFUNCTION);
                    }
                    else
                    {
                        new_offset = static_cast<std::streamoff>(
                            new_end_position.QuadPart);
                    }

                    std::streamoff existing_end = stream_size(m_traits);

                    if (new_offset > existing_end)
                    {
                        std::streampos new_position;
                        m_traits.do_seek(
                            new_offset, std::ios_base::beg, new_position);

                        assert(std::streamoff(new_position) == new_offset);

                        // Force the stream to expand by writing NUL at
                        // new extent
                        ULONG bytes_written;
                        m_traits.do_write("\0", 1, bytes_written);
                        assert(bytes_written == 1);
                    }
                }

                return S_OK;
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("SetSize", "adapted_stream");
        }

        virtual HRESULT STDMETHODCALLTYPE CopyTo( 
            IStream* destination, ULARGE_INTEGER amount,
            ULARGE_INTEGER* bytes_read_out, ULARGE_INTEGER* bytes_written_out)
        {
            ULARGE_INTEGER dummy_read_out;
            if (!bytes_read_out)
            {
                bytes_read_out = &dummy_read_out;
            }
            bytes_read_out->QuadPart = 0U;

            ULARGE_INTEGER dummy_written_out;
            if (!bytes_written_out)
            {
                bytes_written_out = &dummy_written_out;
            }
            bytes_written_out->QuadPart = 0U;

            try
            {
                if (!destination)
                {
                    throw com_error(
                        "Destination stream not given", STG_E_INVALIDPOINTER);
                }

                std::vector<unsigned char> buffer(COPY_CHUNK_SIZE);

                // Perform copy operation in chunks COPY_CHUNK bytes big
                // The chunk must be less than the biggest ULONG in size
                // because of the limits of the Read/Write API.  Of course
                // it will be in any case as it would be insane to use more
                // memory than that, but we make sure anyway using the first
                // min comparison
                do {
                    ULONG next_chunk_size =
                        static_cast<ULONG>(
                            min(
                                (std::numeric_limits<ULONG>::max)(),
                                min(
                                    amount.QuadPart - bytes_read_out->QuadPart,
                                    buffer.size())));

                    ULONG read_this_round = 0U;
                    ULONG written_this_round = 0U;

                    // These two take care of updating the total on each pass
                    // round the loop as well as on termination, exception or
                    // natural.
                    //
                    // The means the out counts are valid even in the failure
                    // case. MSDN says they don't have to be but, as we can,
                    // we might as well
                    impl::byte_count_incrementer read_incrementer(
                        bytes_read_out, read_this_round);

                    impl::byte_count_incrementer write_incrementer(
                        bytes_written_out, written_this_round);

                    m_traits.do_read(
                        &buffer[0], next_chunk_size, read_this_round);
                    HRESULT hr = destination->Write(
                        &buffer[0], read_this_round, &written_this_round);
                    if (FAILED(hr))
                    {
                        com_error_from_interface(destination, hr);
                    }

                    if (read_this_round < next_chunk_size)
                    {
                        // EOF
                        break;
                    }

                } while (amount.QuadPart > bytes_read_out->QuadPart);

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
        stream_traits_type m_traits;
    };
}

template<typename Stream>
inline com_ptr<IStream> adapt_stream(Stream& stream)
{
    return new impl::adapted_stream<Stream>(stream);
}

}

#endif
