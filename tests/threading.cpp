#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/lw_lock.h> // lw_lock, auto_reader_lock, auto_writer_lock
#include <comet/threading.h> // thread

using comet::auto_reader_lock;
using comet::auto_writer_lock;
using comet::lw_lock;
using comet::thread;

BOOST_AUTO_TEST_SUITE( threading_tests )

BOOST_AUTO_TEST_CASE( lock )
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

BOOST_AUTO_TEST_SUITE_END()