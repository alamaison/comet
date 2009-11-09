#pragma once

template<>
class coclass_implementation<VariantMap> : public coclass<VariantMap>
{
	typedef std::map<variant_t, variant_t> map_t;
	map_t map_;
public:
    variant_t GetItem(const variant_t& idx) 
	{ 
		map_t::const_iterator it = map_.find( idx );
		if (it == map_.end()) throw std::runtime_error("No such element");
		return it->second;
    }

    void PutItem(const variant_t& idx, const variant_t& val)
	{
		map_[idx] = val;
	}

    com_ptr<::IEnumVARIANT> EnumKeys()
	{
		return create_enum(map_, this, std::select2nd<map_t::value_type>());
	}
};