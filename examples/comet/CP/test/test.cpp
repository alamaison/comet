// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <set>
#include <comet/threading.h>
#include <exception>

#include <comet/uuid.h>
#include <comet/bstr.h>

class Sink : public sink_impl< DStackEventsImpl<Sink> >
{
public:
	void OnChanged(Ees& e)
	{
		OutputDebugString("OnChanged\n");
		e = E_1;
	}
};

int main(int argc, char* argv[])
{
	auto_coinit ac;

	safearray_t<TName> sa3;
	
	std::vector<TName> v;
	sa3.assign(v.begin(), v.end());


	safearray_t<variant_t> sa( 3, 1 );
	sa[1] = 1.0;
	sa[2] = L"3.0";
	sa[3] = 2.0;

	const safearray_t<variant_t>& sa2 = sa;
	double m = *std::max_element(sa2.begin(), sa2.end());

	com_ptr<IFoo> foo = Foo::create();

	com_ptr<IStack> stack = Stack::create();

	Sink my_sink;
	my_sink.advise(stack);

	TName name = stack->Test4a();
	TName name2( name );
	TName name3; name3 = name;
	TName name4;
	name4 = name;

	bool b = false;
	stack->Test11(b);

	{
	bstr_t s1, s2, s3;
	stack->Test8a(s1, s2, s3);
	}

	{
	safearray_t<bstr_t> sb(2, 1);
	try {
		stack->Test6a(sb);
	}
	catch (...)
	{
	}
	}

	{
	safearray_t<TName> sa(2, 1);
//	stack->Test7b(sa);
	}

	stack->Push(0);
	stack->Pop();
	
	return 0;
}

