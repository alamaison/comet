#pragma once

/*
 * Copyright © 2000, 2001 Sofus Mortensen, Michael Geddes
 *
 * This material is provided "as is", with absolutely no warranty 
 * expressed or implied. Any use is at your own risk. Permission to 
 * use or copy this software for any purpose is hereby granted without 
 * fee, provided the above notices are retained on all copies. 
 * Permission to modify the code and to distribute modified code is 
 * granted, provided the above notices are retained, and a notice that 
 * the code was modified is included with the above copyright notice. 
 */

//#define _STLP_NO_OWN_IOSTREAMS
// Make sure there is no stlport .dll dependence - either use the provide
// iostreams or use stlport's staticx library.
#if !defined(_STLP_NO_OWN_IOSTREAMS) && !defined(_STLP_USE_STATIC_LIB)
#define _STLP_USE_STATIC_LIB
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

#pragma warning( disable : 4786 )

#include <comdef.h>
#include <comutil.h>
#include <crtdbg.h>

#undef IMPLTYPEFLAG_FDEFAULT
#undef IMPLTYPEFLAG_FSOURCE
#undef IMPLTYPEFLAG_FRESTRICTED
#undef IMPLTYPEFLAG_FDEFAULTVTABLE

#undef IDLFLAG_NONE
#undef IDLFLAG_FIN
#undef IDLFLAG_FOUT
#undef IDLFLAG_FLCID
#undef IDLFLAG_FRETVAL

#undef PARAMFLAG_NONE
#undef PARAMFLAG_FIN
#undef PARAMFLAG_FOUT
#undef PARAMFLAG_FLCID
#undef PARAMFLAG_FRETVAL
#undef PARAMFLAG_FOPT
#undef PARAMFLAG_FHASDEFAULT
#undef PARAMFLAG_FHASCUSTDATA

#import <tlbinf32.dll> exclude("_DirectCalls")

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <list> 
#include <sstream>
#include <stack>
#include <set>
#include <string>
