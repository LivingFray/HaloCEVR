#include "InputHandler.h"
#include "Game.h"
#include "Helpers/Controls.h"
#include "Helpers/Camera.h"
#include "Helpers/Menus.h"
#include "Helpers/Maths.h"
#include "Helpers/Cutscene.h"


#define RegisterBoolInput(set, x) x = vr->RegisterBoolInput(set, #x);
#define RegisterVector2Input(set, x) x = vr->RegisterVector2Input(set, #x);
#define ApplyBoolInput(x) controls.##x = vr->GetBoolInput(x) ? 127 : 0;
#define ApplyImpulseBoolInput(x) controls.##x = vr->GetBoolInput(x, bHasChanged) && bHasChanged ? 127 : 0;

void InputHandler::RegisterInputs()
{
	IVR* vr = Game::instance.GetVR();

	// Physical inputs stay on their actual hands when the active weapon changes.
	RegisterBoolInput("default", PhysicalLeftGrip);
	RegisterBoolInput("default", PhysicalRightGrip);
	RegisterBoolInput("default", PhysicalLeftTrigger);
	RegisterBoolInput("default", PhysicalRightTrigger);
	RegisterBoolInput("default", PhysicalJump);
	RegisterBoolInput("default", PhysicalGrenade);
	RegisterBoolInput("default", PhysicalGrenadeCycle);
	RegisterBoolInput("default", PhysicalRecentre);
    RegisterBoolInput("default", PhysicalInteract);
    RegisterVector2Input("default", PhysicalMove);
    RegisterVector2Input("default", PhysicalLook);

	// These bindings will always be the same and won't change depending on action sets
	RegisterBoolInput("left handed", SwapWeaponHand);
	OffhandSwapWeaponHand = SwapWeaponHand;
	RegisterBoolInput("default", SwapWeaponHand);

	UpdateRegisteredInputs();
}

void InputHandler::UpdateRegisteredInputs()
{
	IVR* vr = Game::instance.GetVR();

	const char* actionSet = Game::instance.bLeftHanded ? "left handed" : "default";

	// These bindings will change depending on action sets
	RegisterBoolInput(actionSet, Jump);
	RegisterBoolInput(actionSet, SwitchGrenades);
	RegisterBoolInput(actionSet, Interact);
	RegisterBoolInput(actionSet, SwitchWeapons);
	RegisterBoolInput(actionSet, Melee);
	RegisterBoolInput(actionSet, Flashlight);
	RegisterBoolInput(actionSet, Grenade);
	RegisterBoolInput(actionSet, Fire);
	RegisterBoolInput(actionSet, MenuForward);
	RegisterBoolInput(actionSet, MenuBack);
	RegisterBoolInput(actionSet, Crouch);
	RegisterBoolInput(actionSet, Zoom);
	RegisterBoolInput(actionSet, Reload);
	RegisterBoolInput(actionSet, TwoHandGrip);

	RegisterVector2Input(actionSet, Move);
	RegisterVector2Input(actionSet, Look);
}

float AngleBetweenVector2(const Vector2& v1, const Vector2& v2)
{
	const float dot = v1.dot(v2);
	const float determinant = v1.x * v2.y - v1.y * v2.x;
	const float angle = atan2(determinant, dot);
	return angle;
}

Vector2 RotateVector2(const Vector2& v, float angle)
{
	Vector2 rotated;
	rotated.x = v.x * cos(angle) - v.y * sin(angle);
	rotated.y = v.x * sin(angle) + v.y * cos(angle);
	return rotated;
}

#define DRAW_DEBUG_MOVE 0

void InputHandler::InitializePhysicalWeapons()
{
	const bool profileReady = physicalEngine.Initialize();
    Game::instance.interactions.AttachEngine(physicalEngine);
	physicalWeaponsEnabled = Game::instance.c_PhysicalWeapons->Value() &&
		!Game::instance.bUse3DOFAiming && profileReady;
	if (physicalWeaponsEnabled && (!physicalEngine.InstallPickupGuard() || !physicalEngine.InstallVisualGuards())) {
		physicalWeaponsEnabled = false;
		Logger::log << "[PhysicalWeapons] Required campaign pickup/visual guard unavailable: stock controls retained." << std::endl;
	}
	Logger::log << "[PhysicalWeapons] " << (physicalWeaponsEnabled ?
		"Retail engine verified. R1.9 campaign grip pickup/hold/holster/drop enabled; one held weapon." :
		"Inactive: disabled, 3DOF, unsupported executable, or native code modified. Stock weapon controls retained.") << std::endl;
}

void InputHandler::UpdatePhysicalButtons()
{
	auto* vr = Game::instance.GetVR();
	auto& controls = Helpers::GetControls();
	controls.Jump = controls.Grenade = controls.SwitchGrenades = 0;
	bool jump = false, grenade = false, cycle = false;
	const bool jumpValid = vr->TryGetBoolInput(PhysicalJump,jump);
	const bool grenadeValid = vr->TryGetBoolInput(PhysicalGrenade,grenade);
	const bool cycleValid = vr->TryGetBoolInput(PhysicalGrenadeCycle,cycle);
	controls.Jump = jumpValid && jump ? 127 : 0; // Right A, independent of weapon hand.
	// Physical mode owns throwing even if a per-frame native read is delayed.
    // Never fall through to a surprise stick throw while reaching for a holster.
    controls.Grenade = !Game::instance.c_PhysicalGrenades->Value() && grenadeValid && grenade ? 127 : 0;
	const auto cycleEdge=physicalGrenadeCycle.Update(cycle,cycleValid);
    if(!cycleValid)physicalCycleArmed=false;
    if(cycleEdge.pressed)physicalCycleArmed=true;
    // Wait for X release so an X + RT menu chord never also changes grenades.
    controls.SwitchGrenades=cycleEdge.released && physicalCycleArmed && !menuChordUsed?127:0;
    if(cycleEdge.released)physicalCycleArmed=false;
    Game::instance.interactions.GrenadeControls();
}

void InputHandler::ReportPhysicalStatus(const char* status)
{
	if (physicalStatus == status) return;
	physicalStatus = status;
	Logger::log << "[PhysicalWeapons] " << status << std::endl;
}

bool InputHandler::ShouldBlockPhysicalFire(const HaloID& weapon) const
{
	const Immersion::Object id = static_cast<Immersion::Object>(weapon.index) |
		(static_cast<Immersion::Object>(weapon.id)<<16);
	return physicalControlsActive && physicalSession.Owns(id) &&
		(!physicalFrameValid || !physicalCanFire || physicalSession.Held() != id);
}

bool InputHandler::PhysicalRecoilState(const HaloID& weapon, Immersion::FireState& state, bool& supported) const
{
    state={}; supported=false;
    const Immersion::Object id=static_cast<Immersion::Object>(weapon.index) |
        (static_cast<Immersion::Object>(weapon.id)<<16);
    if (!physicalControlsActive || !physicalFrameValid || !physicalSession.Active() ||
        physicalSession.Held()!=id || physicalSession.SupportOnly() ||
        !physicalEngine.ReadFireState(id,state)) return false;
    supported=physicalSession.Items().Supported(id);
    return true;
}

bool InputHandler::GetSupportOnlyTransform(Matrix4& transform) const
{
	if (!physicalWeaponsEnabled || !physicalSession.Active() || !physicalSession.SupportOnly() || !haveHeldTransform) return false;
	const auto hand = Immersion::Other(physicalSession.FiringHand());
	const auto role = hand == Immersion::Hand::Left ? ControllerRole::Left : ControllerRole::Right;
	auto* vr = Game::instance.GetVR();
	if (!physicalFrameValid || !vr->IsControllerTracked(role)) transform = lastHeldTransform;
	else transform = vr->GetControllerTransform(role,true)*supportRelativeTransform;
	return true;
}

bool InputHandler::UpdatePhysicalWeapons(bool inVehicle)
{
	using namespace Immersion;
	auto& game = Game::instance;
	auto* vr = game.GetVR();
	physicalFrameValid = false;
	physicalCanFire = false;
	game.bCompactSupportGrip = false;
	physicalEngine.SetPickupEnabled(physicalWeaponsEnabled && physicalSession.Active());
	if (!physicalWeaponsEnabled) { game.DeactivateManualReload(); game.interactions.SuspendGrenades(); return false; }
	auto suspend = [&]() {
		game.SuspendManualReload();
        game.interactions.SuspendGrenades();
		for (auto& button : physicalGripButtons) button.Suspend();
		holsterHover.Reset();
		physicalGrenadeCycle.Suspend();
        physicalCycleArmed=false;
		physicalTriggerReady = false;
	};
	const auto now = std::chrono::steady_clock::now();
	if (lastPhysicalInputUpdate.time_since_epoch().count() != 0 &&
		std::chrono::duration<float>(now-lastPhysicalInputUpdate).count() > 0.25f) suspend();
	lastPhysicalInputUpdate = now;
	if (inVehicle || Helpers::IsCampaignLoading()) {
		game.DeactivateManualReload();
		suspend(); physicalSession.Reset(); haveHeldTransform = false; haveHolsterYaw = false;
		physicalEngine.SetPickupEnabled(false);
		game.bUseTwoHandAim = false;
		ReportPhysicalStatus(inVehicle ? "Vehicle: stock controls." : "Loading: waiting for campaign player.");
		return false;
	}
	// Resolve context BEFORE reading optional actions. An unavailable new binding
	// must never take over unsupported contexts or a player without valid inputs.
	NativeInventory snapshot;
	if (!physicalEngine.Read(snapshot)) {
		const bool resume = physicalSession.Active() && physicalEngine.ReadMayResume();
		suspend();
		if (!resume) { game.DeactivateManualReload(); physicalSession.Reset(); haveHeldTransform = false; }
		physicalEngine.SetPickupEnabled(false);
		game.bUseTwoHandAim = false;
		ReportPhysicalStatus(physicalEngine.ReadStatus());
		if (resume) {
            // Keep our input ownership across damage/death/checkpoint gaps.
            // No native inventory action is issued until a fresh read passes.
            // WeaponSession reconciles a replacement unit/checkpoint itself.
            UpdatePhysicalButtons();
            auto& controls = Helpers::GetControls();
            controls.Fire = controls.Reload = controls.SwitchWeapons = 0;
            controls.Interact = controls.PickupWeapon = controls.Melee = 0;
        }
		return resume;
	}
	const auto count = std::count_if(snapshot.weapons.begin(),snapshot.weapons.end(),[](Object id) { return id != None; });
	if (count > 2) {
		game.DeactivateManualReload();
		suspend(); physicalSession.Reset(); haveHeldTransform = false;
		physicalEngine.SetPickupEnabled(false);
		ReportPhysicalStatus("More than two native weapons: stock controls.");
		return false;
	}
	const auto* cutscene = Helpers::GetCutsceneData();
	if (!vr || game.bUse3DOFAiming || Helpers::IsMouseVisible() || game.bNeedsRecentre ||
		(cutscene && cutscene->bInCutscene) || !vr->IsControllerTracked(ControllerRole::Left) ||
		!vr->IsControllerTracked(ControllerRole::Right)) { suspend(); return physicalSession.Active(); }
	std::array<bool,2> down{}, trigger{};
	// Evaluate all four for useful per-action diagnostics; do not short-circuit.
	const bool leftGripValid = vr->TryGetBoolInput(PhysicalLeftGrip,down[0]);
	const bool rightGripValid = vr->TryGetBoolInput(PhysicalRightGrip,down[1]);
	const bool leftTriggerValid = vr->TryGetBoolInput(PhysicalLeftTrigger,trigger[0]);
	const bool rightTriggerValid = vr->TryGetBoolInput(PhysicalRightTrigger,trigger[1]);
	if (!leftGripValid || !rightGripValid || !leftTriggerValid || !rightTriggerValid) {
		ReportPhysicalStatus(physicalSession.Active() ? "Input interrupted: retaining held/stowed state until input resumes." :
			"Bindings unavailable: stock controls retained; holsters have NOT initialized.");
		suspend(); return physicalSession.Active();
	}
	auto point = [](Vector3 p) { return Point{p.x,p.y,p.z}; };
	auto vector = [](Point p) { return Vector3(p.x,p.y,p.z); };
	const auto head = vr->GetHMDTransform();
	const auto origin = point(head*Vector3());
	const auto forward = head.getLeftAxis();
	if (!origin.Finite() || !point(forward).Finite()) { suspend(); return physicalSession.Active(); }
	// Head pitch never lifts the hip holster. Retain yaw while looking straight up.
	if (forward.x*forward.x+forward.y*forward.y > 0.04f) {
		holsterYaw = std::atan2(forward.y,forward.x); haveHolsterYaw = true;
	}
	if (!haveHolsterYaw) { suspend(); return physicalSession.Active(); }
	Layout layout{game.c_LeftHanded->Value()};
	const std::array<Point,2> zones{layout.Anchor(Zone::Shoulder,origin,holsterYaw),
		layout.Anchor(Zone::WeaponHip,origin,holsterYaw)};
	auto radius = [](float value, float fallback, float low, float high) {
		return std::isfinite(value) ? (std::max)(low,(std::min)(value,high)) : fallback;
	};
	const float radii[2] = {radius(game.c_PhysicalShoulderRadius->Value(),0.20f,0.10f,0.28f),
		radius(game.c_PhysicalHipRadius->Value(),0.16f,0.08f,0.24f)};
	const auto camera = Helpers::GetCamera().position;
	std::array<Matrix4,2> poses{vr->GetControllerTransform(ControllerRole::Left,true),
		vr->GetControllerTransform(ControllerRole::Right,true)};
	std::array<Point,2> hands{};
	WeaponFrame frame;
	frame.pickupTarget = physicalEngine.PickupTarget();
	if (frame.pickupTarget != None) {
		// Native weapon-swap buttons must not bypass deliberate grip acquisition.
		// Other interactions keep their ordinary controls when no weapon is offered.
		Helpers::GetControls().Interact = 0;
		Helpers::GetControls().PickupWeapon = 0;
	}
	for (unsigned h=0; h<2; ++h) {
		const auto role = h == 0 ? ControllerRole::Left : ControllerRole::Right;
		hands[h] = point(vr->GetRawControllerTransform(role)*Vector3());
		frame.release[h].position = point(camera+(poses[h]*Vector3())*game.MetresToWorld(1));
		frame.release[h].forward = point(poses[h].getLeftAxis());
		frame.release[h].up = point(poses[h].getForwardAxis());
		frame.release[h].velocity = point(vr->GetControllerVelocity(role));
		if (!hands[h].Finite() || !frame.release[h].position.Finite() || !frame.release[h].forward.Finite() ||
			!frame.release[h].up.Finite() || !frame.release[h].velocity.Finite()) { suspend(); return true; }
		frame.consumed[h] = headGripConsumed[h];
		float nearest = 1.0e10f;
		for (unsigned slot=0; slot<2; ++slot) {
			const float distance = (hands[h]-zones[slot]).LengthSquared();
			if (distance <= radii[slot]*radii[slot] && distance < nearest) {
				nearest = distance; frame.holster[h] = static_cast<int>(slot);
			}
		}
	}
	physicalFrameValid = true;
	for (unsigned h=0; h<2; ++h) frame.grip[h] = physicalGripButtons[h].Update(down[h]);
	const bool previouslySupportOnly = physicalSession.SupportOnly();
	const auto previousHeld = physicalSession.Held();
	if (previousHeld != None) {
		lastHeldTransform = game.GetHeldWeaponTransform(); haveHeldTransform = true;
		const Point gripPoint = point(lastHeldTransform*Vector3());
		const Point direction = point(lastHeldTransform.getLeftAxis());
		const bool longGun = game.UsesLongSupportGrip(previousHeld);
        const float supportAdvance=game.SupportGripAdvance(previousHeld);
        const float supportLift=game.SupportGripLift(previousHeld);
        const float forgiveness=game.SupportGripForgiveness(previousHeld);
        const Point supportUp=point(lastHeldTransform.getForwardAxis());
		for (unsigned h=0; h<2; ++h) {
			const auto fromGrip = hands[h]-gripPoint;
			const float along = fromGrip.Dot(direction);
			frame.nearTrigger[h] = Within(hands[h],gripPoint,0.20f);
			// Both compact-hand contacts use raw controller origins. The rendered
            // wrist sits behind a Quest controller and is not the grab target.
            const auto firingRole=physicalSession.FiringHand()==Hand::Left?ControllerRole::Left:ControllerRole::Right;
            const Point firingController=point(vr->GetRawControllerTransform(firingRole)*Vector3());
            const Point compactCenter=firingController+direction*.10f-point(lastHeldTransform.getForwardAxis())*.075f;
            const float compactAlong=(hands[h]-firingController).Dot(direction);
            frame.nearSupport[h] = longGun ?
                (along>=.20f+supportAdvance-forgiveness && along<=.65f+supportAdvance+forgiveness &&
                    (fromGrip-direction*along-supportUp*supportLift).LengthSquared()<=(.14f+forgiveness)*(.14f+forgiveness) &&
                    (supportLift==0 || fromGrip.Dot(supportUp)>=supportLift-.08f)) :
                (compactAlong>=-.03f && Within(hands[h],compactCenter,.16f));
		}
	}
    // An owned grenade keeps its release/return priority. Otherwise a valid
    // slide, charging handle or magazine grip claims the hand before the chest.
    const bool ownsGrenade=game.interactions.GrenadeHandMask()!=0;
    if(ownsGrenade)game.interactions.Grenades(physicalEngine,physicalSession,frame,poses.data(),origin,holsterYaw);
    bool reloadDown=false;
    const bool reloadValid=vr->TryGetBoolInput(Reload,reloadDown);
    if(game.bLeftHanded)reloadDown=recentreTap;
    if (game.ProcessManualReload(physicalEngine,physicalSession,frame,poses.data(),origin,holsterYaw,reloadDown,reloadValid))
        physicalTriggerReady=false;
    if(!ownsGrenade)game.interactions.Grenades(physicalEngine,physicalSession,frame,poses.data(),origin,holsterYaw);
	const auto events = physicalSession.Update(physicalEngine,frame);
	if (!physicalSession.Active()) {
		game.DeactivateManualReload();
		ReportPhysicalStatus("Initial stow was not confirmed by Halo: stock controls retained.");
		suspend(); return false;
	}
	const bool visualsReady = physicalEngine.SynchronizeViewModel();
	physicalEngine.SetPickupEnabled(true);
	UpdatePhysicalButtons();
	ReportPhysicalStatus(visualsReady ? "Campaign holsters active: loose weapons require grip; surviving homes retained." :
        "Campaign holsters active: waiting for native first-person model refresh.");
	std::array<bool,2> actionHaptic{};
	for (unsigned i=0; i<events.count; ++i) {
		const auto& event = events.entries[i];
		Logger::log << "[PhysicalWeapons] event=" << WeaponEventName(event.event)
			<< " hand=" << (event.hand == Hand::Left ? "L" : "R") << " object=" << event.weapon << std::endl;
		if (event.event == WeaponEvent::Drawn || event.event == WeaponEvent::PickedUp || event.event == WeaponEvent::TriggerRegripped) physicalTriggerReady = false;
		if (event.event != WeaponEvent::Rejected && event.event != WeaponEvent::Initialized && event.event != WeaponEvent::Reconciled && event.event != WeaponEvent::Removed) {
			actionHaptic[Index(event.hand)] = true;
			vr->TriggerHapticVibration(event.hand == Hand::Left ? ControllerRole::Left : ControllerRole::Right,0,0.04f,100,0.35f);
		}
	}
	for (unsigned hand=0; hand<2; ++hand) {
		const int entered = holsterHover.Update(hand,hands[hand],zones,radii);
		if (entered == 0 && !actionHaptic[hand] && !headGripConsumed[hand])
			vr->TriggerHapticVibration(hand == 0 ? ControllerRole::Left : ControllerRole::Right,0,0.025f,90,0.20f);
	}
	const bool supportOnly = physicalSession.SupportOnly();
	if (supportOnly && !previouslySupportOnly && haveHeldTransform) {
		const unsigned supporting = Index(Other(physicalSession.FiringHand()));
		Matrix4 inverse = poses[supporting]; inverse.invertAffine();
		supportRelativeTransform = inverse*lastHeldTransform;
	}
	const Object held = physicalSession.Held();
	if (held != None) {
		const bool left = physicalSession.FiringHand() == Hand::Left;
		if (game.bLeftHanded != left) { game.bLeftHanded = left; UpdateRegisteredInputs(); }
		// The saved LeftHanded layout remains untouched when drawing with the other hand.
	}
	const bool supported=(physicalSession.Items().Supported(held) || game.ManualPumpSupportsWeapon()) && down[0] && down[1];
    const bool longGrip=game.UsesLongSupportGrip(held);
    game.bCompactSupportGrip=supported && !longGrip;
    game.bUseTwoHandAim=supported && longGrip;
	const auto h = Index(physicalSession.FiringHand());
	if (!trigger[h]) physicalTriggerReady = true;
	physicalCanFire = physicalSession.CanFire(frame) && physicalTriggerReady && !(h==1 && menuRightTriggerBlocked);
	if (physicalCanFire && trigger[h]) Helpers::GetControls().Fire = 127;
	else Helpers::GetControls().Fire = 0;
	return true;
}

void InputHandler::UpdateMenuInput()
{
    auto& game=Game::instance;
    auto* vr=game.GetVR();
    const auto now=std::chrono::steady_clock::now();
    // Keep Escape down across native input polling, not just a VR render frame.
    // Service key-up even if tracking/bindings disappear or a menu just opened.
    if(bHoldingMenu && now>=menuEscapeReleaseTime){
        INPUT input{};input.type=INPUT_KEYBOARD;
        input.ki.wScan=0x01;input.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;
        if(SendInput(1,&input,sizeof(INPUT))==1)bHoldingMenu=false;
    }
    const bool ghost=game.interactions.GhostActive();
    recentreTap=false;
    if(physicalWeaponsEnabled){
        bool held=false;
        const bool valid=vr && vr->IsHMDTracked() && vr->IsControllerTracked(ControllerRole::Left) &&
            vr->TryGetBoolInput(PhysicalRecentre,held);
        const auto edge=recentreButton.Update(held,valid);
        if(!valid){recentreArmed=false;recentreFired=false;}
        if(edge.pressed){recentreHeldTime=now;recentreArmed=true;recentreFired=false;}
        if(recentreArmed && edge.down && !recentreFired &&
            std::chrono::duration<float>(now-recentreHeldTime).count()>=5.f){
            recentreFired=true;game.bNeedsRecentre=true;
            vr->TriggerHapticVibration(ControllerRole::Left,0,.12f,100,.5f);
            Logger::log<<"[VRMenu] Physical Y held five seconds: recenter requested."<<std::endl;
        }
        if(edge.released){recentreTap=recentreArmed && !recentreFired;recentreArmed=false;}
    }
    const bool chordMode=physicalWeaponsEnabled && !ghost && (physicalControlsActive || game.interactions.VehicleActive());
    auto tap=[&](){
        if(bHoldingMenu)return;
        INPUT input{};input.type=INPUT_KEYBOARD;
        input.ki.wScan=0x01;input.ki.dwFlags=KEYEVENTF_SCANCODE;
        const bool sent=SendInput(1,&input,sizeof(INPUT))==1;
        if(sent){bHoldingMenu=true;menuEscapeReleaseTime=now+std::chrono::milliseconds(80);}
        Logger::log<<"[VRMenu] "<<(chordMode?"X + RT":"Menu tap")
            <<": Escape "<<(sent?"queued":"injection failed")<<std::endl;
    };
    if(ghost){
        // X and RT must remain usable together for shooting while driving.
        menuRightTriggerBlocked=false;menuChordReady=false;menuChordUsed=true;
        menuBinding=0;menuButton.Suspend();menuChordTrigger.Suspend();menuArmed=false;
        if(recentreTap)tap();
        return;
    }
    const auto binding=chordMode?PhysicalGrenadeCycle:MenuBack;
    if(binding!=menuBinding){
        menuButton.Suspend();menuChordTrigger.Suspend();menuArmed=false;
        menuChordReady=false;menuChordUsed=true;menuBinding=binding;
    }
    bool down=false;
    const bool valid=vr && vr->TryGetBoolInput(binding,down);
    const auto edge=menuButton.Update(down,valid);
    if(chordMode){
        bool trigger=false;
        const bool triggerValid=vr && vr->TryGetBoolInput(PhysicalRightTrigger,trigger);
        const auto triggerEdge=menuChordTrigger.Update(trigger,triggerValid);
        if(!valid || !triggerValid){
            menuChordReady=false;menuChordUsed=true;menuRightTriggerBlocked=true;return;
        }
        if(!trigger)menuRightTriggerBlocked=false;
        if(edge.pressed){menuChordUsed=false;menuChordReady=!trigger;}
        if(edge.down){
            // A held RT must be released before X can arm the menu chord.
            // Suppression persists if X is released first, until RT releases.
            menuRightTriggerBlocked=true;
            if(!trigger)menuChordReady=true;
            if(menuChordReady && triggerEdge.pressed){menuChordReady=false;menuChordUsed=true;tap();}
        }
        if(edge.released)menuChordReady=false;
        return;
    }
    menuRightTriggerBlocked=false;
    if(!valid){menuArmed=false;return;}
    if(edge.pressed){menuHeldTime=now;menuArmed=true;}
    if(!menuArmed)return;
    const float heldTime=std::chrono::duration<float,std::milli>(now-menuHeldTime).count();
    constexpr float holdToRecentreTime=5000.f;
    if(edge.down && heldTime>100.f){
        const float progress=(std::min)(heldTime,holdToRecentreTime)/holdToRecentreTime;
        const Matrix4 hand=vr->GetControllerTransform(ControllerRole::Left,true);
        const Vector3 center=(hand*Vector3())*game.MetresToWorld(1)+Helpers::GetCamera().position+
            Vector3(0,0,game.MetresToWorld(.1f));
        game.inGameRenderer.DrawPolygon(center,-hand.getLeftAxis(),hand.getForwardAxis(),16,
            game.MetresToWorld(.01f),D3DCOLOR_XRGB(255,0,0),false,progress);
    }
    if(!edge.released)return;
    menuArmed=false;
    if(heldTime>=holdToRecentreTime){
        game.bNeedsRecentre=true;
        Logger::log<<"[VRMenu] Recenter requested by held menu button."<<std::endl;
    }else tap();
}

void InputHandler::UpdateInputs(bool bInVehicle)
{
	IVR* vr = Game::instance.GetVR();

	vr->UpdateInputs();
    UpdateMenuInput(); // Resolve the chord before any gameplay trigger processing.
	// Native MenuForward is ignored during gameplay, but accepts tutorial OK
	// even when a dialog fails to expose the game's mouse-visible flag. Fixed
	// right A also works with the user's older personal SteamVR bindings.
	bool confirmDown = false;
	const bool confirmValid = vr->TryGetBoolInput(PhysicalJump,confirmDown);
	const bool confirm = menuConfirm.Update(confirmDown,confirmValid).pressed;

	static bool bHasChanged = false;

	if (Game::instance.bIsCustom)
	{
		Controls_Custom& controls = Helpers::GetControlsCustom();

		ApplyBoolInput(Jump);
		ApplyImpulseBoolInput(SwitchGrenades);
		ApplyBoolInput(Interact);
		ApplyBoolInput(Melee);
		ApplyBoolInput(Flashlight);
		ApplyBoolInput(Grenade);
		ApplyBoolInput(Fire);
		ApplyBoolInput(MenuForward);
		if (confirm) controls.MenuForward = 127;
        // Preserve native Escape/MenuBack written by HandleInputs. Clearing it
        // here cancels the keyboard Escape that UpdateMenuInput sends.
		ApplyBoolInput(Crouch);
		ApplyImpulseBoolInput(Zoom);
		ApplyBoolInput(Reload);

		Game::instance.bIsFiring = controls.Fire;
	}
	else
	{
		Controls& controls = Helpers::GetControls();

		ApplyBoolInput(Jump);
		ApplyImpulseBoolInput(SwitchGrenades);
		ApplyBoolInput(Interact);
		ApplyBoolInput(Melee);
		ApplyBoolInput(Flashlight);
		ApplyBoolInput(Grenade);
		ApplyBoolInput(Fire);
		ApplyBoolInput(MenuForward);
		if (confirm) controls.MenuForward = 127;
        // Preserve native Escape/MenuBack written by HandleInputs. Clearing it
        // here cancels the keyboard Escape that UpdateMenuInput sends.
		ApplyBoolInput(Crouch);
		ApplyImpulseBoolInput(Zoom);
		ApplyBoolInput(Reload);

		Game::instance.bIsFiring = controls.Fire;
	}


    if(physicalWeaponsEnabled && !Game::instance.bIsCustom){
        bool interact=false;
        if(vr->TryGetBoolInput(PhysicalInteract,interact))Helpers::GetControls().Interact=interact?127:0;
        else if(Game::instance.bLeftHanded)Helpers::GetControls().Interact=0;
    }
	unsigned char MotionControlFlashlight = UpdateFlashlight();
	if (MotionControlFlashlight > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Flashlight = MotionControlFlashlight;
		}
		else
		{
			Helpers::GetControls().Flashlight = MotionControlFlashlight;
		}
	}

	// Native controls may retain last update's switch value; always clear it.
	if (Game::instance.bIsCustom) Helpers::GetControlsCustom().SwitchWeapons = 0;
	else Helpers::GetControls().SwitchWeapons = 0;

	if(physicalWeaponsEnabled && Game::instance.bLeftHanded && !bInVehicle)Helpers::GetControls().Reload=recentreTap?127:0;
	physicalControlsActive = UpdatePhysicalWeapons(bInVehicle);
	if (!physicalControlsActive)
	{
		if (Game::instance.c_EnableWeaponHolsters->Value())
		{
			unsigned char HolsterSwitchWeapons = UpdateHolsterSwitchWeapons();
			bool bSwitchWeaponsPressed = vr->GetBoolInput(SwitchWeapons);

			if (HolsterSwitchWeapons > 0 && bSwitchWeaponsPressed)
			{
				if (Game::instance.bIsCustom)
				{
					Controls_Custom& controls = Helpers::GetControlsCustom();
					ApplyImpulseBoolInput(SwitchWeapons);
				}
				else
				{
					Controls& controls = Helpers::GetControls();
					ApplyImpulseBoolInput(SwitchWeapons);
				}
			}
		}
		else
		{
			if (Game::instance.bIsCustom)
			{
				Controls_Custom& controls = Helpers::GetControlsCustom();
				ApplyImpulseBoolInput(SwitchWeapons);
			}
			else
			{
				Controls& controls = Helpers::GetControls();
				ApplyImpulseBoolInput(SwitchWeapons);
			}
		}

		if (headGripConsumed[Game::instance.bLeftHanded ? 0 : 1]) {
			if (Game::instance.bIsCustom) Helpers::GetControlsCustom().SwitchWeapons = 0;
			else Helpers::GetControls().SwitchWeapons = 0;
		}
	}
	else if (!physicalFrameValid || !physicalCanFire) Helpers::GetControls().Fire = 0;

	unsigned char MotionControlMelee = UpdateMelee();
	if (MotionControlMelee > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Melee = MotionControlMelee;
		}
		else
		{
			Helpers::GetControls().Melee = MotionControlMelee;
		}
	}

	unsigned char MotionControlCrouch = UpdateCrouch();
	if (MotionControlCrouch > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Crouch = MotionControlCrouch;
		}
		else
		{
			Helpers::GetControls().Crouch = MotionControlCrouch;
		}
	}

    if(menuRightTriggerBlocked){
        auto block=[&](auto& controls){
            if(!Game::instance.bLeftHanded)controls.Fire=0;
            controls.Zoom=0;
            controls.SwitchGrenades=0;
        };
        if(Game::instance.bIsCustom)block(Helpers::GetControlsCustom());
        else block(Helpers::GetControls());
    }

	if (!physicalControlsActive) UpdateHandsProximity();


	Vector2 MoveInput = vr->GetVector2Input(Move);

	if (!bInVehicle && (Game::instance.c_HandRelativeMovement->Value() != 0))
	{
		Camera& cam = Helpers::GetCamera();
		Vector3 camForward = cam.lookDir;

		const bool leftHand = (Game::instance.c_HandRelativeMovement->Value() == 1);
		const ControllerRole role = leftHand ? ControllerRole::Left : ControllerRole::Right;
		Matrix4 controllerTransform = vr->GetControllerTransform(role); // TODO: Not sure about bRenderPose bool param here. Doesn't seem to matter.
		const float offset = Game::instance.c_HandRelativeOffsetRotation->Value();
		controllerTransform.rotateZ(leftHand ? offset: -offset);
		Vector3 handForward = controllerTransform.getLeftAxis();

#if DRAW_DEBUG_MOVE
		Vector3 start = cam.position + cam.lookDirUp * -Game::instance.MetresToWorld(0.1f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + camForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 255, 0, 0), false, 0.5f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + handForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 0, 0, 255), false, 0.5f);
		handForward.z = camForward.z = 0.0f;
		Game::instance.inGameRenderer.DrawLine3D(start, start + camForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 255, 75, 0), false, 0.5f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + handForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 100, 0, 255), false, 0.5f);

		start = cam.position + cam.lookDir * Game::instance.MetresToWorld(2.00f);
		const Vector3 camRight = cam.lookDir.cross(cam.lookDirUp);
		Vector3 end = start + cam.lookDirUp * MoveInput.y * Game::instance.MetresToWorld(1.0f) + camRight * MoveInput.x * Game::instance.MetresToWorld(1.0f);
		Game::instance.inGameRenderer.DrawLine3D(start, end, D3DCOLOR_ARGB(255, 0, 255, 0), false, 0.5f);
#endif

		const float angle = AngleBetweenVector2(Vector2(camForward.x, camForward.y), Vector2(handForward.x, handForward.y));
		MoveInput = RotateVector2(MoveInput, angle);

#if DRAW_DEBUG_MOVE
		end = start + cam.lookDirUp * MoveInput.y * Game::instance.MetresToWorld(1.0f) + camRight * MoveInput.x * Game::instance.MetresToWorld(1.0f);
		Game::instance.inGameRenderer.DrawLine3D(start, end, D3DCOLOR_ARGB(255, 255, 255, 0), false, 0.5f);
#endif
	}

	if (Game::instance.bIsCustom)
	{
		Controls_Custom& controls = Helpers::GetControlsCustom();

		controls.Left = -MoveInput.x;
		controls.Forwards = MoveInput.y;
	}
	else
	{
		Controls& controls = Helpers::GetControls();

		controls.Left = -MoveInput.x;
		controls.Forwards = MoveInput.y;
	}
    bool triggers[2]{};
    const bool lt=vr->TryGetBoolInput(PhysicalLeftTrigger,triggers[0]);
    const bool rt=vr->TryGetBoolInput(PhysicalRightTrigger,triggers[1]);
    if(menuRightTriggerBlocked)triggers[1]=false;
    Game::instance.interactions.UpdateVehicle(physicalEngine,bInVehicle,triggers,lt && rt,
        vr->GetVector2Input(PhysicalMove));
    if(physicalControlsActive && !physicalFrameValid && Game::instance.c_PhysicalGrenades->Value())Helpers::GetControls().Grenade=0;
	Game::instance.bIsFiring = Game::instance.bIsCustom ? Helpers::GetControlsCustom().Fire != 0 : Helpers::GetControls().Fire != 0;
}

void InputHandler::UpdateCamera(float& yaw, float& pitch)
{
	IVR* vr = Game::instance.GetVR();

	Vector2 lookInput = vr->GetVector2Input(Look);

	float yawOffset = vr->GetYawOffset();

	if (Game::instance.c_SnapTurn->Value())
	{
		if (lastSnapState == 1)
		{
			if (lookInput.x < 0.4f)
			{
				lastSnapState = 0;
			}
		}
		else if (lastSnapState == -1)
		{
			if (lookInput.x > -0.4f)
			{
				lastSnapState = 0;
			}
		}
		else
		{
			if (lookInput.x > 0.6f)
			{
				lastSnapState = 1;
				yawOffset += Game::instance.c_SnapTurnAmount->Value();
			}
			else if (lookInput.x < -0.6f)
			{
				lastSnapState = -1;
				yawOffset -= Game::instance.c_SnapTurnAmount->Value();
			}
		}
	}
	else
	{
		yawOffset += lookInput.x * Game::instance.c_SmoothTurnAmount->Value() * Game::instance.lastDeltaTime;
	}

	vr->SetYawOffset(yawOffset);

	Vector3 lookHMD = vr->GetHMDTransform().getLeftAxis();
    // The animated exit camera is not an aiming reference. Feeding its error
    // back into native yaw/pitch can alternate and amplify every frame.
    auto& exit=Game::instance.vehicleExit;
    if(exit.Suppressing()){
        if(exit.TakeSync()){
            Game::instance.LastLookDir=Helpers::GetCamera().lookDir=lookHMD;
            Logger::log<<"[PhysicalVehicles] exit rotation reference resynchronized"<<std::endl;
        }
        yaw=pitch=0;return;
    }
	// Get current camera angle
	Vector3 lookGame = Game::instance.bDetectedChimera ? Game::instance.LastLookDir : Helpers::GetCamera().lookDir;
	// Apply deltas
	float yawHMD = atan2(lookHMD.y, lookHMD.x);
	float yawGame = atan2(lookGame.y, lookGame.x);
	yaw = Immersion::WrapRadians(yawHMD - yawGame);

	float pitchHMD = atan2(lookHMD.z, sqrt(lookHMD.x * lookHMD.x + lookHMD.y * lookHMD.y));
	float pitchGame = atan2(lookGame.z, sqrt(lookGame.x * lookGame.x + lookGame.y * lookGame.y));
	pitch = (pitchHMD - pitchGame);
}

void InputHandler::UpdateTutorialLook()
{
    auto& game=Game::instance;IVR* vr=game.GetVR();
    const bool valid=vr && vr->IsHMDTracked() && !game.bUse3DOFAiming &&
        !game.bNeedsRecentre && !Helpers::IsMouseVisible() && !Helpers::IsCampaignLoading() &&
        !game.interactions.VehicleActive();
    if(!valid){physicalEngine.RecordLook({},false,0);return;}
    // Run once per render frame, including scripted cryotube control locks.
    // Physical head motion and deliberate stick input feed the SAME native
    // history tests as the mouse; the script still decides when to proceed.
    const auto head=vr->GetHMDTransform().rotateZ(vr->GetYawOffset()).getLeftAxis();
    const auto stick=vr->GetVector2Input(Look);
    const auto recorded=physicalEngine.RecordLook({head.x,head.y,head.z},true,game.lastDeltaTime,stick.x,stick.y);
    if(recorded)Logger::log<<"[TutorialLook] headset/stick direction recorded: "<<recorded<<std::endl;
}

Vector2 InputHandler::VehicleLookInput(){return Game::instance.GetVR()->GetVector2Input(PhysicalLook);}

void InputHandler::UpdateCameraForVehicles(float& yaw, float& pitch)
{
    if(Game::instance.interactions.VehicleCamera(yaw,pitch,Game::instance.GetVR()->GetVector2Input(PhysicalMove),VehicleLookInput()))return;
	IVR* vr = Game::instance.GetVR();

	Vector2 lookInput = vr->GetVector2Input(Look);

	const float DegToRad = 3.141593f / 180.0f;

	const float YawDelta = lookInput.x * Game::instance.c_HorizontalVehicleTurnAmount->Value() * Game::instance.lastDeltaTime;
	const float PitchDelta = lookInput.y * Game::instance.c_VerticalVehicleTurnAmount->Value() * Game::instance.lastDeltaTime;

	float yawOffset = vr->GetYawOffset();

	yawOffset += YawDelta;

	vr->SetYawOffset(yawOffset);

	yaw = -DegToRad * YawDelta;
	pitch = DegToRad * PitchDelta;
}

unsigned char InputHandler::UpdateFlashlight()
{
	if (Game::instance.c_GripHeadFlashlight->Value()) return UpdateGripFlashlight();
	headGripConsumed = {};
    flashlightPulse=0;
	for (auto& button : headGripButtons) button.Suspend();
	headTouch.Update({}, {}, {}, 0, false);
	lastHeadTouchUpdate = {};
	IVR* vr = Game::instance.GetVR();

	Vector3 headPos = vr->GetHMDTransform() * Vector3(-0.1f, 0.0f, 0.0f);

	float leftDistance = Game::instance.c_LeftHandFlashlightDistance->Value();
	float rightDistance = Game::instance.c_RightHandFlashlightDistance->Value();

	bool offhandFlashlightEnabled = Game::instance.c_OffhandHandFlashlight->Value();

	bool checkLeftHand = !offhandFlashlightEnabled || !Game::instance.bLeftHanded;
	if (checkLeftHand && leftDistance > 0.0f)
	{
		Vector3 handPos = vr->GetRawControllerTransform(ControllerRole::Left) * Vector3(0.0f, 0.0f, 0.0f);

		if ((headPos - handPos).lengthSqr() < leftDistance * leftDistance)
		{
			return 127;
		}
	}

	bool checkRightHand = !offhandFlashlightEnabled || Game::instance.bLeftHanded;
	if (checkRightHand && rightDistance > 0.0f)
	{
		Vector3 handPos = vr->GetRawControllerTransform(ControllerRole::Right) * Vector3(0.0f, 0.0f, 0.0f);

		if ((headPos - handPos).lengthSqr() < rightDistance * rightDistance)
		{
			return 127;
		}
	}

	return 0;
}

unsigned char InputHandler::UpdateGripFlashlight()
{
	IVR* vr = Game::instance.GetVR();
	const auto now = std::chrono::steady_clock::now();
	const float dt = lastHeadTouchUpdate.time_since_epoch().count() == 0 ? 0.0f :
		std::chrono::duration<float>(now - lastHeadTouchUpdate).count();
	lastHeadTouchUpdate = now;
	const auto* cutscene = Helpers::GetCutsceneData();
	const auto* player = Helpers::GetLocalPlayer();
	std::array<bool,2> grip{};
	const bool valid = vr && player && player->health > 0 &&
		vr->IsHMDTracked() && !Helpers::IsMouseVisible() && !Helpers::IsCampaignLoading() &&
		!Game::instance.bNeedsRecentre && !(cutscene && cutscene->bInCutscene);
	if (!valid) {
        flashlightPulse=0;
		for (auto& button : headGripButtons) button.Suspend();
		headGripConsumed = {};
		headTouch.Update({}, {}, {}, dt, false);
		return 0;
	}
	const auto head = vr->GetHMDTransform();
	// Follow the saved layout, not the hand currently holding the weapon.
	const float templeSide = Game::instance.c_LeftHanded->Value() ? -0.12f : 0.12f;
	const auto temple = head * Vector3(-0.04f, templeSide, -0.03f);
	auto point = [](const Vector3& v) { return Immersion::Point{v.x, v.y, v.z}; };
	float radius = Game::instance.c_GripHeadFlashlightRadius->Value();
	if (!std::isfinite(radius)) radius = 0.14f;
	radius = (std::max)(0.06f, (std::min)(radius, 0.22f));
	std::array<bool, 2> nearTemple{}, held{}, pressed{};
	for (unsigned h = 0; h < 2; ++h) {
		const auto role = h == 0 ? ControllerRole::Left : ControllerRole::Right;
		const bool handValid=vr->IsControllerTracked(role) &&
            vr->TryGetBoolInput(h==0?PhysicalLeftGrip:PhysicalRightGrip,grip[h]);
        if(!handValid){headGripButtons[h].Suspend();headGripConsumed[h]=false;continue;}
        const auto p = vr->GetRawControllerTransform(role) * Vector3(0,0,0);
		nearTemple[h] = Immersion::Within(point(p), point(temple), radius);
		const auto edge = headGripButtons[h].Update(grip[h]);
		held[h] = edge.down;
		pressed[h] = edge.pressed;
        // An overhead grenade wind-up is not a head-touch flashlight gesture.
        if(Game::instance.interactions.GrenadeHandMask()&(1u<<h)){held[h]=pressed[h]=false;nearTemple[h]=false;}
		if (!held[h]) headGripConsumed[h] = false;
		else if (nearTemple[h]) headGripConsumed[h] = true;
	}
	const bool toggle = headTouch.Update(nearTemple, held, pressed, dt);
	if (toggle) {
        flashlightPulse=.12f;
		for (unsigned h = 0; h < 2; ++h) if (nearTemple[h] && held[h])
			vr->TriggerHapticVibration(h == 0 ? ControllerRole::Left : ControllerRole::Right, 0, 0.035f, 100, 0.25f);
	}
	const bool pulse=flashlightPulse>0;
    flashlightPulse=(std::max)(0.f,flashlightPulse-(std::min)(.1f,(std::max)(0.f,dt)));
    return pulse?127:0;
}

unsigned char InputHandler::UpdateHolsterSwitchWeapons()
{
	if (headGripConsumed[Game::instance.bLeftHanded ? 0 : 1]) return 0;
	IVR* vr = Game::instance.GetVR();

	Matrix4 headTransform = vr->GetHMDTransform();

	// Calculate shoulder holster positions with the correct offset
	Vector3 leftShoulderPos = headTransform * Game::instance.c_LeftShoulderHolsterOffset->Value();
	Vector3 rightShoulderPos = headTransform * Game::instance.c_RightShoulderHolsterOffset->Value();

	Vector3 handPos;
	if (Game::instance.bLeftHanded)
	{
		handPos = vr->GetRawControllerTransform(ControllerRole::Left) * Vector3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		handPos = vr->GetRawControllerTransform(ControllerRole::Right) * Vector3(0.0f, 0.0f, 0.0f);
	}

	if (InputHandler::IsHandInHolster(handPos, leftShoulderPos, Game::instance.c_LeftShoulderHolsterActivationDistance->Value())
		|| InputHandler::IsHandInHolster(handPos, rightShoulderPos, Game::instance.c_RightShoulderHolsterActivationDistance->Value()))
	{
		return 127;
	}

	return 0;
}

// Helper function to check if a hand is in a holster
bool InputHandler::IsHandInHolster(const Vector3& handPos, const Vector3& holsterPos, const float& holsterActivationDistance)
{
	return (holsterPos - handPos).lengthSqr() < holsterActivationDistance * holsterActivationDistance;
}

unsigned char InputHandler::UpdateMelee()
{
	IVR* vr = Game::instance.GetVR();

	Vector3 handVel = vr->GetControllerVelocity(ControllerRole::Left);

	handVel *= Game::instance.WorldToMetres(1.0f);

	if (!Game::instance.interactions.BlocksMelee(0) && Game::instance.c_LeftHandMeleeSwingSpeed->Value() > 0.0f && abs(handVel.z) > Game::instance.c_LeftHandMeleeSwingSpeed->Value())
	{
		return 127;
	}

	handVel = vr->GetControllerVelocity(ControllerRole::Right);

	handVel *= Game::instance.WorldToMetres(1.0f);

	if (!Game::instance.interactions.BlocksMelee(1) && Game::instance.c_RightHandMeleeSwingSpeed->Value() > 0.0f && abs(handVel.z) > Game::instance.c_RightHandMeleeSwingSpeed->Value())
	{
		return 127;
	}

    return 0;
}

unsigned char InputHandler::UpdateCrouch()
{
	IVR* vr = Game::instance.GetVR();

	Vector3 headPos = vr->GetHMDTransform() * Vector3(0.0f, 0.0f, 0.0f);

	float crouchHeight = Game::instance.c_CrouchHeight->Value();

	if (crouchHeight < 0)
	{
		return 0;
	}

	if (headPos.z < -crouchHeight)
	{
		return 127;
	}

	return 0;
}

void InputHandler::SetMousePosition(int& x, int& y)
{
	if (!Game::instance.GetVR()->HasMousePointer()) return;
	Vector2 mousePos = Game::instance.GetVR()->GetMousePos();
	// Best I can tell the mouse is always scaled to a 640x480 canvas
	x = static_cast<int>(mousePos.x * 640);
	y = static_cast<int>(mousePos.y * 480);
}

void InputHandler::UpdateMouseInfo(MouseInfo* mouseInfo)
{
	const bool wasVRDown = mouseDownState != 0;
	if (Game::instance.GetVR()->GetMouseDown())
	{
		if (mouseDownState < 255)
		{
			mouseDownState++;
		}
	}
	else
	{
		mouseDownState = 0;
	}

	// Preserve real mouse input when VR is not clicking; clear our own release.
	if (mouseDownState != 0 || wasVRDown) mouseInfo->buttonState[0] = mouseDownState;
}

bool InputHandler::GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand)
{
	ControllerRole dominant = Game::instance.bLeftHanded ? ControllerRole::Left : ControllerRole::Right;
	ControllerRole nonDominant = Game::instance.bLeftHanded ? ControllerRole::Right : ControllerRole::Left;

	controllerTransform = Game::instance.GetVR()->GetControllerTransform(dominant, true);

	Vector3 poseDirection;
	bool bHasPoseData = Game::instance.GetVR()->TryGetControllerFacing(dominant, poseDirection);

	// When 2h aiming point the main hand at the offhand
	if (Game::instance.bUseTwoHandAim || bHasPoseData)
	{
		Matrix4 aimingTransform = Game::instance.GetVR()->GetRawControllerTransform(dominant, true);
		Matrix4 offHandTransform = Game::instance.GetVR()->GetRawControllerTransform(nonDominant, true);

		const Vector3 actualControllerPos = controllerTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 mainHandPos = aimingTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 offHandPos = Game::instance.bUseTwoHandAim ? offHandTransform * Vector3(0.0f, 0.0f, 0.0f) : mainHandPos + poseDirection;
		Vector3 toOffHand = (offHandPos - mainHandPos);

		// Avoid NaN errors
		if (toOffHand.lengthSqr() < 1e-8)
		{
			return false;
		}

		toOffHand.normalize();
		dominantHandPos = actualControllerPos;
		offHand = toOffHand;

		return true;
	}

	return false;
}

void InputHandler::CalculateSmoothedInput()
{
	Matrix4 controllerTransform;
	Vector3 actualControllerPos;
	Vector3 toOffHand;

	if (!GetCalculatedHandPositions(controllerTransform, actualControllerPos, toOffHand))
	{
		return;
	}

	if (!bLastYawInitialized)
	{
		lastSmoothingYawOffset = Game::instance.GetVR()->GetYawOffset();
		bLastYawInitialized = true;
	}

	// Detect snap turns and rotate smoothed position to prevent lerping artifact
	float currentYawOffset = Game::instance.GetVR()->GetYawOffset();
	float yawDelta = currentYawOffset - lastSmoothingYawOffset;

	// Detect snap turns and rotate smoothed position
	Helpers::RotateForSnapTurn(smoothedPosition, yawDelta, Game::instance.c_SnapTurnAmount->Value());

	// Update tracked yaw offset for next frame
	lastSmoothingYawOffset = currentYawOffset;

	float userInput = 0.0f;
	short zoom = Helpers::GetInputData().zoomLevel;

	if (zoom == -1)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountNoZoom->Value();
	}
	else if (zoom == 0)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountOneZoom->Value();
	}
	else if (zoom == 1)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountTwoZoom->Value();
	}

	float clampedValue = std::clamp(userInput, 0.0f, 2.0f);
	if (clampedValue == 0.0f)
	{
		smoothedPosition = actualControllerPos + toOffHand;
	}
	else
	{
		const float scaleFactor = (-20.0f / 9.0f);

		float h = 90.0f * log2(1.0f - exp(clampedValue * scaleFactor));

		float t = 1.0f - pow(2.0f, Game::instance.lastDeltaTime * h);

		smoothedPosition = Helpers::Lerp(smoothedPosition, actualControllerPos + toOffHand, t);
	}
}

void InputHandler::UpdateHandsProximity()
{
	float swapHandDistance = Game::instance.c_SwapHandDistance->Value();

	const Vector3 leftPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Left, true) * Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 rightPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Right, true) * Vector3(0.0f, 0.0f, 0.0f);
	float handDistance = (rightPos - leftPos).lengthSqr();

	bool handsWithinSwapWeaponDistance = false;
	if (swapHandDistance >= 0.0f && handDistance < swapHandDistance * swapHandDistance)
	{
		handsWithinSwapWeaponDistance = true;
		CheckSwapWeaponHand();
	}

	UpdateTwoHandedHold(handDistance, handsWithinSwapWeaponDistance);
}

void InputHandler::CheckSwapWeaponHand()
{
	if (headGripConsumed[0] || headGripConsumed[1]) return;
	IVR* vr = Game::instance.GetVR();

	bool bWeaponHandChanged;
	bool bOffhandWeaponHandChanged;
	bool bIsSwitchHandsPressed = vr->GetBoolInput(SwapWeaponHand, bWeaponHandChanged);
	bool bIsOffhandSwitchHandsPressed = vr->GetBoolInput(OffhandSwapWeaponHand, bOffhandWeaponHandChanged);

	bool offHandGrabbedWeapon = false;
	bool dominantHandReleasedWeapon = false;

    if (!Game::instance.bLeftHanded)
    {
		offHandGrabbedWeapon = bIsSwitchHandsPressed && bWeaponHandChanged && !bIsOffhandSwitchHandsPressed;
		dominantHandReleasedWeapon = bIsSwitchHandsPressed && !bIsOffhandSwitchHandsPressed && bOffhandWeaponHandChanged;
    }
    else
    {
        offHandGrabbedWeapon = bIsOffhandSwitchHandsPressed && bOffhandWeaponHandChanged && !bIsSwitchHandsPressed;
		dominantHandReleasedWeapon = bIsOffhandSwitchHandsPressed && !bIsSwitchHandsPressed && bWeaponHandChanged;
    }

	if (offHandGrabbedWeapon || dominantHandReleasedWeapon)
    {
		// Enable left handed and update the bindings to use the relevant action set
        Game::instance.bLeftHanded = !Game::instance.bLeftHanded;

		UpdateRegisteredInputs();
    }
}

void InputHandler::UpdateTwoHandedHold(float handDistance, bool handsWithinSwapWeaponDistance)
{
	if (headGripConsumed[Game::instance.bLeftHanded ? 1 : 0]) {
		Game::instance.bUseTwoHandAim = false;
		bWasGripping = false;
		return;
	}
	// Two hand aim is disabled when 3DOF is enabled.
	if (Game::instance.bUse3DOFAiming) {
		Game::instance.bUseTwoHandAim = false;
		return;
	}

	IVR* vr = Game::instance.GetVR();

	bool bGripChanged;
	bool bIsGripping = vr->GetBoolInput(TwoHandGrip, bGripChanged);

	if (handsWithinSwapWeaponDistance)
	{
		if (!bIsGripping) {
	        Game::instance.bUseTwoHandAim = false;
	    }
		return;
	}

	if (Game::instance.c_ToggleGrip->Value())
	{
	    if (bGripChanged && bIsGripping)
	    {
	        bWasGripping ^= true;
	    }
	    bIsGripping = bWasGripping;
	}

	float twoHandDistance = Game::instance.c_TwoHandDistance->Value();
	if (twoHandDistance >= 0.0f)
	{
	    if (bGripChanged)
	    {
	        if (bIsGripping)
	        {
				if (handDistance < twoHandDistance * twoHandDistance)
	            {
	                Game::instance.bUseTwoHandAim = true;
	            }
	        }
	        else
	        {
	            Game::instance.bUseTwoHandAim = false;
	        }
	    }
	}
	else
	{
	    Game::instance.bUseTwoHandAim = bIsGripping;
	}
}

#undef ApplyBoolInput
#undef ApplyImpulseBoolInput
#undef RegisterBoolInput
#undef RegisterVector2Input
