#include "Objects.h"
#include "../Hooking/Hooks.h"

ObjectTable& Helpers::GetObjectTable()
{
	return **reinterpret_cast<ObjectTable**>(Hooks::o.ObjectTable);
}

PlayerTable& Helpers::GetPlayerTable()
{
	return **reinterpret_cast<PlayerTable**>(Hooks::o.PlayerTable);
}

BaseDynamicObject* Helpers::GetDynamicObject(HaloID& id)
{
	if (id.index == 0xffff || id.id == 0xffff) return nullptr;
	ObjectTable& table = GetObjectTable();
	// Dropping/picking up can recycle an index for a different object. The salt
	// is part of the identity; a stale index must never resolve to that new gun.
	if (!table.elements || id.index >= table.maxElements || table.elements[id.index].id != id.id)
	{
		return nullptr;
	}

	return table.elements[id.index].dynamicObject;
}

bool Helpers::GetLocalPlayerID(HaloID& outID)
{
	PlayerTable& table = GetPlayerTable();

	if (table.currentSize == 0)
	{
		return false;
	}

	outID = table.elements->objectID;
	return true;
}

BaseDynamicObject* Helpers::GetLocalPlayer()
{
	// TODO: Test this in MP, it may not be that the first player is the local one

	HaloID playerID;
	if (!GetLocalPlayerID(playerID))
	{
		return nullptr;
	}

	return GetDynamicObject(playerID);
}
