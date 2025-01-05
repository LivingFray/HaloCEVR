#pragma once

struct Controls
{
	unsigned char Jump;
	unsigned char SwitchGrenades;
	unsigned char Interact;
	unsigned char SwitchWeapons;
	unsigned char Melee;
	unsigned char Flashlight;
	unsigned char Grenade;
	unsigned char Fire;
	unsigned char MenuForward;
	unsigned char MenuBack;
	unsigned char Crouch;
	unsigned char Zoom;
	unsigned char ShowScores;
	unsigned char Reload;
	unsigned char PickupWeapon;
	unsigned char AllChat;
	unsigned char TeamChat;
	unsigned char VehicleChat;
	unsigned char unk_01;
	unsigned char unk_02;
	float Forwards;
	float Left;
	float Yaw;
	float Pitch;
	unsigned char ControllerAim;
};

struct Controls_Custom
{
	unsigned char Jump;
	unsigned char SwitchGrenades;
	unsigned char Interact;
	unsigned char SwitchWeapons;
	unsigned char Melee;
	unsigned char Flashlight;
	unsigned char Grenade;
	unsigned char Fire;
	unsigned char MenuForward;
	unsigned char MenuBack;
	unsigned char Crouch;
	unsigned char Zoom;
	unsigned char ShowScores;
	unsigned char Reload;
	unsigned char PickupWeapon;
	unsigned char AllChat;
	unsigned char TeamChat;
	unsigned char VehicleChat;
	unsigned char unk_01;
	unsigned char unk_02;
	unsigned char whygearboxwhy[12];
	float Forwards;
	float Left;
	float Yaw;
	float Pitch;
	unsigned char ControllerAim;
};


namespace Helpers
{
	Controls& GetControls();
	Controls_Custom& GetControlsCustom();
}