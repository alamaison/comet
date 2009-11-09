// cometaw.cpp : implementation file
//

#include "stdafx.h"
#include "comet.h"
#include "cometaw.h"
#import "C:\Program Files\Microsoft Visual Studio\Common\MSDev98\bin\ide\devbld.pkg"

#ifdef _PSEUDO_DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// This is called immediately after the custom AppWizard is loaded.  Initialize
//  the state of the custom AppWizard here.
void CCometAppWiz::InitCustomAppWiz()
{
	// There are no steps in this custom AppWizard.
	SetNumberOfSteps(0);

	// Add build step to .hpj if there is one
	m_Dictionary[_T("HELP")] = _T("1");

	// Inform AppWizard that we're making a DLL.
	m_Dictionary[_T("PROJTYPE_DLL")] = _T("1");

	GUID guid;

	CoCreateGuid(&guid);

	TCHAR buffer[1024];

	_stprintf(buffer, _T("%08lX-%04X-%04x-%02X%02X-%02X%02X%02X%02X%02X%02X"), 
		// first copy...
			guid.Data1, guid.Data2, guid.Data3, 
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] );

	m_Dictionary[_T("TLIB_GUID")] = buffer;
	// TODO: Add any other custom AppWizard-wide initialization here.
}

// This is called just before the custom AppWizard is unloaded.
void CCometAppWiz::ExitCustomAppWiz()
{
	// TODO: Add code here to deallocate resources used by the custom AppWizard
}

_bstr_t g_root;

// This is called when the user clicks "Create..." on the New Project dialog
CAppWizStepDlg* CCometAppWiz::Next(CAppWizStepDlg* pDlg)
{
	ASSERT(pDlg == NULL);	// By default, this custom AppWizard has no steps

	// Set template macros based on the project name entered by the user.

	// Get value of $$root$$ (already set by AppWizard)
	CString strRoot;
	m_Dictionary.Lookup(_T("root"), strRoot);

	g_root = strRoot;
	
	// Set value of $$Doc$$, $$DOC$$
	CString strDoc = strRoot.Left(6);
	m_Dictionary[_T("Doc")] = strDoc;
	strDoc.MakeUpper();
	m_Dictionary[_T("DOC")] = strDoc;

	// Set value of $$MAC_TYPE$$
	strRoot = strRoot.Left(4);
	int nLen = strRoot.GetLength();
	if (strRoot.GetLength() < 4)
	{
		CString strPad(_T(' '), 4 - nLen);
		strRoot += strPad;
	}
	strRoot.MakeUpper();
	m_Dictionary[_T("MAC_TYPE")] = strRoot;

	// Return NULL to indicate there are no more steps.  (In this case, there are
	//  no steps at all.)
	return NULL;
}

void add_compiler_settings(DSProjectSystem::IConfigurationPtr pConfig, _bstr_t settings)
{
	try {
		pConfig->AddToolSettings(L"cl.exe", settings);
		return;
	} catch (const _com_error &) {}
	pConfig->AddToolSettings(L"xicl6.exe", settings);
}

void remove_compiler_settings(DSProjectSystem::IConfigurationPtr pConfig, _bstr_t settings)
{
	try {
		pConfig->RemoveToolSettings(L"cl.exe", settings);
		return;
	} catch (const _com_error &) {}
	pConfig->RemoveToolSettings(L"xicl6.exe", settings);
}

void add_linker_settings(DSProjectSystem::IConfigurationPtr pConfig, _bstr_t settings)
{
	try {
		pConfig->AddToolSettings(L"link.exe", settings);
		return;
	} catch (const _com_error &) {}
	pConfig->AddToolSettings(L"xilink6.exe", settings);
}

void remove_linker_settings(DSProjectSystem::IConfigurationPtr pConfig, _bstr_t settings)
{
	try {
		pConfig->RemoveToolSettings(L"link.exe", settings);
		return;
	} catch (const _com_error &) {}
	pConfig->RemoveToolSettings(L"xilink6.exe", settings);
}

void CCometAppWiz::CustomizeProject(IBuildProject* pProject)
{

	
   using namespace DSProjectSystem;

      long lNumConfigs;
      IConfigurationsPtr pConfigs;
      IBuildProjectPtr pProj;
      // Needed to convert IBuildProject to the DSProjectSystem namespace
      pProj.Attach((DSProjectSystem::IBuildProject*)pProject, true);
//	  pProj = (DSProjectSystem::IBuildProject*)pProject;

      pProj->get_Configurations(&pConfigs);
      pConfigs->get_Count(&lNumConfigs);
      //Get each individual configuration
      for (long j = 1 ; j < lNumConfigs+1 ; j++)
      {
         _bstr_t varTool;
         _bstr_t varSwitch;
         IConfigurationPtr pConfig;
         _variant_t varj = j;

         pConfig = pConfigs->Item(varj);

		pConfig->AddToolSettings(L"mfc", L"0", varj);
		pConfig->AddFileSettings(L"std.cpp", L"/Yc\"std.h\"", varj);
		remove_compiler_settings(pConfig, "/D_MBCS");
		pConfig->AddFileSettings(g_root + L".cpp", L"/Yu\"std.h\"");

		_bstr_t idl_file = g_root + L".idl";
		_bstr_t output = g_root + L"lib.h\n" + g_root + L".tlb";
		pConfig->AddCustomBuildStepToFile(idl_file, L"idl2h $(InputName)", output, L"Compiling Type Library.", varj);

		pConfig->AddCustomBuildStep("regsvr32 /s /c \"$(TargetPath)\"\necho regsvr32 exec. time > \"$(OutDir)\\regsvr32.trg\"", 
			"$(OutDir)\\regsvr32.trg", 
			"Performing registration");

		remove_compiler_settings(pConfig, "/D_USRDLL");
		remove_compiler_settings(pConfig, "/O2");
		add_compiler_settings(pConfig, L"/Yu\"std.h\"");

		if(pConfig->Name == g_root + " - Win32 Release")
		{
			remove_compiler_settings(pConfig, "/D_DEBUG /ZI /GZ");
			add_compiler_settings(pConfig, "/O1 /DNDEBUG /MD");

			//pConfig->RemoveToolSettings("link.exe", "/debug");
			remove_linker_settings(pConfig, "/debug");

			//pConfig->AddToolSettings("link.exe", "/incremental:no /opt:nowin98");
			add_linker_settings(pConfig, "/incremental:no /opt:nowin98");
		}
		else
		{
			remove_compiler_settings(pConfig, "/O1 /DNDEBUG");
			add_compiler_settings(pConfig, "/D_DEBUG /MDd");
		}

		pConfig->MakeCurrentSettingsDefault();

/*         varTool   = "rc.exe";
         varSwitch = "/d \"_AFXDLL\"";
         pConfig->RemoveToolSettings(varTool, varSwitch, varj);

         // OPTIONAL
         // Add Libs that MFC headers would have pulled in automatically
         // Feel free to customize this listing to your tastes
         varTool = "link.exe";
         varSwitch = "kernel32.lib user32.lib gdi32.lib winspool.lib "
                     "comdlg32.lib advapi32.lib shell32.lib ole32.lib "
                     "oleaut32.lib uuid.lib odbc32.lib odbccp32.lib";
         pConfig->AddToolSettings(varTool, varSwitch, varj);*/
      }
   
/*	IConfigurations* pConfigs;
	pProject->get_Configurations(&pConfigs);

	long n;
	pConfigs->get_Count(&n);


	for (long i=1; i<=n; ++i) {
		IConfiguration* pConfig;
		pConfigs->Item(_variant_t(i), &pConfig);


         _variant_t varj = i;

         _bstr_t varTool;
         _bstr_t varSwitch;
         
		
         varTool   = "cl.exe";
         varSwitch = "/D \"_AFXDLL\"";
         pConfig->RemoveToolSettings(varTool, varSwitch, varj);

         varTool   = "cl.exe";
         varSwitch = "/D \"foockup\"";
         pConfig->AddToolSettings(varTool, varSwitch, varj);

*/
/*		_bstr_t idl_file = g_root + _bstr_t(L".idl");
		_bstr_t output = g_root + _bstr_t(L"lib.h");

		pConfig->AddCustomBuildStepToFile(idl_file, L"idl2h $(InputPath) $(TargetName)", output, L"Compiling Type Library.", dummy);

		pConfig->AddToolSettings(L"mfc", L"0", dummy);

		pConfig->AddToolSettings(L"link.exe", 
			L"kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib",
			dummy);

		pConfig->AddFileSettings(L"std.cpp", L"/Yu\"std.h\"", dummy);

		pConfig->RemoveToolSettings(L"cl.exe", L"/GX", dummy);
//		pConfig->AddToolSettings(L"cl.exe", L"/Yu\"std.h\"", dummy);
*/ /*
		pConfig->Release();
	}

	pConfigs->Release();*/

	// TODO: Add code here to customize the project.  If you don't wish
	//  to customize project, you may remove this virtual override.
	
	// This is called immediately after the default Debug and Release
	//  configurations have been created for each platform.  You may customize
	//  existing configurations on this project by using the methods
	//  of IBuildProject and IConfiguration such as AddToolSettings,
	//  RemoveToolSettings, and AddCustomBuildStep. These are documented in
	//  the Developer Studio object model documentation.

	// WARNING!!  IBuildProject and all interfaces you can get from it are OLE
	//  COM interfaces.  You must be careful to release all new interfaces
	//  you acquire.  In accordance with the standard rules of COM, you must
	//  NOT release pProject, unless you explicitly AddRef it, since pProject
	//  is passed as an "in" parameter to this function.  See the documentation
	//  on CCustomAppWiz::CustomizeProject for more information.
}


// Here we define one instance of the CCometAppWiz class.  You can access
//  m_Dictionary and any other public members of this class through the
//  global Cometaw.
CCometAppWiz Cometaw;

