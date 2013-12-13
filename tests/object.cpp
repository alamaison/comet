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

#include <comet/interface.h> // comtype<IPersist>
#include <comet/error.h> // com_error
#include <comet/handle_except.h> // COMET_CATCH_CLASS_INTERFACE_BOUNDARY
#include <comet/ptr.h> // com_ptr
#include <comet/server.h> // simple_object

#include <string>

using comet::com_error;
using comet::com_error_from_interface;
using comet::com_ptr;
using comet::simple_object;

using std::string;

BOOST_AUTO_TEST_SUITE( object_tests )

// Test that exceptions are correctly converted to Error info and back again
// when thrown inside a com object and caught with the comet exception handler
BOOST_AUTO_TEST_CASE( object_exceptions )
{
    // IPersist is easy to (badly) implement
    class error_object : public simple_object<IPersist>
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetClassID(CLSID* /*class_id_out*/)
        {
            try
            {
                throw com_error("Test error message", E_NOTIMPL);
            }
            COMET_CATCH_CLASS_INTERFACE_BOUNDARY("GetClassID", "error_object")
        }
    };

    com_ptr<IPersist> broken_object = new error_object();

    CLSID clsid = CLSID();
    HRESULT hr = broken_object->GetClassID(&clsid);

    BOOST_REQUIRE_EQUAL(hr, E_NOTIMPL);

    com_error error = com_error_from_interface(broken_object, hr);
    BOOST_CHECK_EQUAL(error.s_str(), "Test error message");
    BOOST_CHECK_EQUAL(error.source().s_str(), "error_object.GetClassID");
}

BOOST_AUTO_TEST_SUITE_END()