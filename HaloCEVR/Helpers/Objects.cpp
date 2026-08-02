#include "Objects.h"
#include "Camera.h"
#include "../Hooking/Hooks.h"

#include <cstddef>

namespace
{
	constexpr uint16_t InvalidID = 0xFFFF;

	bool LooksValid(const HaloID& id)
	{
		return id.index != InvalidID && id.id != InvalidID;
	}

	float DistanceSquaredXY(const Vector3& a, const Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		return dx * dx + dy * dy;
	}

	bool ResolvePlayerBiped(uint16_t playerIndex, HaloID& outID, UnitDynamicObject*& outUnit)
	{
		PlayerDatum* player = Helpers::GetPlayerDatum(playerIndex);
		if (!player || !LooksValid(player->objectID))
		{
			return false;
		}

		BaseDynamicObject* object = Helpers::GetDynamicObject(player->objectID);
		if (!object || object->N0000027E != ObjectType::BIPED)
		{
			return false;
		}

		UnitDynamicObject* unit = reinterpret_cast<UnitDynamicObject*>(object);

		// PlayerDatum.playerID is the salt/id half of Unit.playerID.  Reject stale
		// table slots, but tolerate a temporarily uninitialised Unit.playerID while
		// a map or spawn is being created.
		const bool unitPlayerIDUninitialised = unit->playerID.index == InvalidID || unit->playerID.id == InvalidID;
		const bool unitMatchesDatum = unit->playerID.index == playerIndex && unit->playerID.id == player->playerID;
		if (!unitPlayerIDUninitialised && !unitMatchesDatum)
		{
			return false;
		}

		outID = player->objectID;
		outUnit = unit;
		return true;
	}

}

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
	ObjectTable& table = GetObjectTable();

	if (!table.elements || !LooksValid(id) || id.index >= table.currentSize || id.index >= table.maxElements)
	{
		return nullptr;
	}

	ObjectDatum& datum = table.elements[id.index];
	if (datum.id != id.id)
	{
		return nullptr;
	}

	return datum.dynamicObject;
}

PlayerDatum* Helpers::GetPlayerDatum(uint16_t index)
{
	PlayerTable& table = GetPlayerTable();

	if (!table.elements || index >= table.currentSize || index >= table.maxElements)
	{
		return nullptr;
	}

	// Use the engine-reported stride rather than C++ array indexing.  This also
	// protects the resolver if a compatible executable reports a larger datum.
	constexpr size_t RequiredPrefixSize = offsetof(PlayerDatum, objectID) + sizeof(HaloID);
	if (table.elementSize < RequiredPrefixSize)
	{
		return nullptr;
	}

	uint8_t* elements = reinterpret_cast<uint8_t*>(table.elements);
	return reinterpret_cast<PlayerDatum*>(elements + static_cast<size_t>(index) * table.elementSize);
}

bool Helpers::GetLocalPlayerID(HaloID& outID)
{
	PlayerTable& table = GetPlayerTable();

	if (!table.elements || table.currentSize == 0 || table.maxElements == 0)
	{
		return false;
	}

	const uint16_t playerCount = table.currentSize < table.maxElements ? table.currentSize : table.maxElements;

	// localID is zero (or another local split-screen slot) for players owned by
	// this client and 0xFFFF for network peers.  This works for both a VR host
	// (player index 0) and a joined VR client (player index 1+).
	for (uint16_t i = 0; i < playerCount; i++)
	{
		PlayerDatum* player = GetPlayerDatum(i);
		if (!player || player->localID == InvalidID)
		{
			continue;
		}

		UnitDynamicObject* unit = nullptr;
		if (ResolvePlayerBiped(i, outID, unit))
		{
			return true;
		}
	}

	// During early spawn/map transitions localID can be populated later than the
	// biped.  The engine Player/Camera structure is always local to this process,
	// so choose the valid player biped nearest that camera as a temporary fallback.
	const Vector3 cameraPosition = GetCamera().position;
	float bestDistance = 99999999.0f;
	int bestPlayerIndex = -1;
	HaloID bestID{ InvalidID, InvalidID };

	for (uint16_t i = 0; i < playerCount; i++)
	{
		HaloID candidateID{ InvalidID, InvalidID };
		UnitDynamicObject* candidateUnit = nullptr;
		if (!ResolvePlayerBiped(i, candidateID, candidateUnit))
		{
			continue;
		}

		const float distance = DistanceSquaredXY(candidateUnit->position, cameraPosition);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestPlayerIndex = i;
			bestID = candidateID;
		}
	}

	if (bestPlayerIndex >= 0)
	{
		outID = bestID;
		return true;
	}

	return false;
}

BaseDynamicObject* Helpers::GetLocalPlayer()
{
	HaloID playerID;
	if (!GetLocalPlayerID(playerID))
	{
		return nullptr;
	}

	return GetDynamicObject(playerID);
}
