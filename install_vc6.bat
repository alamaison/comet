@echo Copying wizard...
copy bin\comet.awx "%MSDEVDIR%\Template"
@echo Modifying Tools search path for Visual Studio IDE
@echo Note: These changes will not have an effect if you have MSDEV.EXE
@echo open. If you have the IDE currently running, close it and run this
@echo batch file again.
@bin\regpath hive current_user key "Software\Microsoft\DevStudio\6.0\Build System\Components\Platforms\Win32 (x86)\Directories" value "Include Dirs" add include
@bin\regpath hive current_user key "Software\Microsoft\DevStudio\6.0\Build System\Components\Platforms\Win32 (x86)\Directories" value "Path Dirs" add bin
@echo Finished install. Now open up the IDE and check the settings for
@echo "Include Directories" and "Executable Files" under
@echo "Tools->Options->Directories"
