#pragma once

template<>
class coclass_implementation<SASort> : public coclass<SASort>
{
public:
    void Sort(safearray_t<variant_t>& sa)
	{
		std::sort(sa.begin(), sa.end());
	}
};
