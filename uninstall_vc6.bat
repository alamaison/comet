del "%MSDEVDIR%\Template\comet.awx"
@bin\regpath hive current_user key "Software\Microsoft\DevStudio\6.0\Build System\Components\Platforms\Win32 (x86)\Directories" value "Include Dirs" remove include
@bin\regpath hive current_user key "Software\Microsoft\DevStudio\6.0\Build System\Components\Platforms\Win32 (x86)\Directories" value "Path Dirs" remove bin
