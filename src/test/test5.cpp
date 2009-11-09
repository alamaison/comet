#include <comet/uuid.h>

bool test(comet::uuid_t u1, comet::uuid_t u2)
{
	return u1 < u2;
}