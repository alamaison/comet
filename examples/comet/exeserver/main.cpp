// Implementation of WinMain

#include "std.h"
#include "ExeTest.h"

using namespace comet;
using namespace ExeTest;

template<>
class coclass_implementation<Test> : public coclass<Test>
{
public:
	void HelloWorld()
	{
		MessageBox(0, "Hello, World!", 0, 0);
	}
};

/////////////////////////////////////////////////////////////////////////////
//
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, 
										  HINSTANCE /*hPrevInstance*/, 
										  LPTSTR lpCmdLine, 
										  int /*nShowCmd*/)
{
	using namespace comet;

#if _WIN32_WINNT >= 0x0400 & defined(COMET_FREE_THREADED)
	auto_coinit coinit(COINIT_MULTITHREADED);
#else
	auto_coinit coinit;
#endif

	exe_server<type_library> server(hInstance);

	bool should_run = true;
	HRESULT result = S_OK;
	tstring token;
	cmd_line_parser parser(lpCmdLine);
	cmd_line_parser::kind_t kind;

	while ((kind = parser.get_next_token(token)) != cmd_line_parser::Done)
	{
		if (kind == cmd_line_parser::Option)
		{
			if (lstrcmpi(token.c_str(), _T("UnregServer"))==0)
			{
				server.unregister_server();
				should_run = false;
				break;
			}
			else if (lstrcmpi(token.c_str(), _T("RegServer"))==0)
			{
				server.register_server();
				should_run = false;
				break;
			}
		}
	}

//	if (should_run)
		result = server.run();

	return result;
}

