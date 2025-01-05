#include "Version.h"
#include "../Hooking/Hooks.h"

char* Helpers::GetGameTypeString()
{
	return reinterpret_cast<char*>(Hooks::o.GameType);
}

char* Helpers::GetVersionString()
{
	return reinterpret_cast<char*>(Hooks::o.GameVersion);
}
