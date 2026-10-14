"""Portable tests only; this does not compile or validate the Windows DLL."""
from pathlib import Path
import os
import subprocess
import tempfile
import json

root = Path(__file__).resolve().parents[1]
# Use the real public Game interface with protected-member doubles below.
game_header = (root / "HaloCEVR/Game.h").read_text()
reload_accessors = game_header[game_header.index("    bool ManualPumpSupportsWeapon()"):
                               game_header.index("    void SuspendManualReload()")]
with tempfile.TemporaryDirectory(prefix="halo-immersion-tests-") as folder:
    output = Path(folder) / ("interaction_model.exe" if os.name == "nt" else "interaction_model")
    compiler = os.environ.get("CXX", "g++")
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(root/"tests/body_depth.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=undefined", "-g", str(root / "tests/interaction_model.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=undefined", "-g", str(root / "tests/visual_interaction.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=undefined", "-g", str(root / "tests/weapon_session.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=undefined", "-g", str(root / "tests/look_history.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined", "-g", "-I", str(root),
                    str(root / "tests/native_model_pose.cpp"), str(root / "HaloCEVR/Maths/Matrices.cpp"), "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(root / "tests/hand_grip_pose.cpp"),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(root / "tests/holster_markers.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(root / "tests/reload_sound_segments.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    samples=(root / "HaloCEVR/ReloadSamples.cpp").read_text()
    samples=samples[samples.index("namespace {"):]
    sample_source=Path(folder)/"reload_samples.cpp"
    sample_source.write_text((root / "tests/reload_samples_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_SAMPLES",samples))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(sample_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    input_text=(root / "HaloCEVR/InputHandler.cpp").read_text()
    aim=input_text[input_text.index("bool InputHandler::GetCalculatedHandPositions("):input_text.index("void InputHandler::CalculateSmoothedInput()")]
    tutorial=input_text[input_text.index("void InputHandler::UpdateTutorialLook()"):input_text.index("Vector2 InputHandler::VehicleLookInput(")]
    tutorial_source=Path(folder)/"tutorial_look.cpp"
    tutorial_source.write_text((root/"tests/tutorial_look_fixture.cpp").read_text().replace("// INSERT_TUTORIAL_LOOK",tutorial))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(tutorial_source),str(root/"HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    camera_input=input_text[input_text.index("void InputHandler::UpdateCamera(float&"):input_text.index("void InputHandler::UpdateTutorialLook()")]
    camera_game=(root/"HaloCEVR/Game.cpp").read_text()
    camera_input+=camera_game[camera_game.index("void Game::UpdateCamera(float&"):camera_game.index("void Game::SetMousePosition(")]
    camera_source=Path(folder)/"camera_input.cpp"
    camera_source.write_text((root/"tests/camera_input_fixture.cpp").read_text().replace("// INSERT_CAMERA_INPUT",camera_input))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),str(camera_source),str(root/"HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    weapon_text=(root / "HaloCEVR/WeaponHandler.cpp").read_text()
    support=weapon_text[weapon_text.index("    // Compact weapons use a cupped support grip"):weapon_text.index("\n}\n\ninline void WeaponHandler::CalculateBoneTransform",weapon_text.index("    // Compact weapons use a cupped support grip"))]
    support_source=Path(folder)/"compact_support.cpp"
    support_source.write_text((root / "tests/compact_support_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_AIM",aim).replace("// INSERT_PRODUCTION_SUPPORT_POSE",support))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(support_source),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    guide=(root/"HaloCEVR/UI/SettingsMenu.cpp").read_text()
    methods=guide[guide.index("void SettingsMenu::PopulateGuideTab("):guide.index("void SettingsMenu::PopulateAboutTab(")]
    methods+=guide[guide.index("void SettingsMenu::NextPage("):guide.index("void SettingsMenu::ReloadSettingsPages(")]
    guide_source=Path(folder)/"player_guide.cpp"
    guide_source.write_text((root/"tests/player_guide_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_GUIDE",methods))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-Wno-unused-function","-fsanitize=undefined","-I",str(root),str(guide_source),"-o",str(output)],check=True)
    subprocess.run([str(output),str(root/"Extras/Fonts/normal.ttf")],check=True)

    # Compile the actual production gesture adapter with small engine doubles.
    audio = (root / "HaloCEVR/ReloadAudio.cpp").read_text()
    audio = audio[audio.index("namespace {"):]
    a=audio.index("bool Fingerprint("); b=audio.index("bool __cdecl DeferSound",a)
    audio=audio[:a]+"bool Fingerprint(const Immersion::CodeFingerprint&){return fingerprintPass;}\n"+audio[b:]
    a=audio.index("void __declspec(naked) OnSound");b=audio.index("\n}\nbool InitializeReloadAudio",a)
    audio=audio[:a]+"void* OnSound=nullptr;void* OnEffect=nullptr;\n"+audio[b:]
    import re
    replacements=["NativeStop(handle);", "result=NativeSound(weapon,sound,volume);", "NativeEffect(weapon,effect);"]
    for replacement in replacements:
        audio=re.sub(r"__asm\s*\{[^}]*\}",lambda _:replacement,audio,count=1)
    audio=audio.replace("const std::uintptr_t function=0x549ee0;", "")
    audio_source=Path(folder)/"native_audio.cpp"
    audio_source.write_text((root / "tests/reload_audio_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_RELOAD_ACCESSORS",reload_accessors).replace("// INSERT_PRODUCTION_AUDIO",audio))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(audio_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    reload = (root / "HaloCEVR/ReloadHandler.cpp").read_text()
    reload = reload[reload.index("using namespace Immersion;"):]
    reload_source = Path(folder) / "manual_reload.cpp"
    reload_source.write_text((root / "tests/manual_reload_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_RELOAD",reload))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(reload_source),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    # Compile the full new adapter; only D3D's matrix POD needs a stub here.
    stubs=Path(folder)/"stubs";stubs.mkdir(exist_ok=True)
    (stubs/"d3d9.h").write_text("#pragma once\nstruct D3DMATRIX{float m[4][4];};\n")
    interactions=(root/"HaloCEVR/PhysicalInteractionHandler.cpp").read_text()
    interactions=interactions[interactions.index("using namespace Immersion;"):]
    interaction_source=Path(folder)/"physical_interactions.cpp"
    melee=input_text[input_text.index("unsigned char InputHandler::UpdateMelee()"):input_text.index("unsigned char InputHandler::UpdateCrouch()")]
    interaction_source.write_text((root/"tests/physical_interactions_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_INTERACTIONS",interactions).replace("// INSERT_PRODUCTION_MELEE",melee))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(stubs),"-I",str(root),
                    str(interaction_source),str(root/"HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    source = (root / "HaloCEVR/InputHandler.cpp").read_text()
    game = (root / "HaloCEVR/Game.cpp").read_text()
    begin = game.index("void Game::QueuePhysicalHolsterLabels()")
    end = game.index("\nvoid Game::PreDrawEye",begin+1)
    startup_source = Path(folder) / "startup.cpp"
    startup_source.write_text((root / "tests/empty_hands_startup_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_METHOD",game[begin:end]))
    subprocess.run([compiler, "-std=c++17", "-fsanitize=undefined", "-g", "-I", str(root),
                    str(startup_source), str(root / "HaloCEVR/Maths/Matrices.cpp"), "-o", str(output)],check=True)
    subprocess.run([str(output)],check=True)
    begin = game.index("bool Game::PreDrawCrosshair(")
    end = game.index("void Game::PreDrawImage",begin)
    startup_source.write_text((root / "tests/hud_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_HUD",game[begin:end]))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined",str(startup_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    begin = source.index("unsigned char InputHandler::UpdateGripFlashlight()")
    end = source.index("unsigned char InputHandler::UpdateHolsterSwitchWeapons()", begin)
    fixture = (root / "tests/flashlight_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_METHOD", source[begin:end])
    test_source = Path(folder) / "flashlight.cpp"
    test_source.write_text(fixture)
    subprocess.run([compiler, "-std=c++17", "-fsanitize=undefined", "-g", "-I", str(root),
                    str(test_source), str(root / "HaloCEVR/Maths/Matrices.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    begin = source.index("void InputHandler::InitializePhysicalWeapons()")
    end = source.index("void InputHandler::UpdateMenuInput()", begin)
    fixture = (root / "tests/physical_input_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_RELOAD_ACCESSORS",reload_accessors).replace("// INSERT_PRODUCTION_METHODS", source[begin:end])
    test_source.write_text(fixture)
    subprocess.run([compiler, "-std=c++17", "-fsanitize=undefined", "-g", "-I", str(root),
                    str(test_source), str(root / "HaloCEVR/Maths/Matrices.cpp"),
                    "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    # Preserve the native Escape state through the complete production input pass.
    menu_header=(root / "HaloCEVR/InputHandler.h").read_text()
    menu_state=menu_header[menu_header.index("bool bHoldingMenu"):menu_header.index("bool bWasGripping")]
    menu_methods=source[source.index("void InputHandler::UpdatePhysicalButtons()"):source.index("void InputHandler::ReportPhysicalStatus(")]
    menu_methods+=source[source.index("void InputHandler::UpdateMenuInput()"):source.index("void InputHandler::UpdateCamera(")]
    test_source.write_text((root / "tests/menu_input_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_MENU_STATE",menu_state).replace("// INSERT_PRODUCTION_MENU_METHODS",menu_methods))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(test_source),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    render = (root / "HaloCEVR/InGameRenderer.cpp").read_text()
    begin=render.index("void InGameRenderer::ExtractMatrices(")
    end=render.index("void InGameRenderer::Render(",begin)
    test_source.write_text((root/"tests/camera_projection_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_EXTRACTION",render[begin:end]))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(test_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    begin=render.index("void InGameRenderer::DrawMeshes(")
    end=render.index("void InGameRenderer::DrawLabel3D",begin)
    test_source.write_text((root / "tests/mesh_render_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_MESH",render[begin:end]))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined",
                    str(test_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    begin=render.index("void InGameRenderer::DrawLabel3D(")
    end=render.index("//================================",begin)
    test_source.write_text((root / "tests/holster_labels_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_LABELS",render[begin:end]))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(test_source),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    cache = (root / "HaloCEVR/WeaponHandler.cpp").read_text()
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(root / "tests/recoil.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    methods = cache[cache.index("namespace {\ndouble RecoilTime()"):cache.index("bool WeaponHandler::GetWorldWeaponAim(")]
    methods += cache[cache.index("Vector3 WeaponHandler::GetScopeLocation("):cache.index("namespace {\ndouble RecoilTime()")]
    methods += cache[cache.index("bool WeaponHandler::GetLocalWeaponScope("):cache.index("bool WeaponHandler::GetWorldWeaponScope(")]
    methods += cache[cache.index("void WeaponHandler::RelocatePlayer("):cache.index("void WeaponHandler::SetPlasmaPistolCharge()")]
    methods += cache[cache.index("void WeaponHandler::PostFireWeapon("):cache.index("void WeaponHandler::PreThrowGrenade(")]
    test_source.write_text((root / "tests/recoil_adapter_fixture.cpp").read_text().replace("// INSERT_PRODUCTION_RECOIL",methods))
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-Wno-unused-parameter","-fsanitize=undefined","-I",str(root),
                    str(test_source),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)
    begin=cache.index("bool WeaponHandler::UpdateCache(")
    end=cache.index("\n}",cache.index("inline WeaponType WeaponHandler::GetWeaponType",begin))+2
    test_source.write_text((root / "tests/viewmodel_cache_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_CACHE",cache[begin:end]))
    subprocess.run([compiler,"-std=c++17","-fsanitize=undefined","-g","-I",str(root),
                    str(test_source),str(root / "HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

    native = (root / "HaloCEVR/Immersion/RetailWeaponEngine.cpp").read_text()
    helpers = native[native.index("bool Copy("):native.index("bool Store(")]
    helpers += native[native.index("template<std::size_t N>"):native.index("void RefreshViewModel(")]
    helpers += native[native.index("bool TagData("):native.index("void* ObjectLighting(")]
    begin = native.index("bool RetailWeaponEngine::ReadNative(")
    end = native.index("bool RetailWeaponEngine::Select(", begin)
    fixture = (root / "tests/native_reader_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_VISUAL\n", native[native.index("bool RetailWeaponEngine::Select("):native.index("NativeDrop RetailWeaponEngine::Drop")]).replace(
        "// INSERT_PRODUCTION_VEHICLE_DRAW_HELPER", native[native.index("const Transform* __cdecl PrepareVehicleModelDraw"):native.index("void __declspec(naked) OnModelDraw")]).replace(
        "// INSERT_PRODUCTION_VISUAL_GUARDS", native[native.index("void __cdecl OnPose()"):native.index("void __cdecl OnWorldItem")]).replace(
        "// INSERT_PRODUCTION_READ_HELPERS", helpers).replace("// INSERT_PRODUCTION_READ", native[begin:end]).replace(
        "// INSERT_PRODUCTION_PICKUP", native[native.index("bool RetailWeaponEngine::NewLooseWeapon("):native.index('#include "RetailInteractions.inl"')]).replace("// INSERT_PRODUCTION_INTERACTION_ENGINE",(root/"HaloCEVR/Immersion/RetailInteractions.inl").read_text()).replace("// INSERT_PRODUCTION_ACCURACY",(root/"HaloCEVR/Immersion/RetailAccuracy.inl").read_text())
    test_source.write_text(fixture)
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                    "-g", "-I", str(root), str(test_source), str(root / "HaloCEVR/Maths/Matrices.cpp"), "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    openvr = (root / "HaloCEVR/VR/OpenVR.cpp").read_text()
    begin = openvr.index("void OpenVR::UpdateMenuPointer()")
    end = openvr.index("void OpenVR::UpdateSkeleton(",begin)
    fixture = (root / "tests/menu_pointer_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_METHOD",openvr[begin:end])
    test_source.write_text(fixture)
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
                    "-g", "-I", str(root), str(test_source), "-o", str(output)], check=True)
    subprocess.run([str(output)],check=True)
    methods = ""
    for start, end in [("void OpenVR::UpdateInputs()", "InputBindingID OpenVR::RegisterBoolInput"),
                       ("InputBindingID OpenVR::RegisterBoolInput", "InputBindingID OpenVR::RegisterVector2Input"),
                       ("bool OpenVR::TryGetBoolInput", "Vector2 OpenVR::GetVector2Input"),
                       ("InputBindingID OpenVR::RegisterVector2Input", "bool OpenVR::GetBoolInput"),
                       ("Vector2 OpenVR::GetVector2Input", "Vector2 OpenVR::GetMousePos")]:
        methods += openvr[openvr.index(start):openvr.index(end,openvr.index(start))]
    header = (root / "HaloCEVR/VR/OpenVR.h").read_text()
    state = header[header.index("struct PhysicalBinding"):header.index("vr::IVROverlay* vrOverlay;")]
    fixture = (root / "tests/openvr_input_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_METHODS", methods).replace("// INSERT_PRODUCTION_BINDING_STATE", state)
    test_source.write_text(fixture)
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                    "-fsanitize=undefined", "-g", "-I", str(root), str(test_source), "-o", str(output)], check=True)
    subprocess.run([str(output)], check=True)

    # Shared metre/world conversion and actual OpenVR stereo pose path.
    scaling=(root/"tests/world_scale_fixture.cpp").read_text().replace(
        "// INSERT_PRODUCTION_CONVERSION",game[game.index("float Game::WorldScale()"):game.index("void Game::CreateConsole()")]).replace(
        "// INSERT_PRODUCTION_CAMERA",openvr[openvr.index("void OpenVR::UpdateCameraFrustum("):openvr.index("int OpenVR::GetViewWidth()")])
    test_source.write_text(scaling)
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsanitize=undefined","-I",str(root),
                    str(test_source),str(root/"HaloCEVR/Maths/Matrices.cpp"),"-o",str(output)],check=True)
    subprocess.run([str(output)],check=True)

bindings = root / "Extras/Bindings"
manifest = json.loads((bindings / "actions.json").read_text())
names = [a["name"] for a in manifest["actions"]]
assert len(names) == len(set(names)), "Duplicate action names"
for entry in manifest["default_bindings"]:
    data = json.loads((bindings / entry["binding_url"]).read_text())
    sources = data["bindings"]["/actions/default"]["sources"]
    for side in ("Left", "Right"):
        for control in ("Grip", "Trigger"):
            action = f"/actions/default/in/Physical{side}{control}"
            assert action in names
            matching = [s for s in sources if s.get("inputs",{}).get("click",{}).get("output") == action]
            assert len(matching) == 1
            assert matching[0]["path"] == f"/user/hand/{side.lower()}/input/{control.lower()}"
    stick = "joystick" if entry["controller_type"] == "oculus_touch" else "thumbstick"
    cycle = "x" if entry["controller_type"] == "oculus_touch" else "a"
    for action, path, event in [
        ("PhysicalInteract", "/user/hand/right/input/b", "click"),
        ("PhysicalJump", "/user/hand/right/input/a", "click"),
        ("PhysicalGrenade", f"/user/hand/right/input/{stick}", "north"),
        ("PhysicalGrenadeCycle", f"/user/hand/left/input/{cycle}", "click"),
        ("PhysicalRecentre", f"/user/hand/left/input/{'y' if entry['controller_type']=='oculus_touch' else 'b'}", "click")]:
        full = f"/actions/default/in/{action}"
        assert full in names
        matching = [s for s in sources if s.get("inputs",{}).get(event,{}).get("output") == full]
        assert len(matching) == 1 and matching[0]["path"] == path, (entry, action)
print("Physical grip, trigger and fixed-hand button actions verified in all four controller profiles")

for entry in manifest["default_bindings"]:
    sources=json.loads((bindings/entry["binding_url"]).read_text())["bindings"]["/actions/default"]["sources"]
    for hand,action in [("left","PhysicalMove"),("right","PhysicalLook")]:
        matches=[x for x in sources if x.get("inputs",{}).get("position",{}).get("output")=="/actions/default/in/"+action]
        axis="joystick" if entry["controller_type"]=="oculus_touch" else "thumbstick"
        assert len(matches)==1 and matches[0]["path"]==f"/user/hand/{hand}/input/{axis}"
print("Fixed left/right vehicle sticks verified in all four controller profiles")
