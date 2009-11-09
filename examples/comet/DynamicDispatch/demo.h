#pragma once

#include <comet/dispatch.h>

typedef coclass_implementation<Demo> CDemo;

template<>
class coclass_implementation<Demo> : public coclass<Demo>
{
	typedef std::map<std::wstring, variant_t> MAP;
	MAP map_; 
public:
	variant_t AddOne(const variant_t& v) { return int(v) + 1; }
	variant_t GetMeaningOfTheUniverse() { return 42; }

	variant_t GetItem(const variant_t& idx) 
	{ 
		MAP::const_iterator it = map_.find(idx);
		if (it == map_.end()) throw std::runtime_error("No such item.");
		return it->second;
	}

	void PutItem(const variant_t& idx, const variant_t& val)
	{
		map_[idx] = val;
	}

	coclass_implementation()
	{
		add_method(L"AddOne", &CDemo::AddOne);
		add_get_property(L"MeaningOfTheUniverse", &CDemo::GetMeaningOfTheUniverse);

		add_put_property(L"Item", &CDemo::PutItem);
		add_get_property(L"Item", &CDemo::GetItem);
	}

	static const TCHAR* get_progid() { return _T("CometExampleDynamicDispatch.Demo"); }
};