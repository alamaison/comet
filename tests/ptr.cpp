#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/ptr.h>
#include <comet/server.h>

#include <ObjIdl.h> // IStorage, IPersist

using comet::com_ptr;
using comet::simple_object;

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
        { 0x546aefac, 0x6ecc, 0x44bf,
          { 0x8d, 0x17, 0x13, 0x83, 0x46, 0x24, 0xd4, 0x15 } };
        return iid;
    }
};

BOOST_AUTO_TEST_SUITE( com_ptr_tests )

// This tests for a reference counting bug encountered in Comet A5 -
// the destructor for com_ptr<> was missing so Release() was never called.
BOOST_AUTO_TEST_CASE( destruction )
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

BOOST_AUTO_TEST_CASE( no_throw_cast )
{
    com_ptr<IStorage> itf2 = 0;

    com_ptr<IPersist> itf1( com_cast(itf2) );

    itf1 = com_cast(itf1);

}

BOOST_AUTO_TEST_CASE( throwing_cast )
{
    com_ptr<IStorage> itf2 = 0;

    com_ptr<IPersist> itf1( try_cast(itf2) );

    itf1 = try_cast(itf1);
}

BOOST_AUTO_TEST_CASE( no_throw_cast_raw )
{
    IStorage* itf2 = 0;

    com_ptr<IPersist> itf1( comet::com_cast(itf2) ); // No ADL for raw pointer

    itf1 = com_cast(itf1);
}

BOOST_AUTO_TEST_CASE( throwing_cast_raw )
{
    IStorage* itf2 = 0;

    com_ptr<IPersist> itf1( comet::try_cast(itf2) ); // No ADL for raw pointer

    itf1 = try_cast(itf1);
}

BOOST_AUTO_TEST_SUITE_END()



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

// 2. http://groups.yahoo.com/group/tlb2h/message/18
void check_upcast()
{
    com_ptr<IDummyInterface> d;
    com_ptr<IUnknown> unknown;
    unknown = d;
}