// comet.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include <afxdllx.h>
#include "comet.h"
#include "cometaw.h"

#ifdef _PSEUDO_DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static AFX_EXTENSION_MODULE CometDLL = { NULL, NULL };

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		TRACE0("COMET.AWX Initializing!\n");
		
		// Extension DLL one-time initialization
		AfxInitExtensionModule(CometDLL, hInstance);

		// Insert this DLL into the resource chain
		new CDynLinkLibrary(CometDLL);

		// Register this custom AppWizard with MFCAPWZ.DLL
		SetCustomAppWizClass(&Cometaw);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		TRACE0("COMET.AWX Terminating!\n");

		// Terminate the library before destructors are called
		AfxTermExtensionModule(CometDLL);
	}
	return 1;   // ok
}
