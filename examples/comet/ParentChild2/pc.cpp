#include "std.h"
#include "pc.h"

#include <comet/enum.h>

template<typename T> struct shared_ptr_converter_select2nd : std::unary_function< com_ptr<IUnknown>, T>
{
	com_ptr<IUnknown> operator()(const T& x) { return impl::cast_to_unknown(x.second.get()); }
};

template<typename T>
typename T::const_reference at(const T& t, const typename T::key_type& k)
{
	T::const_iterator it = t.find(k);
	if (it == t.end()) throw std::runtime_error("Index out of range");
	return *it;
}

com_ptr<IChild> TDemo::GetChild(const std::wstring& idx)
{
	return at(children_, idx).second.get();
}

com_ptr<IEnumVARIANT> TDemo::EnumChildren()
{
	return create_enum(children_, this, shared_ptr_converter_select2nd<MAP::value_type>());
}

com_ptr<IChild> TDemo::AddChild(const std::wstring& idx)
{
	if (children_.find(idx) != children_.end()) throw std::runtime_error("Child already exists");

	boost::shared_ptr<Child> p(new Child(this));
	children_[idx] = p;

	return p.get();
}
