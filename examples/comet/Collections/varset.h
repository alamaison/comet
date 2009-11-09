#pragma once

template<>
class coclass_implementation<VariantSet> : public coclass<VariantSet>
{
	typedef std::set<variant_t> set_t;
	set_t set_;
public:
    long GetCount() { return set_.size(); }

    bool GetItem(const variant_t& var)
	{
		set_t::iterator it = set_.find( var );
		return it != set_.end();
	}

    void PutItem(const variant_t& var, bool newval)
	{
		if (newval)
			set_.insert( var );
		else
		{
			set_t::iterator it = set_.find( var );
			if (it == set_.end()) throw std::runtime_error("No such element");
			set_.erase(it);
		}
	}

    com_ptr<::IEnumVARIANT> Enum()
	{
		return create_enum( set_, this );
	}
};