#include "std.h"
#include "objmap.h"

com_ptr<IEnumVARIANT> ObjectMapKeys::Enum()
{
	return get_parent()->EnumKeys();
}

com_ptr<IEnumVARIANT> ObjectMapValues::Enum()
{
	return get_parent()->EnumValues();
}