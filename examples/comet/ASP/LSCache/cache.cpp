#include "std.h"
#include "cache.h"
#include <process.h>
#include <comet/enum.h>

variant_t CCache::GetValue(const std::wstring& idx, const std::wstring& key)
{
	auto_cs lock(cs_);
	MAP::iterator it = map_.find(idx);
	if (it == map_.end()) throw std::runtime_error("Item not found");
	it->second.timestamp = datetime_t::now();

	COMET_ASSERT( map_.size() == heap_.size());

	SUBMAP::iterator it2 = it->second.submap.find(key);
	if (it2 == it->second.submap.end()) throw std::runtime_error("key not found");
	return it2->second;
}

long CCache::GetCount()
{
	auto_cs lock(cs_);
	return map_.size();
}

void CCache::Remove(const std::wstring& idx)
{
	auto_cs lock(cs_);
	map_.erase(idx);
}

void CCache::PutValue(const std::wstring& idx, const std::wstring& key, const variant_t& val)
{
	if (val.is_object()) {
		com_ptr<IUnknown> unk = com_cast(val);
		if (unk) if (!is_object_aggregating_ftm( unk )) {
			throw std::runtime_error("Cache can only store apartment neutral objects");
		}
	}

	auto_cs lock(cs_);
	datetime_t now = datetime_t::now();
	MAP::iterator it = map_.find(idx);
	if (it == map_.end()) 
	{
		map_[idx].submap[key] = val;
		map_[idx].timestamp = now;
	}
	else
	{
		// delete old time stamp
		heap_.erase( it->second.timestamp );

		// update existing value
		it->second.timestamp = now;
		it->second.submap[key] = val;

	}

	// add time stamp
	heap_.insert( std::make_pair(now, idx) );
}

com_ptr<IEnumVARIANT> CCache::EnumerateKeys()
{
	auto_cs lock(cs_);
	return create_enum(map_, this, std::select1st<MAP::value_type>());
}

long CCache::GetExpiry()
{
	auto_cs lock(cs_);
	return expiry_;
}

void CCache::PutExpiry(long minutes)
{
	auto_cs lock(cs_);
	expiry_ = minutes;

	// force gc
	SetEvent(event_);	
}

long CCache::GetMaxSize()
{
	auto_cs lock(cs_);
	return max_size_;
}

void CCache::PutMaxSize(long x)
{
	auto_cs lock(cs_);
	max_size_ = x;

	// force gc
	SetEvent(event_);
}

namespace {
	void thread_routine(void* This)
	{
		auto_coinit x(COINIT_MULTITHREADED);
		static_cast<CCache*>(This)->gc_thread();
	}
}

CCache::coclass_implementation()
{
	event_ = auto_attach( CreateEvent(0, FALSE, FALSE, 0) );

	// expiry set to 2 hours per default
	expiry_ = 120;

	// no limit on the size of the cache
	max_size_ = -1;

	keep_running_ = true;
	DWORD res = _beginthread( thread_routine, 0, this);
	if (res == -1 || res == 0) throw std::runtime_error("Failed to start gc thread.");
	thread_ = auto_attach( (HANDLE)res );
}

CCache::~coclass_implementation()
{
	auto_cs lock(cs_);
	keep_running_ = false;
	SetEvent(event_);
	DWORD res = WaitForSingleObject( thread_, 1000 );
	COMET_ASSERT( res != WAIT_TIMEOUT );

	// Did the thread close down nicely?
	if (res != WAIT_TIMEOUT) 
	{
		// if so, thread_ handle has already been released
		thread_.detach_handle();
	} 
	else 
	{
		// otherwise, attempt terminating the thread, handle will be closed in ~auto_handle.
		TerminateThread(thread_, 0);
	}
}

void CCache::gc_thread()
{
	for (;;)
	{
		// activate gc every 60 seconds
		DWORD res = WaitForSingleObject( event_, 60*1000 );
		if (res != WAIT_TIMEOUT && !keep_running_) return;
		
		gc();
	}
}

void CCache::gc()
{
	auto_cs lock(cs_);

	datetime_t now = datetime_t::now();

	HEAP::iterator it = heap_.begin();

	long elements_to_be_removed = heap_.size() - max_size_;
	if (max_size_ >= 0 && elements_to_be_removed > 0) std::advance(it, elements_to_be_removed);

	if (expiry_ >= 0) while (it != heap_.end() && (now - it->first).as_minutes() >= expiry_) ++it;

	HEAP::iterator it2 = heap_.begin();
	for (;it2 != it; ++it2) map_.erase( it2->second );
	heap_.erase(heap_.begin(), it);
}
