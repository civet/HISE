/** Autogenerated Main.cpp. */

#include "JuceHeader.h"

#include "includes.h"


namespace project
{
	struct Factory : public dll::PluginFactory
	{
		Factory()
		{
			registerNode<project::PeakClearAdd1>();
			registerNode<project::TableTest1>();
		}
	};
}

project::Factory f;

DLL_EXPORT int getNumNodes()
{
	return f.getNumNodes();
}

DLL_EXPORT void* createNode(int index)
{
	return nullptr;
}

DLL_EXPORT size_t getNodeId(int index, char* t)
{
	return HelperFunctions::writeString(t, f.getId(index).getCharPointer());
}

DLL_EXPORT void deInitOpaqueNode(scriptnode::OpaqueNode* n)
{
	n->callDestructor();
}

DLL_EXPORT void initOpaqueNode(scriptnode::OpaqueNode* n, int index)
{
	f.initOpaqueNode(n, index);
}

DLL_EXPORT void deleteNode(void* obj)
{
	delete obj;
}