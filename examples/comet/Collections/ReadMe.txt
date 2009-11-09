========================================================================
                    COLLECTIONS DLL Server: 
========================================================================


The AppWizard has created this DLL COM server for you. 

This file contains a summary of what you will find in each of the files that
make up your project.

Collections.dsw
    This file (the project workspace file) contains information on the contents
    and organization of the project workspace. Other users can share the project
    workspace (.dsw) file, but they should export the makefiles locally.

    Files associated with the project workspace file are a project file (.dsp)
    for each project in the workspace and a workspace options file (.opt).

Collections.dsp
    This file (the project file) contains information at the project level and
    is used to build a single project or subproject. Other users can share the
    project (.dsp) file, but they should export the makefiles locally.

Collections.idl
	This is the .IDL file that is used to generate the type library and header file for your DLL.
	If you right click this file and select "Settings", you'll see that the app wizard has created
	a custom build step. The custom build step runs a batch file called "idl2h.bat", which runs
	first MIDL.EXE to generate Collections.tlb, and then tlb2h.exe to generate a header file
	called CollectionsLib.h.
	
	Note that MIDL is _only_ used to generate the type library file "Collections.tlb".
	It is not used to generate the C++ header file. tlb2h.exe generates the header file.

Collections.cpp
    This file is the main DLL source file that contains the definition of
    DllMain, DllGetClassObject,  DllCanUnloadNow, DllRegisterServer
	and DllUnregisterServer. These 5 functions are implemented by the
	macro COMET_DECLARE_DLL_FUNCTIONS. This
	macro is very simple - it forwards the declarations
	on to static methods of the same name on the class
	that you supply.

Collections.h
    This file is the main header file for the DLL.  It includes your 
    RESOURCE.H file.

Collections.rc
    This file is the resource script that is used to generate the resources
	embedded into your DLL. It refers to the type library generated from
	Collections.idl - so you will have to build the project once (and generate
	the type library) before you can edit the resources in the resource editor.

/////////////////////////////////////////////////////////////////////////////
Other Standard Files:

std.h, std.cpp
    These files are used to build a precompiled header (PCH) file
    named Collections.pch and a precompiled types file named std.obj.

Resource.h
    This is the standard header file, which defines new resource IDs.
    Visual C++ reads and updates this file.
