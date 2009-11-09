#include <comet/tlbinfo.h>

using namespace comet;

void test(com_ptr<ITypeInfo> x, com_ptr<ITypeLib> y)
{
	y->GetTypeInfoCount();
	y->GetTypeInfoOfGuid( uuid_t::create() );
	x->GetTypeAttr();
}