
function OnFinish(selProj, selObj)
{
	try
	{
		var strProjectPath = wizard.FindSymbol('PROJECT_PATH');
		var strProjectName = wizard.FindSymbol('PROJECT_NAME');

		selProj = CreateCustomProject(strProjectName, strProjectPath);
		AddConfig(selProj, strProjectName);
		AddFilters(selProj);

		var InfFile = CreateCustomInfFile();
		AddFilesToCustomProj(selProj, strProjectName, strProjectPath, InfFile);
		PchSettings(selProj);
		InfFile.Delete();

		selProj.Object.Save();
	}
	catch(e)
	{
		if (e.description.length != 0)
			SetErrorInfo(e);
		return e.number
	}
}

function CreateCustomProject(strProjectName, strProjectPath)
{
	try
	{
		var strProjTemplatePath = wizard.FindSymbol('PROJECT_TEMPLATE_PATH');
		var strProjTemplate = '';
		strProjTemplate = strProjTemplatePath + '\\default.vcproj';

		var Solution = dte.Solution;
		var strSolutionName = "";
		if (wizard.FindSymbol("CLOSE_SOLUTION"))
		{
			Solution.Close();
			strSolutionName = wizard.FindSymbol("VS_SOLUTION_NAME");
			if (strSolutionName.length)
			{
				var strSolutionPath = strProjectPath.substr(0, strProjectPath.length - strProjectName.length);
				Solution.Create(strSolutionPath, strSolutionName);
			}
		}

		var strProjectNameWithExt = '';
		strProjectNameWithExt = strProjectName + '.vcproj';

		var oTarget = wizard.FindSymbol("TARGET");
		var prj;
		if (wizard.FindSymbol("WIZARD_TYPE") == vsWizardAddSubProject)  // vsWizardAddSubProject
		{
			var prjItem = oTarget.AddFromTemplate(strProjTemplate, strProjectNameWithExt);
			prj = prjItem.SubProject;
		}
		else
		{
			prj = oTarget.AddFromTemplate(strProjTemplate, strProjectPath, strProjectNameWithExt);
		}
		return prj;
	}
	catch(e)
	{
		throw e;
	}
}

function AddFilters(proj)
{
	try
	{
		// Add the folders to your project
		proj.Object.AddFilter('Source Files').Filter = "cpp;idl;rc;def";
		proj.Object.AddFilter('Header Files').Filter = "h";
	}
	catch(e)
	{
		throw e;
	}
}

function AddConfig(proj, strProjectName)
{
	try
	{
		var is_unicode = wizard.FindSymbol('UNICODE');
		var is_dll = wizard.FindSymbol('DLL_LINK');
		var ppro = wizard.FindSymbol('OPTIMIZE_PPRO');
	
		var config = proj.Object.Configurations('Debug');
		config.ConfigurationType = typeDynamicLibrary;
		config.IntermediateDirectory = 'Debug';
		config.OutputDirectory = 'Debug';
		
		if (is_unicode) config.CharacterSet = charSetUnicode;

		var CLTool = config.Tools('VCCLCompilerTool');
		CLTool.UsePrecompiledHeader = pchUseUsingSpecific;
		CLTool.PrecompiledHeaderThrough = "std.h";
		
		if (is_dll) 
			CLTool.RuntimeLibrary = rtMultiThreadedDebugDLL;		
		else
			CLTool.RuntimeLibrary = rtMultiThreadedDebug;

		CLTool.PreprocessorDefinitions = "_WINDOWS;_NDEBUG";
		CLTool.MinimalRebuild = true;
		CLTool.TreatWChar_tAsBuiltInType = true;
		CLTool.DebugInformationFormat = debugEditAndContinue;
		CLTool.Optimization = optimizeDisabled;
		CLTool.BasicRuntimeChecks = runtimeBasicCheckAll;
		CLTool.EnableFunctionLevelLinking = true;
		CLTool.WarningLevel = warningLevel_3;
		
		var LinkTool = config.Tools('VCLinkerTool');
		LinkTool.LinkIncremental = linkIncrementalYes;
		LinkTool.GenerateDebugInformation = true;

		var RCTool = config.Tools("VCResourceCompilerTool");
		RCTool.Culture = rcEnglishUS;
		RCTool.PreprocessorDefinitions = "_DEBUG";
		RCTool.AdditionalIncludeDirectories = "$(IntDir)";

		config = proj.Object.Configurations('Release');
		config.ConfigurationType = typeDynamicLibrary;
		config.IntermediateDirectory = 'Release';
		config.OutputDirectory = 'Release';
		config.WholeProgramOptimization = true;

		if (is_unicode) config.CharacterSet = charSetUnicode;

		var CLTool = config.Tools('VCCLCompilerTool');
		CLTool.UsePrecompiledHeader = pchUseUsingSpecific;
		CLTool.PrecompiledHeaderThrough = "std.h";

		if (is_dll) 
			CLTool.RuntimeLibrary = rtMultiThreadedDLL;		
		else
			CLTool.RuntimeLibrary = rtMultiThreaded;
			
		CLTool.PreprocessorDefinitions= "_WINDOWS;NDEBUG";
		CLTool.InlineFunctionExpansion = expandOnlyInline;
		CLTool.MinimalRebuild = false;
		CLTool.TreatWChar_tAsBuiltInType = true;
		CLTool.DebugInformationFormat = debugEnabled;
		CLTool.Optimization = optimizeFull;
		CLTool.StringPooling = true;
		CLTool.ForceConformanceInForLoopScope = true;
		CLTool.WarningLevel = warningLevel_3;
		
		if (ppro)
			CLTool.OptimizeForProcessor = procOptimizePentiumProAndAbove;
		else
			CLTool.OptimizeForProcessor = procOptimizeBlended;

		var LinkTool = config.Tools('VCLinkerTool');
		LinkTool.GenerateDebugInformation = true;
		LinkTool.LinkIncremental = linkIncrementalNo;
		LinkTool.OptimizeForWindows98 = false;
		LinkTool.EnableCOMDATFolding = optFolding;
		LinkTool.OptimizeReferences = optReferences;

		RCTool = config.Tools("VCResourceCompilerTool");
		RCTool.Culture = rcEnglishUS;
		RCTool.PreprocessorDefinitions = "NDEBUG";
		RCTool.AdditionalIncludeDirectories = "$(IntDir)";
	}
	catch(e)
	{
		throw e;
	}
}

function PchSettings(proj)
{
	var f_std = proj.Object.Files("std.cpp");
	
	f_std.FileConfigurations("Debug").Tool.UsePrecompiledHeader = pchCreateUsingSpecific;
	f_std.FileConfigurations("Release").Tool.UsePrecompiledHeader = pchCreateUsingSpecific;

	var idlname = wizard.FindSymbol('PROJECT_NAME') + ".idl";
	var f_idl = proj.Object.Files(idlname);
	
	var config = proj.Object.Configurations('Debug');
	f_idl.FileConfigurations("Debug").Tool = config.Tools("VCCustomBuildTool");
	var tool = f_idl.FileConfigurations("Debug").Tool;
	
	tool.CommandLine = 'idl2h $(InputName) $(InputName)';
	var tlbname = wizard.FindSymbol('TLB_NAME');
	tool.Outputs = tlbname + ".h;" + tlbname + ".tlb";

	var config = proj.Object.Configurations('Release');
	f_idl.FileConfigurations("Release").Tool = config.Tools("VCCustomBuildTool");
	var tool = f_idl.FileConfigurations("Release").Tool;
	
	tool.CommandLine = 'idl2h $(InputName) $(InputName)';
	var tlbname = wizard.FindSymbol('TLB_NAME');
	tool.Outputs = tlbname + ".h;" + tlbname + ".tlb";
}

function DelFile(fso, strWizTempFile)
{
	try
	{
		if (fso.FileExists(strWizTempFile))
		{
			var tmpFile = fso.GetFile(strWizTempFile);
			tmpFile.Delete();
		}
	}
	catch(e)
	{
		throw e;
	}
}

function CreateCustomInfFile()
{
	try
	{
		var fso, TemplatesFolder, TemplateFiles, strTemplate;
		fso = new ActiveXObject('Scripting.FileSystemObject');

		var TemporaryFolder = 2;
		var tfolder = fso.GetSpecialFolder(TemporaryFolder);
		var strTempFolder = tfolder.Drive + '\\' + tfolder.Name;

		var strWizTempFile = strTempFolder + "\\" + fso.GetTempName();

		var strTemplatePath = wizard.FindSymbol('TEMPLATES_PATH');
		var strInfFile = strTemplatePath + '\\Templates.inf';
		wizard.RenderTemplate(strInfFile, strWizTempFile);

		var WizTempFile = fso.GetFile(strWizTempFile);
		return WizTempFile;
	}
	catch(e)
	{
		throw e;
	}
}

function GetTargetName(strName, strProjectName)
{
	try
	{
		var strTarget = strName;

		if (strName == 'readme.txt')
			strTarget = 'ReadMe.txt';

		if (strName == 'sample.txt')
			strTarget = 'Sample.txt';
			
		if (strName == 'main.idl')
			strTarget = strProjectName + ".idl";
			
		if (strName == 'main.rc')
			strTarget = strProjectName + ".rc";
			
		if (strName == 'main.def')
			strTarget = strProjectName + ".def";
			
		if (strName == 'main.cpp')
			strTarget = wizard.FindSymbol('PROJECT_NAME') + ".cpp";
			
		return strTarget; 
	}
	catch(e)
	{
		throw e;
	}
}

function AddFilesToCustomProj(proj, strProjectName, strProjectPath, InfFile)
{
	try
	{
		var projItems = proj.ProjectItems

		var strTemplatePath = wizard.FindSymbol('TEMPLATES_PATH');

		var strTpl = '';
		var strName = '';

		var strTextStream = InfFile.OpenAsTextStream(1, -2);
		while (!strTextStream.AtEndOfStream)
		{
			strTpl = strTextStream.ReadLine();
			if (strTpl != '')
			{
				strName = strTpl;
				var strTarget = GetTargetName(strName, strProjectName);
				var strTemplate = strTemplatePath + '\\' + strTpl;
				var strFile = strProjectPath + '\\' + strTarget;

				var bCopyOnly = false;  //"true" will only copy the file from strTemplate to strTarget without rendering/adding to the project
				var strExt = strName.substr(strName.lastIndexOf("."));
				if(strExt==".bmp" || strExt==".ico" || strExt==".gif" || strExt==".rtf" || strExt==".css")
					bCopyOnly = true;
				wizard.RenderTemplate(strTemplate, strFile, bCopyOnly);
				proj.Object.AddFile(strFile);
			}
		}
		strTextStream.Close();
	}
	catch(e)
	{
		throw e;
	}
}
