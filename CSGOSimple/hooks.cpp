#include "hooks.hpp"
#include <intrin.h>  

#include "render.hpp"
#include "menu.hpp"
#include "options.hpp"
#include "helpers/input.hpp"
#include "helpers/utils.hpp"
#include "features/bhop.hpp"
#include "features/chams.hpp"
#include "features/visuals.hpp"
#include "features/glow.hpp"
#pragma intrinsic(_ReturnAddress)  

Hooks::hkCreateMove::fn create_move_o = nullptr;
Hooks::surface::fn lock_cursor_o = nullptr;
Hooks::draw_model::fn draw_model_original = nullptr;
Hooks::paint_traverse::fn paint_traverse_original = nullptr;
Hooks::do_post_effects::fn do_post_effects_o = nullptr;

IDirect3DVertexDeclaration9* vert_declaration{ nullptr };
IDirect3DVertexShader9* vert_shader{ nullptr };
DWORD old_d3drs_colorwriteenable{ NULL };

namespace Hooks {

	void Initialize()
	{
		/*hlclient_hook.setup(g_CHLClient);
		direct3d_hook.setup(g_D3DDevice9);
		vguipanel_hook.setup(g_VGuiPanel);
		vguisurf_hook.setup(g_VGuiSurface);
		sound_hook.setup(g_EngineSound);
		mdlrender_hook.setup(g_MdlRender);
		clientmode_hook.setup(g_ClientMode);
		ConVar* sv_cheats_con = g_CVar->FindVar("sv_cheats");
		sv_cheats.setup(sv_cheats_con);

		direct3d_hook.hook_index(index::EndScene, hkEndScene);
		direct3d_hook.hook_index(index::Reset, hkReset);
		hlclient_hook.hook_index(index::FrameStageNotify, hkFrameStageNotify);
		hlclient_hook.hook_index(index::CreateMove, hkCreateMove_Proxy);
		vguipanel_hook.hook_index(index::PaintTraverse, hkPaintTraverse);
		sound_hook.hook_index(index::EmitSound1, hkEmitSound1);
		vguisurf_hook.hook_index(index::LockCursor, hkLockCursor);
		mdlrender_hook.hook_index(index::DrawModelExecute, hkDrawModelExecute);
		clientmode_hook.hook_index(index::DoPostScreenSpaceEffects, hkDoPostScreenEffects);
		clientmode_hook.hook_index(index::OverrideView, hkOverrideView);
		sv_cheats.hook_index(index::SvCheatsGetBool, hkSvCheatsGetBool);*/

		const auto create_move_target = reinterpret_cast<void*>(get_virtual(g_ClientMode, 24));
		const auto lock_cursor_target = reinterpret_cast<void*>(get_virtual(g_VGuiSurface, 67));
		const auto end_scene_target = reinterpret_cast<void*>(get_virtual(g_D3DDevice9, 42));
		const auto draw_model_target = reinterpret_cast<void*>(get_virtual(g_MdlRender, 21));
		const auto paint_traverse_target = reinterpret_cast<void*>(get_virtual(g_VGuiPanel, 41));
		const auto do_post_effects_t = reinterpret_cast<void*>(get_virtual(g_ClientMode, 44));

		//const auto end_scene_target = **reinterpret_cast<void***>(Utils::PatternScan("shaderapidx9.dll", "A1 ?? ?? ?? ?? 50 8B 08 FF 51 0C") + 1);
		const auto reset_target = **reinterpret_cast<void***>(Utils::PatternScan("gameoverlayrenderer.dll", "FF 15 ? ? ? ? 8B F8 85 FF 78 18") + 2);
		//const auto present_target = **reinterpret_cast<void***>(Utils::PatternScan("gameoverlayrenderer.dll", "FF 15 ? ? ? ? 8B F8 85 DB") + 2);

		if (MH_Initialize() != MH_OK)
			throw std::runtime_error("failed to initialize MH_Initialize.");

		if (MH_CreateHook(end_scene_target, static_cast<LPVOID>(&menu::hkEndScene), reinterpret_cast<void**>(&menu::end_scene_original)) != MH_OK)
			throw std::runtime_error("failed to initialize end_scene. (outdated imgui?)");

		if (MH_CreateHook(reset_target, static_cast<LPVOID>(&menu::reset), reinterpret_cast<void**>(&menu::reset_original)) != MH_OK)
			throw std::runtime_error("failed to initialize reset. (outdated imgui?)");

		/*if (MH_CreateHook(present_target, static_cast<LPVOID>(&menu::present), reinterpret_cast<void**>(&menu::present_original)) != MH_OK)
			throw std::runtime_error("failed to initialize present. (outdated imgui?)");*/

		if (MH_CreateHook(create_move_target, &hkCreateMove::hook, reinterpret_cast<void**>(&create_move_o)) != MH_OK)
			throw std::runtime_error("failed to initialize create_move. (outdated index?)");

		if (MH_CreateHook(lock_cursor_target, &surface::hook, reinterpret_cast<void**>(&lock_cursor_o)) != MH_OK)
			throw std::runtime_error("failed to initialize lock_cursor. (outdated index?)");

		if (MH_CreateHook(paint_traverse_target, &paint_traverse::hook, reinterpret_cast<void**>(&paint_traverse_original)) != MH_OK)
			throw std::runtime_error("failed to initialize paint_traverse. (outdated index?)");

		if (MH_CreateHook(draw_model_target, &draw_model::hook, reinterpret_cast<void**>(&draw_model_original)) != MH_OK)
			throw std::runtime_error("failed to initialize draw_model. (outdated index?)");

		if (MH_CreateHook(do_post_effects_t, &do_post_effects::hook, reinterpret_cast<void**>(&do_post_effects_o)) != MH_OK)
			throw std::runtime_error("failed to initialize do_post_effects. (outdated index?)");

		if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
			throw std::runtime_error("failed to enable hooks.");
	}
	//--------------------------------------------------------------------------------
	void Shutdown()
	{
		/*hlclient_hook.unhook_all();
		direct3d_hook.unhook_all();
		vguipanel_hook.unhook_all();
		vguisurf_hook.unhook_all();
		mdlrender_hook.unhook_all();
		clientmode_hook.unhook_all();
		sound_hook.unhook_all();
		sv_cheats.unhook_all();

		Glow::Get().Shutdown();*/

		MH_Uninitialize();

		MH_DisableHook(MH_ALL_HOOKS);
		g_InputSystem->EnableInput(true);

		/*ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();*/
	}
	//--------------------------------------------------------------------------------
	long __stdcall Hooks::menu::hkEndScene(IDirect3DDevice9* pDevice)
	{
		static auto viewmodel_fov = g_CVar->FindVar("viewmodel_fov");
		static auto mat_ambient_light_r = g_CVar->FindVar("mat_ambient_light_r");
		static auto mat_ambient_light_g = g_CVar->FindVar("mat_ambient_light_g");
		static auto mat_ambient_light_b = g_CVar->FindVar("mat_ambient_light_b");
		static auto crosshair_cvar = g_CVar->FindVar("crosshair");

		viewmodel_fov->m_fnChangeCallbacks.m_Size = 0;
		viewmodel_fov->SetValue(g_Options.viewmodel_fov);
		mat_ambient_light_r->SetValue(g_Options.mat_ambient_light_r);
		mat_ambient_light_g->SetValue(g_Options.mat_ambient_light_g);
		mat_ambient_light_b->SetValue(g_Options.mat_ambient_light_b);
		
		crosshair_cvar->SetValue(!(g_Options.esp_enabled && g_Options.esp_crosshair));

		DWORD colorwrite, srgbwrite;
		IDirect3DVertexDeclaration9* vert_dec = nullptr;
		IDirect3DVertexShader9* vert_shader = nullptr;
		DWORD dwOld_D3DRS_COLORWRITEENABLE = NULL;
		pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &colorwrite);
		pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbwrite);

		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		//removes the source engine color correction
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);

		pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &dwOld_D3DRS_COLORWRITEENABLE);
		pDevice->GetVertexDeclaration(&vert_dec);
		pDevice->GetVertexShader(&vert_shader);
		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		pDevice->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		pDevice->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, NULL);
		
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		auto esp_drawlist = Render::Get().RenderScene();

		Menu::Get().Render();

		ImGui::Render(esp_drawlist);

		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

		static uintptr_t gameoverlay_return_address = 0;

		if (!gameoverlay_return_address) {
			MEMORY_BASIC_INFORMATION info;
			VirtualQuery(_ReturnAddress(), &info, sizeof(MEMORY_BASIC_INFORMATION));

			char mod[MAX_PATH];
			GetModuleFileNameA((HMODULE)info.AllocationBase, mod, MAX_PATH);

			if (strstr(mod, ("gameoverlay")))
				gameoverlay_return_address = (uintptr_t)(_ReturnAddress());
		}

		if (gameoverlay_return_address != (uintptr_t)(_ReturnAddress()))
			return end_scene_original(pDevice);

		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, colorwrite);
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbwrite);
		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, dwOld_D3DRS_COLORWRITEENABLE);
		pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, true);
		pDevice->SetVertexDeclaration(vert_dec);
		pDevice->SetVertexShader(vert_shader);

		return menu::end_scene_original(pDevice);
	}
	//--------------------------------------------------------------------------------
	long __stdcall Hooks::menu::reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		Menu::Get().OnDeviceLost();

		auto hr = Hooks::menu::reset_original(device, pPresentationParameters);

		if (hr >= 0)
			Menu::Get().OnDeviceReset();

		return hr;
	}
	//--------------------------------------------------------------------------------
	/*HRESULT __stdcall Hooks::menu::present(IDirect3DDevice9* device, const RECT* src, const RECT* dest, HWND window_override, const RGNDATA* dirty_region) {
		// Save state
		device->GetRenderState(D3DRS_COLORWRITEENABLE, &old_d3drs_colorwriteenable);
		device->GetVertexDeclaration(&vert_declaration);
		device->GetVertexShader(&vert_shader);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		device->SetSamplerState(NULL, D3DSAMP_SRGBTEXTURE, NULL);
		*/
		/*uncomment this if you wish to disable anti aliasing. see gui.cpp too
		device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
		device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );*/
	/*
		ImGui_ImplDX9_Init(device);

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		auto esp_drawlist = Render::Get().RenderScene();

		Menu::Get().Render();

		ImGui::EndFrame();
		ImGui::Render();

		if (device->BeginScene() == D3D_OK) {

			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			device->EndScene();
		}

		// Restore state
		device->SetRenderState(D3DRS_COLORWRITEENABLE, old_d3drs_colorwriteenable);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, true);
		device->SetVertexDeclaration(vert_declaration);
		device->SetVertexShader(vert_shader);

		return present_original(device, src, dest, window_override, dirty_region);
	}

	HRESULT __stdcall Hooks::menu::reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {

		ImGui_ImplDX9_InvalidateDeviceObjects();
		return reset_original(device, params);
	}*/

	bool __stdcall Hooks::hkCreateMove::hook(float input_sample_frametime, CUserCmd* cmd) {
		create_move_o(input_sample_frametime, cmd);

		if (!cmd || !cmd->command_number)
			return false;

		uintptr_t* frame_pointer;
		__asm mov frame_pointer, ebp;
		bool& send_packet = *reinterpret_cast<bool*>(*frame_pointer - 0x1C);

		if (Menu::Get().IsVisible())
			cmd->buttons &= ~IN_ATTACK;

		if (g_Options.misc_bhop)
			BunnyHop::OnCreateMove(cmd);

		// https://github.com/spirthack/CSGOSimple/issues/69
		if (g_Options.misc_showranks && cmd->buttons & IN_SCORE) // rank revealer will work even after unhooking, idk how to "hide" ranks  again
			g_CHLClient->DispatchUserMessage(CS_UM_ServerRankRevealAll, 0, 0, nullptr);

		return false;
	}

	void __stdcall Hooks::surface::hook() {
		if (Menu::Get().IsVisible()) {
			g_VGuiSurface->UnlockCursor();
			g_InputSystem->ResetInputState();
			return;
		}

		lock_cursor_o(g_VGuiSurface);
	}

	void __fastcall Hooks::draw_model::hook(void* _this, int edx, IMatRenderContext* ctx, const DrawModelState_t& state, const ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld)
	{
		if (g_MdlRender->IsForcedMaterialOverride() &&
			!strstr(pInfo.pModel->szName, "arms") &&
			!strstr(pInfo.pModel->szName, "weapons/v_")) {
			return draw_model_original(_this, edx, ctx, state, pInfo, pCustomBoneToWorld);
		}

		//Chams::Get().OnDrawModelExecute(ctx, state, pInfo, pCustomBoneToWorld);

		draw_model_original(_this, edx, ctx, state, pInfo, pCustomBoneToWorld);

		g_MdlRender->ForcedMaterialOverride(nullptr);
	}

	/*void __stdcall Hooks::hkCreateMove::hook(int sequence_number, float input_sample_frametime, bool active, bool& bSendPacket)
	{
		create_move_o(g_CHLClient, 0, sequence_number, input_sample_frametime, active);

		auto cmd = g_Input->GetUserCmd(sequence_number);
		auto verified = g_Input->GetVerifiedCmd(sequence_number);

		if (!cmd || !cmd->command_number)
			return;
		
		if (Menu::Get().IsVisible())
			cmd->buttons &= ~IN_ATTACK;

		if (g_Options.misc_bhop)
			BunnyHop::OnCreateMove(cmd);

		// https://github.com/spirthack/CSGOSimple/issues/69
		if (g_Options.misc_showranks && cmd->buttons & IN_SCORE) // rank revealer will work even after unhooking, idk how to "hide" ranks  again
			g_CHLClient->DispatchUserMessage(CS_UM_ServerRankRevealAll, 0, 0, nullptr);


		verified->m_cmd = *cmd;
		verified->m_crc = cmd->GetChecksum();
	}*/
	//--------------------------------------------------------------------------------
	/*__declspec(naked) void __fastcall Hooks::hkCreateMove_Proxy::hook(void* _this, int, int sequence_number, float input_sample_frametime, bool active)
	{
		__asm
		{
			push ebp
			mov  ebp, esp
			push ebx; not sure if we need this
			push esp
			push dword ptr[active]
			push dword ptr[input_sample_frametime]
			push dword ptr[sequence_number]
			call Hooks::hkCreateMove
			pop  ebx
			pop  ebp
			retn 0Ch
		}
	}*/
	//--------------------------------------------------------------------------------
	void __fastcall Hooks::paint_traverse::hook(void* _this, int edx, vgui::VPANEL panel, bool forceRepaint, bool allowForce)
	{
		static auto panelId = vgui::VPANEL{ 0 };

		paint_traverse_original(g_VGuiPanel, edx, panel, forceRepaint, allowForce);

		if (!panelId) {
			const auto panelName = g_VGuiPanel->GetName(panel);
			if (!strcmp(panelName, "FocusOverlayPanel")) {
				panelId = panel;
			}
		}
		else if (panelId == panel) 
		{
			//Ignore 50% cuz it called very often
			static bool bSkip = false;
			bSkip = !bSkip;

			if (bSkip)
				return;

			Render::Get().BeginScene();
		}
	}
	/*//--------------------------------------------------------------------------------
	void __fastcall hkEmitSound1(void* _this, int edx, IRecipientFilter& filter, int iEntIndex, int iChannel, const char* pSoundEntry, unsigned int nSoundEntryHash, const char *pSample, float flVolume, int nSeed, float flAttenuation, int iFlags, int iPitch, const Vector* pOrigin, const Vector* pDirection, void* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity, int unk) {
		static auto ofunc = sound_hook.get_original<decltype(&hkEmitSound1)>(index::EmitSound1);


		if (!strcmp(pSoundEntry, "UIPanorama.popup_accept_match_beep")) {
			static auto fnAccept = reinterpret_cast<bool(__stdcall*)(const char*)>(Utils::PatternScan(GetModuleHandleA("client.dll"), "55 8B EC 83 E4 F8 8B 4D 08 BA ? ? ? ? E8 ? ? ? ? 85 C0 75 12"));

			if (fnAccept) {

				fnAccept("");

				//This will flash the CSGO window on the taskbar
				//so we know a game was found (you cant hear the beep sometimes cause it auto-accepts too fast)
				FLASHWINFO fi;
				fi.cbSize = sizeof(FLASHWINFO);
				fi.hwnd = InputSys::Get().GetMainWindow();
				fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
				fi.uCount = 0;
				fi.dwTimeout = 0;
				FlashWindowEx(&fi);
			}
		}

		ofunc(g_EngineSound, edx, filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, nSeed, flAttenuation, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity, unk);

	}*/
	//--------------------------------------------------------------------------------
	int __fastcall Hooks::do_post_effects::hook(void* _this, int edx, int a1)
	{
		if (g_LocalPlayer && g_Options.glow_enabled)
			Glow::Get().Run();

		return do_post_effects_o(g_ClientMode, edx, a1);
	}
	//--------------------------------------------------------------------------------
	/*void __fastcall hkFrameStageNotify(void* _this, int edx, ClientFrameStage_t stage)
	{
		static auto ofunc = hlclient_hook.get_original<decltype(&hkFrameStageNotify)>(index::FrameStageNotify);
		// may be u will use it lol
		ofunc(g_CHLClient, edx, stage);
	}
	//--------------------------------------------------------------------------------
	void __fastcall hkOverrideView(void* _this, int edx, CViewSetup* vsView)
	{
		static auto ofunc = clientmode_hook.get_original<decltype(&hkOverrideView)>(index::OverrideView);

		if (g_EngineClient->IsInGame() && vsView)
			Visuals::Get().ThirdPerson();

		ofunc(g_ClientMode, edx, vsView);
	}
	//--------------------------------------------------------------------------------
	void __fastcall hkLockCursor(void* _this)
	{
		static auto ofunc = vguisurf_hook.get_original<decltype(&hkLockCursor)>(index::LockCursor);

		if (Menu::Get().IsVisible()) {
			g_VGuiSurface->UnlockCursor();
			g_InputSystem->ResetInputState();
			return;
		}
		ofunc(g_VGuiSurface);

	}
	//--------------------------------------------------------------------------------
	void __fastcall hkDrawModelExecute(void* _this, int edx, IMatRenderContext* ctx, const DrawModelState_t& state, const ModelRenderInfo_t& pInfo, matrix3x4_t* pCustomBoneToWorld)
	{
		static auto ofunc = mdlrender_hook.get_original<decltype(&hkDrawModelExecute)>(index::DrawModelExecute);

		if (g_MdlRender->IsForcedMaterialOverride() &&
			!strstr(pInfo.pModel->szName, "arms") &&
			!strstr(pInfo.pModel->szName, "weapons/v_")) {
			return ofunc(_this, edx, ctx, state, pInfo, pCustomBoneToWorld);
		}

		Chams::Get().OnDrawModelExecute(ctx, state, pInfo, pCustomBoneToWorld);

		ofunc(_this, edx, ctx, state, pInfo, pCustomBoneToWorld);

		g_MdlRender->ForcedMaterialOverride(nullptr);
	}

	
	
	bool __fastcall hkSvCheatsGetBool(PVOID pConVar, void* edx)
	{
		static auto dwCAM_Think = Utils::PatternScan(GetModuleHandleW(L"client.dll"), "85 C0 75 30 38 86");
		static auto ofunc = sv_cheats.get_original<bool(__thiscall *)(PVOID)>(13);
		if (!ofunc)
			return false;

		if (reinterpret_cast<DWORD>(_ReturnAddress()) == reinterpret_cast<DWORD>(dwCAM_Think))
			return true;
		return ofunc(pConVar);
	}*/
}
