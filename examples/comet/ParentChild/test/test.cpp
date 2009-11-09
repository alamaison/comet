#include "..\CometExampleParentChild.h"

using namespace comet;
using namespace CometExampleParentChild;

void main()
{
	auto_coinit ac;

	com_ptr<IParent> parent = Demo::create();

	// Grab child pointer
	com_ptr<IChild> child = parent->GetChildA();

	// Releaes parent pointer
	parent = 0;

	// Get parent pointer
	parent = child->GetParent();

	// Release parent pointer
	parent = 0;

	// Release child pointer (notice that both children and parent are destroyed at this point)
	child = 0;
}