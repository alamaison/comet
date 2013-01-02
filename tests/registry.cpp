#include <boost/test/unit_test.hpp>

#define COMET_ASSERT_THROWS_ALWAYS
#include <comet/regkey.h>

using comet::regkey;

BOOST_AUTO_TEST_SUITE( registry_test )

// This tests for a bug we found where the assignment operator for key_base
// was missing.
BOOST_AUTO_TEST_CASE( regkey_assignment )
{
    regkey k1 = regkey(HKEY_LOCAL_MACHINE).open(
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ );
    regkey k2;
    k2 = k1;
    k1.close();
    // This will fail if it is the exact same bug that we ran into before.
    k2.flush();
}

// This tests for a VC++ 6.0 bug where the compiler-generated assignment
// operator for name_iterator was incorrect.
BOOST_AUTO_TEST_CASE( name_iterator_assignment )
{
    regkey::info_type::subkeys_type::iterator it;
    {
        regkey key = regkey(HKEY_LOCAL_MACHINE).open(
            "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            KEY_READ);
        regkey::subkeys_type subkeys = key.enumerate().subkeys();

        // Invoke assignment operator for name_iterator
        it = subkeys.begin();
        key.close();
        BOOST_CHECK_MESSAGE(
            it != subkeys.end(),
            "There are no subkeys - this is a very strange Windows "
            "installation!");
    }
    // Dereference the key iterator after everything else has been closed.
    *it;
}

BOOST_AUTO_TEST_SUITE_END()
