#pragma once

class Child;

typedef coclass_implementation<Demo> TDemo;

template<>
class coclass_implementation<Demo> : public coclass<Demo>
{
	typedef std::map<std::wstring, boost::shared_ptr<Child> > MAP;
	MAP children_;
public:
    long GetCount() { return children_.size(); }
    com_ptr<::IEnumVARIANT> EnumChildren();
    com_ptr<IChild> GetChild(const std::wstring& idx);

	com_ptr<IChild> AddChild(const std::wstring& idx);
};

class Child : public embedded_object<TDemo, IChildImpl<Child> >
{
	variant_t val_;

	int x_;
public:
	Child(TDemo* p) : base_class(p) {}

	com_ptr<IParent> GetParent() { return get_parent(); }

	variant_t GetValue() { return val_; }
	void PutValue(const variant_t& val) { val_ = val; }
};
