#pragma once

using namespace comet;

typedef coclass_implementation<LSCache::Cache> CCache;

using namespace std;

template<>
class coclass_implementation<LSCache::Cache> : public coclass<LSCache::Cache, thread_model::Both, FTM>
{
private:

	typedef std::map<std::wstring, variant_t> SUBMAP;
	struct value_t 
	{
		SUBMAP submap;
		datetime_t timestamp;
	};

	typedef std::map<std::wstring, value_t > MAP;
	MAP map_;

	typedef std::multimap<datetime_t, std::wstring> HEAP;
	HEAP heap_;

	long expiry_;
	long max_size_;

	bool keep_running_;

	auto_handle event_;
	critical_section cs_;

	auto_handle thread_;
	
public:
	static const TCHAR* get_progid() { return _T("LSCache.Cache"); }

	coclass_implementation();
	~coclass_implementation();
	
	variant_t GetValue(const std::wstring&, const std::wstring&);
	void PutValue(const std::wstring&, const std::wstring&, const variant_t&);
	
	long GetCount();
	void Remove(const std::wstring&);
	com_ptr<IEnumVARIANT> EnumerateKeys();

	long GetExpiry();
	void PutExpiry(long minutes);
	
	long GetMaxSize();
	void PutMaxSize(long x);
	
	void gc_thread();
	void gc();
};