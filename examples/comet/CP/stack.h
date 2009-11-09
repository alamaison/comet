#pragma once

#include <comet/variant.h>

class CStack : public coclass< CometExampleCP::Stack, thread_model::Both, FTM >
{
	std::stack<variant_t> stack_;
	critical_section cs_;

public:
	CStack()
	{
	}

	~CStack()
	{
		OutputDebugString(_T("***destruction***"));
	}
	
	long GetSize() 
	{ 
		return stack_.size(); 
	}

	void Push(const variant_t& v) 
	{ 
		{	
			auto_cs lock(cs_);
			stack_.push(v); 
		}

		Ees e = E_1;
		connection_point.Fire_OnChanged(e);
	}

	variant_t Pop()
	{	
		variant_t rv;

		{
			auto_cs lock(cs_);
			rv = stack_.top();
			stack_.pop();
		}

		Ees e = E_2;
		connection_point.Fire_OnChanged(e);

		return rv;
	}

	int GetClassSize() { return sizeof(*this) - sizeof(stack_) - sizeof(cs_) - sizeof(long); }

	void Test1(const TName& v, const uuid_t& w)
	{
	}

	void Test2(const TName& v, const uuid_t& w)
	{
	}

	void Test3(TName& v, uuid_t& w)
	{
	}

	TName Test4a()
	{
		TName name;
		name.family = L"Mortensen";
		name.given = L"Sofus";
		return name;
	}

	uuid_t Test4b()
	{
		return uuid_t::create();
	}

	void Test5(TName& v, uuid_t& w)
	{
	}

	void Test6b(safearray_t<TName>& sa)
	{
	}

	void Test6a(safearray_t<bstr_t>& sa)
	{
		safearray_t<bstr_t> sa2;
		sa.swap( sa2 );
	}

	void Test7a(const safearray_t<bstr_t>& sa)
	{}

	void Test7b(const safearray_t<TName>& sa)
	{}

	bstr_t Test8a(const bstr_t&, bstr_t&, bstr_t&)
	{
		return "";
	}

	long Test8b(const long&, long&, long&)
	{
		return 0;
	}

	Ees Test8c(const Ees&, Ees&, Ees&)
	{
		return E_1;
	}

	void Test9a(const currency_t&, const datetime_t&) {}
	void Test9b(currency_t&, datetime_t&) {}
	void Test9c(currency_t&, datetime_t&) {}

	void Test10(bstr_t&, com_ptr<IDispatch>&, com_ptr<IUnknown>&, IStackPtr&, variant_t&, long&, float&)
	{}

	void Test11(bool& b)
	{
		b = true;
	}

	static TCHAR* get_progid() { return _T("CometExampleCP.Stack"); }
};

template<> class coclass_implementation< CometExampleCP::Stack > : public CStack {};


typedef coclass_implementation< CometExampleCP::Foo > TFoo;

template<>
class coclass_implementation< CometExampleCP::Foo > : public coclass< CometExampleCP::Foo >, 
													  public sink_impl< DStackEventsImpl<TFoo > >
{
public:
	void Test() 
	{
	}

	void OnChanged(Ees& e)
	{
		
	}

	coclass_implementation()
	{
		OutputDebugString(_T("Foo\n"));
	}

	~coclass_implementation() 
	{
		OutputDebugString(_T("~Foo\n"));
	}

};

template<>
class coclass_implementation< CometExampleCP::Bar > : public coclass< CometExampleCP::Bar, thread_model::Both, aggregates<Foo> >
{
public:
	coclass_implementation()
	{
		create_aggregate(this);
	}

	~coclass_implementation() 
	{
		OutputDebugString(_T("~Bar\n"));
	}

	void Test2() 
	{}
};