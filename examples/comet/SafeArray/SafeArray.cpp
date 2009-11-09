#include "std.h"

typedef com_server< CometExampleSafeArray::type_library > SERVER;

// Declare and implement DllMain, DllGetClassObject, DllRegisterServer,
// DllUnregisterServer and DllCanUnloadNow.
// These are implemented by simple forwarding onto a static method of
// the same name on SERVER:
// i.e. DllMain simply calls SERVER::DllMain.
COMET_DECLARE_DLL_FUNCTIONS(SERVER)
