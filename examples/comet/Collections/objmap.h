#pragma once

#pragma warning( disable : 4355 )

class ObjectMapKeys : public embedded_object< coclass_implementation<ObjectMap>, IEnumImpl<ObjectMapKeys> >
{
public:
	ObjectMapKeys(coclass_implementation<ObjectMap>* parent) : base_class(parent) {}
	com_ptr<::IEnumVARIANT> Enum();
};

class ObjectMapValues : public embedded_object< coclass_implementation<ObjectMap>, IEnumImpl<ObjectMapValues> >
{
public:
	ObjectMapValues(coclass_implementation<ObjectMap>* parent) : base_class(parent) {}
	com_ptr<::IEnumVARIANT> Enum();
};

template<>
class coclass_implementation<ObjectMap> : public coclass<ObjectMap>
{
	typedef std::map< std::wstring, com_ptr<IDispatch> > map_t;
	map_t map_;

	ObjectMapKeys map_keys_;
	ObjectMapValues map_values_;
public:
	coclass_implementation() : map_keys_(this), map_values_(this) {}

    com_ptr<IDispatch> GetItem(const std::wstring& idx)
	{
		map_t::const_iterator it = map_.find(idx);
		if (it == map_.end()) throw std::runtime_error("No such element");
		return it->second;
	}

    void PutItem(const std::wstring& idx, const com_ptr<IDispatch>& val)
	{
		map_[idx] = val;
	}

    long GetCount() { return map_.size(); }

    com_ptr<IEnum> GetKeys() { return &map_keys_; }
    com_ptr<IEnum> GetValues() { return &map_values_; }

	com_ptr<::IEnumVARIANT> EnumKeys()
	{
		return create_enum(map_, this, std::select1st<map_t::value_type>());
	}

	com_ptr<::IEnumVARIANT> EnumValues()
	{
		return create_enum(map_, this, std::select2nd<map_t::value_type>());
	}
};
