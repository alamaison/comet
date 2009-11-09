#include <iostream>

#include <comet/dispatch.h>
#include <comet/util.h>

using namespace comet;

void main()
{
	auto_coinit ac;

	try {
		com_ptr<IDispatch> dp( L"CometExampleDynamicDispatch.Demo" );
		
		int x = dp->call(L"AddOne", 1);

		int the_answer = dp->get(L"MeaningOfTheUniverse");
		dp->put(L"Item", L"MyPlace", the_answer);

		int the_answer2 = dp->get(L"Item", L"MyPlace");
	}
	catch (com_error& err)
	{
		std::cout << err.what();
	}

}