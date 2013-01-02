#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/ptr.h>
#include <comet/server.h> // simple_object
#include <comet/smart_enum.h>
#include <comet/stl_enum.h>

#include <exception>
#include <stdexcept> // runtime_error
#include <vector>

using comet::com_ptr;
using comet::nil;
using comet::simple_object;
using comet::smart_enumeration;
using comet::stl_enumeration_t;

using std::exception;
using std::runtime_error;
using std::vector;

template<> struct comet::comtype<IEnumUnknown>
{
    static const IID& uuid() throw() { return IID_IEnumUnknown; }
    typedef IUnknown base;
};

template<> struct comet::impl::type_policy<IUnknown*>
{
    template<typename S>
    static void init(IUnknown*& p, const S& s)
    {  p = s.get(); p->AddRef(); }

    static void clear(IUnknown*& p) { p->Release(); }
};

BOOST_AUTO_TEST_SUITE( enum_tests )

void empty_enum_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punk;
    ULONG count = 12;
    HRESULT hr = e->Next(1, &punk, &count);
    if (hr != S_FALSE)
        throw runtime_error("HRESULT is not S_FALSE");
    if (count != 0)
        throw runtime_error("Incorrect return count");
}

/**
* Empty stl_enumeration_t.
*/
BOOST_AUTO_TEST_CASE( stl_enumeration_t_empty )
{
    typedef std::vector< com_ptr<IUnknown> > collection_type;
    collection_type coll;
    com_ptr<IEnumUnknown> e = new stl_enumeration_t<
        IEnumUnknown, collection_type, IUnknown*>(coll);
    empty_enum_test(e);
}

/**
* Empty smart_enumeration.
*/
BOOST_AUTO_TEST_CASE( smart_enumeration_empty )
{
    typedef std::auto_ptr< std::vector< com_ptr<IUnknown> > > smart_collection;
    smart_collection coll(new std::vector< com_ptr<IUnknown> >());
    com_ptr<IEnumUnknown> e = new smart_enumeration<
        IEnumUnknown, smart_collection, IUnknown*>(coll);
    coll.reset(); // force enum to be the only owner (should happen
    // regardless with an auto_ptr)
    empty_enum_test(e);
}

vector< com_ptr<IUnknown> > test_collection()
{
    class test_obj : public simple_object<nil> {};

    vector< com_ptr<IUnknown> > collection;
    collection.push_back(new test_obj());
    collection.push_back(new test_obj());
    collection.push_back(new test_obj());
    return collection;
}

template<typename Itf, size_t arraysize>
void release_pointer_array(Itf* (&pointers)[arraysize])
{
    for (int i=0; i < arraysize; ++i)
        if (pointers[i])
            pointers[i]->Release();
}

void enum_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punks[3] = { NULL, NULL, NULL };

    try
    {
        // Test validity
        for (int i = 0; i < 3; ++i)
        {
            IUnknown* punk;
            ULONG count = 12;
            HRESULT hr = e->Next(1, &punk, &count);
            if (hr != S_OK)
                throw runtime_error("HRESULT is not S_OK");

            if (count != 1)
                throw runtime_error("Incorrect return count");

            if (punk == NULL)
                throw runtime_error("NULL returned instead of object");

            punks[i] = punk; // store for uniqueness test later
        }

        empty_enum_test(e);

        // Test uniqueness
        if (punks[0] == punks[1])
            throw runtime_error("Same object returned twice");
        if (punks[0] == punks[2])
            throw runtime_error("Same object returned twice");
        if (punks[2] == punks[1])
            throw runtime_error("Same object returned twice");

        release_pointer_array(punks);
    }
    catch(const exception&)
    {
        release_pointer_array(punks);
        throw;
    }
}

void enum_chunk_test(com_ptr<IEnumUnknown> e)
{
    IUnknown* punks[4] = { NULL, NULL, NULL, NULL };

    try
    {
        ULONG count = 12;
        HRESULT hr = e->Next(4, punks, &count); // request more than expected
        if (hr != S_FALSE)
            throw runtime_error("HRESULT is not S_FALSE");

        if (count != 3)
            throw runtime_error("Incorrect return count");

        // Test validity
        for (int i = 0; i < 3; ++i)
        {
            if (punks[i] == NULL)
                throw runtime_error("NULL returned instead of object");
            punks[i]->AddRef();
            punks[i]->Release();
        }

        empty_enum_test(e);

        // Test uniqueness
        if (punks[0] == punks[1])
            throw runtime_error("Same object returned twice");
        if (punks[0] == punks[2])
            throw runtime_error("Same object returned twice");
        if (punks[2] == punks[1])
            throw runtime_error("Same object returned twice");

        release_pointer_array(punks);
    }
    catch(const exception&)
    {
        release_pointer_array(punks);
        throw;
    }
}

/**
 * Populated stl_enumeration_t.
 */
BOOST_AUTO_TEST_CASE( stl_enumeration_t_non_empty )
{
    typedef std::vector< com_ptr<IUnknown> > collection_type;
    collection_type coll = test_collection();
    com_ptr<IEnumUnknown> e = new stl_enumeration_t<
        IEnumUnknown, collection_type, IUnknown*>(coll);
    
    enum_test(e);
    e->Reset();
    enum_chunk_test(e);
}


/**
 * Populated smart_enumeration.
 */
BOOST_AUTO_TEST_CASE( smart_enumeration_non_empty )
{
    typedef std::auto_ptr< std::vector< com_ptr<IUnknown> > > smart_collection;
    smart_collection coll(
        new std::vector< com_ptr<IUnknown> >(test_collection()));
    com_ptr<IEnumUnknown> e = new smart_enumeration<
        IEnumUnknown, smart_collection, IUnknown*>(coll);
    coll.reset(); // force enum to be the only owner (should happen
    // regardless with an auto_ptr)
    enum_test(e);
    e->Reset();
    enum_chunk_test(e);
}

BOOST_AUTO_TEST_SUITE_END()