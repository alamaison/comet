#include <comet/ptr.h>
#include <comet/tlbinfo.h>
#include <comet/uuid.h>

#include <OAIdl.h> // ITypeInfo, ITypeLib

using comet::com_ptr;
using comet::uuid_t;

// Compile-time tests:

void test(com_ptr<ITypeInfo> x, com_ptr<ITypeLib> y)
{
    y->GetTypeInfoCount();
    y->GetTypeInfoOfGuid( uuid_t::create() );
    x->GetTypeAttr();
}
