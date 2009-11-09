#pragma once
#pragma warning( disable : 4355 )

typedef coclass_implementation<Demo> TDemo;

class Child : public embedded_object<TDemo, IChildImpl<Child> >
{
public:
	Child(TDemo* parent) : base_class(parent) 
	{
		OutputDebugString("Child constructed\n");
	}

	~Child() 
	{ 
		OutputDebugString("Child destroyed\n");
	}

    inline com_ptr<IParent> GetParent();
};

template<>
class coclass_implementation<Demo> : public coclass<Demo>
{
	Child childA_;
	Child* childB_;
	std::auto_ptr<Child> childC_;
	bool f_;
public:
	coclass_implementation() : childA_(this), childB_(new Child(this)), childC_(new Child(this)), f_(false)
	{
		OutputDebugString("Parent constructed\n");
	}

	~coclass_implementation()
	{
		// Remember to manually destroy childB_
		delete childB_;

		OutputDebugString("Parent destroyed\n");
	}

    com_ptr<IChild> GetChildA() { return &childA_; }
    com_ptr<IChild> GetChildB() { return childB_; }
    com_ptr<IChild> GetChildC() { return childC_.get(); }
};

inline com_ptr<IParent> Child::GetParent() 
{
	return get_parent();
}