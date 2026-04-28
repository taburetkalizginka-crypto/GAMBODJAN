#include "main.hpp"

#include "core/sdk_game/CDOTA_InventoryManager.hpp"
#include "core/offsets.h"

#include "gui/gui.hpp"
#include "core/hook/hook.hpp"
#include "gui/panorama_gui.h"

CGui* pGui = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;

// Soft check: log error but don't abort
#define SOFT_CHECK( var, name ) \
if(!(var)) { spdlog::error( "[FAIL] {} is nullptr\n", name ); std::cout.flush(); } else { spdlog::info( "[OK] {} = 0x{:X}\n", name, (std::uintptr_t)(var) ); std::cout.flush(); }

// Soft FIND_FN: log error but continue instead of showing MessageBox and returning
#define FIND_FN_SOFT( dll_name, fn, sig, fn_name, relative_call )\
fn = (decltype(fn))util::find_pattern( (util::fast_strcmp(dll_name, "client.dll")==0) ? global::client : (HMODULE)util::get_module_base_ansi( dll_name ), sig, fn_name ); \
if (relative_call && fn)\
	fn = (decltype(fn))util::get_absolute_address((uintptr_t)fn, 1,5); \
if(!fn) { spdlog::error( "[FAIL] {}\n", fn_name ); } \
std::cout.flush();

bool start_init( ) {
	std::uintptr_t aParticleManager = 0,
		aNetchanVMT = 0,
		aSteamGC = 0;
	std::uintptr_t** aParticleCollectionVMT = nullptr;
	spdlog::stopwatch start_init;

	spdlog::info( "Start init cheat...\n" );
	std::cout.flush( );

	const auto peb = reinterpret_cast<PPEB64>( __readgsqword( 0x60 ) );
	peb->BeingDebugged = 0;

	spdlog::info( "PEB: {} | IsDebuggerPresent(): {}\n", (void*)peb, IsDebuggerPresent( ) ? "true" : "false" );

	global::client = util::get_module_base_wchar( L"client.dll" );
	global::tier0 = util::get_module_base_wchar( L"tier0.dll" );

	if ( !global::client ) { spdlog::critical( "client.dll not found!\n" ); return false; }
	if ( !global::tier0 ) { spdlog::critical( "tier0.dll not found!\n" ); return false; }

	spdlog::info( "client.dll: 0x{:X}\n", (uintptr_t)global::client );
	spdlog::info( "tier0.dll: 0x{:X}\n", (uintptr_t)global::tier0 );
	std::cout.flush( );

	pGui = new CGui( );

	/*
	* STEP 1: Install Present hook FIRST so menu appears even if other things fail
	*/
	spdlog::info( "--- Installing Present hook (menu) ---\n" );
	std::cout.flush( );
	DETOUR_PATTERN( "GameOverlayRenderer64.dll", "48 89 6C 24 ?? 48 89 74 24 ?? 41 56 48 83 EC ?? 41 8B E8", "SteamOverlayPresent", Present, true, false );
	std::cout.flush( );

	/*
	* STEP 2: Steam API + FrameStageNotify (critical hooks)
	*/
	spdlog::info( "--- Installing critical hooks ---\n" );
	std::cout.flush( );

	ISteamClient::GetHSteamPipe = (decltype( ISteamClient::GetHSteamPipe ))util::find_export_address( util::get_module_base_ansi( "steam_api64.dll" ), "GetHSteamPipe" );
	ISteamClient::GetHSteamUser = (decltype( ISteamClient::GetHSteamUser ))util::find_export_address( util::get_module_base_ansi( "steam_api64.dll" ), "GetHSteamUser" );
	SOFT_CHECK( ISteamClient::GetHSteamPipe, "GetHSteamPipe" );
	SOFT_CHECK( ISteamClient::GetHSteamUser, "GetHSteamUser" );

	if ( ISteamClient::GetHSteamPipe && ISteamClient::GetHSteamUser ) {
		aSteamGC = (std::uintptr_t)ISteamClient::get( ).GetISteamGenericInterface( ISteamClient::GetHSteamPipe( ), ISteamClient::GetHSteamUser( ), "SteamGameCoordinator001" );
		SOFT_CHECK( aSteamGC, "SteamGC" );
	}

	DETOUR_PATTERN( "client.dll", "E8 ?? ?? ?? ?? 48 8B 4D 90 48 89 7C 24", "CGCClient::BAsyncSendProto", BAsyncSendProto, true, true );
	std::cout.flush( );
	DETOUR_PATTERN( "client.dll", "44 88 44 24 ?? 89 54 24 ?? 55 53 56 57 41 54", "CDOTAInput::CreateMove", CreateMove, true, false );
	std::cout.flush( );

	if ( aSteamGC )
		DETOUR_VF( aSteamGC, 2, SGCRetrieveMessage, false );

	DETOUR_VF( CSource2Client::get( ), 29, FrameStageNotify, false );
	std::cout.flush( );

	/*
	* STEP 3: Resolve core addresses using offsets
	*/
	spdlog::info( "--- Resolving core addresses (offsets) ---\n" );
	std::cout.flush( );

	{
		const auto pEntitySystemAddr = (uintptr_t)global::client + source2_dumper::offsets::client_dll::dwGameEntitySystem;
		g_pGameEntitySystem = *reinterpret_cast<CGameEntitySystem**>( pEntitySystemAddr );
		spdlog::info( "[OK] GameEntitySystem = 0x{:X} (offset 0x{:X})\n", (uintptr_t)g_pGameEntitySystem, source2_dumper::offsets::client_dll::dwGameEntitySystem );
		std::cout.flush( );

		if ( g_pGameEntitySystem ) {
			g_pEntityListener = CMemAlloc::GetInstance( )->allocate<EntityEventListener>( );
			g_pGameEntitySystem->m_vecEntityEvents.AddToTail( g_pEntityListener );
			spdlog::info( "[OK] Entity listener registered\n" );
		} else {
			spdlog::warn( "[WARN] GameEntitySystem is null - entity listener not registered\n" );
		}
		std::cout.flush( );
	}

	/*
	* STEP 4: Pattern scans (may hang on outdated sigs - non-critical)
	*/
	spdlog::info( "--- Scanning patterns (non-critical) ---\n" );
	std::cout.flush( );

	{
		spdlog::info( "Scanning client.dll for ParticleManager...\n" );
		std::cout.flush( );
		auto pmPattern = util::find_pattern( global::client, "7E 0B 41 8D 42 F1 A9 FB FF FF FF 75 0B 41 8B C9 E8 ?? ?? ?? ?? 48 8B D8 E8", "CDOTA_ParticleManager" );
		std::cout.flush( );
		if ( pmPattern ) {
			aParticleManager = GAB( pmPattern + 0x18, 1, 5 );
			if ( aParticleManager )
				global::patterns::DOTAParticleManager = GAB( aParticleManager, 3, 7 );
		}
		SOFT_CHECK( aParticleManager, "ParticleManager" );

		spdlog::info( "Scanning networksystem.dll for NetChannel VMT...\n" );
		std::cout.flush( );
		aNetchanVMT = util::find_pattern( "networksystem.dll", "40 53 56 57 41 56 48 83 EC ?? 45 33 F6 48 8D 71", "NetChannel VMT" );
		std::cout.flush( );
		SOFT_CHECK( aNetchanVMT, "NetchanVMT" );

		if ( aNetchanVMT ) {
			auto netchanPtr = GAB( aNetchanVMT + 0x15, 3, 7 );
			if ( netchanPtr ) {
				DETOUR_VF( netchanPtr, 86, PostReceivedNetMessage, true );
				DETOUR_VF( netchanPtr, 69, SendNetMessage, true );
			} else {
				spdlog::error( "[FAIL] NetchanVMT resolve failed\n" );
			}
		}

		spdlog::info( "Scanning particles.dll for ParticleCollection VMT...\n" );
		std::cout.flush( );
		auto pcPattern = util::find_pattern( "particles.dll", "48 8D 05 ?? ?? ?? ?? 48 89 01 0F 57 C0", "ParticleCollection VMT" );
		std::cout.flush( );
		if ( pcPattern )
			aParticleCollectionVMT = (uintptr_t**)GAB( pcPattern, 3, 7 );
		SOFT_CHECK( aParticleCollectionVMT, "ParticleCollectionVMT" );
	}

	spdlog::info( "--- Resolving functions (patterns) ---\n" );
	std::cout.flush( );

	spdlog::stopwatch start_funcs;
	{
		FIND_FN_SOFT( "client.dll", global::patterns::CSlider__SetValue, "40 57 48 83 EC ?? 0F 29 74 24 ?? 48 8B F9 F3 0F 10 71", "CPanel2D::SetValue", false );
		FIND_FN_SOFT( "client.dll", global::patterns::CDOTA_UI_HeroImage__SetHeroName, "48 89 5C 24 ?? 57 48 83 EC 20 48 8B FA 48 8B D9 E8 ?? ?? ?? ?? 80 B8 ?? ?? ?? ?? ?? 75 17 80 B8 ?? ?? ?? ?? ?? 75 0E 80 B8 ?? ?? ?? ?? ?? B9 ?? ?? ?? ?? 74 05 B9 ?? ?? ?? ?? 48 03 C8 48 8B D7 E8 ?? ?? ?? ?? 39 83", "CDOTA_UI_HeroImage::SetHeroName", false );
		FIND_FN_SOFT( "client.dll", global::patterns::CEconItem__DeserializeItemProtobuf, "E8 ?? ?? ?? ?? 41 0F B6 46 ?? A8 01", "CEconItem::DeserializeFromProtoBufItem", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CImagePanel__SetImage, "E8 ?? ?? ?? ?? EB 18 4E 8B 04 37", "CImagePanel::SetImage", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CPanel2D__DeleteAsync, "E8 ?? ?? ?? ?? 4C 89 7F 60", "CPanel2D::DeleteAsync", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CTextEntry__SetText, "E8 ?? ?? ?? ?? 48 8B 77 10", "CTextEntry::SetText", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CTextEntry__GetText, "E8 ?? ?? ?? ?? F3 0F 10 5D ?? 48 8B D0", "CTextEntry::GetText", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CDOTA_UI_AbilityImage__SetAbilityName, "E8 ?? ?? ?? ?? 4A 8B 04 E7", "CDOTA_UI_AbilityImage::SetAbilityName", true );
		FIND_FN_SOFT( "client.dll", global::patterns::CDOTA_UI_ItemImage__SetItemByName, "E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 8B D3 48 8B C8", "CDOTA_UI_ItemImage::SetItemByName", true );
		FIND_FN_SOFT( "client.dll", calls::GetSOCDataForItem, "48 89 54 24 ?? 57 48 83 EC 70", "CDOTAPlayerInventory::GetSOCDataForItem", false );
		FIND_FN_SOFT( "client.dll", calls::destroy_particle, "83 FA ?? 0F 84 ?? ?? ?? ?? 48 89 6C 24 18", "CDOTA_ParticleManager::DestroyParticle", false );
		FIND_FN_SOFT( "client.dll", calls::CreateEconItemObject, "48 83 EC ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8D 0D", "CreateEconItem", false );
		FIND_FN_SOFT( "client.dll", calls::GetCurrentCamera, "E8 ?? ?? ?? ?? 48 85 C0 74 ?? C6 80 84 02 00 00", "C_DOTACameraInit", true );
		FIND_FN_SOFT( "client.dll", calls::GetLevelSpecialValueFor, "E8 ?? ?? ?? ?? F3 0F 5A C0", "C_BaseEntity::FindLevelSpecialValueFor", true );
		FIND_FN_SOFT( "client.dll", CDOTAItemSchema::GetItemDefByIndex, "E8 ?? ?? ?? ?? 8B 4E 64", "CDOTAItemSchema::GetItemDefByIndex", true );
		FIND_FN_SOFT( "client.dll", C_DOTA_PlayerResource::aGetNetWorthOfPlayer, "E8 ?? ?? ?? ?? F3 0F 5F F7", "C_DOTA_PlayerResource::GetNetWorthOfPlayer", true );
		FIND_FN_SOFT( "client.dll", C_DOTA_PlayerResource::aGetLastHits, "E8 ?? ?? ?? ?? 3B F0 7C 5A", "C_DOTA_PlayerResource::GetLastHits", true );
		FIND_FN_SOFT( "client.dll", C_DOTA_PlayerResource::aGetDenies, "E8 ?? ?? ?? ?? 45 8B C6 89 44 24 44", "C_DOTA_PlayerResource::GetDenies", true );
		FIND_FN_SOFT( "client.dll", CDropDown::aCDropDown__GetSelected, "E8 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? 49 8B 8F", "CDropDown::GetSelected", true );

		if ( CDOTAItemSchema::GetItemDefByIndex )
			CDOTAItemSchema::GetItemDefArrIdx = AddressWrapper( CDOTAItemSchema::GetItemDefByIndex ).get_offset( 0x16 ).get_address_from_instruction_ptr( 1 );
	}

	const auto duration_funcs = start_funcs.elapsed( );
	spdlog::info( "Functions resolved in {:.2}s\n", duration_funcs.count( ) );
	std::cout.flush( );

	/*
	* STEP 5: Unlock console variables
	*/
	for ( auto& ccmd : ICVar::get( ).ccommands( ) ) {

		if ( !( &ccmd ) || !ccmd.m_name )
			continue;

		if ( ccmd.m_flags & FCVAR_DEVELOPMENTONLY )
			ccmd.m_flags &= ~FCVAR_DEVELOPMENTONLY;

		if ( ccmd.m_flags & FCVAR_HIDDEN )
			ccmd.m_flags &= ~FCVAR_HIDDEN;

		if ( ccmd.m_flags & FCVAR_CHEAT )
			ccmd.m_flags &= ~FCVAR_CHEAT;
	}

	for ( auto& [cvar_node, idx] : ICVar::get( ).cvars( ) )
	{
		if ( !cvar_node || !cvar_node->m_name )
			continue;

		if ( cvar_node->m_flags & FCVAR_DEVELOPMENTONLY )
			cvar_node->m_flags &= ~FCVAR_DEVELOPMENTONLY;

		if ( cvar_node->m_flags & FCVAR_HIDDEN )
			cvar_node->m_flags &= ~FCVAR_HIDDEN;

#ifdef _DEBUG
		cvar_node->m_flags &= FCVAR_NOTIFY;
#endif
	}

	spdlog::info( "Unlocked all console variables/commands\n" );
	spdlog::info( "Total init: {:.2}s\n", start_init.elapsed( ).count( ) );
	spdlog::info( "=== GAMBODJAN initialized (F1 - open menu) ===\n\n" );
	std::cout.flush( );

	return true;
}

static int safe_start_init( ) {
	__try {
		return start_init( ) ? 0 : 1;
	}
	__except ( EXCEPTION_EXECUTE_HANDLER ) {
		return (int)GetExceptionCode( );
	}
}

static DWORD WINAPI cheat_thread( LPVOID hModule ) {
	util::allocate_console( );
	util::clear_console( );
	setlocale( 0, "" );
	global::hModule = (HINSTANCE)hModule;

#ifdef _DEBUG
	spdlog::set_level( spdlog::level::debug );
#endif

	spdlog::info( "Waiting for game modules...\n" );

	// Wait for ALL required game modules to load
	const wchar_t* required_modules[] = {
		L"client.dll",
		L"tier0.dll",
		L"networksystem.dll",
		L"particles.dll",
		L"GameOverlayRenderer64.dll",
	};

	for ( const auto& mod : required_modules ) {
		int attempts = 0;
		while ( !util::get_module_base_wchar( mod ) ) {
			Sleep( 500 );
			attempts++;
			if ( attempts > 120 ) { // 60 seconds timeout
				spdlog::error( "Timeout waiting for module: {}\n", util::utf8_encode( mod ) );
				break;
			}
		}
	}
	// Also need steam_api64.dll
	while ( !util::get_module_base_ansi( "steam_api64.dll" ) ) {
		Sleep( 500 );
	}

	Sleep( 3000 ); // Extra delay for modules to fully initialize

	spdlog::info( "All modules loaded, starting init...\n" );

	const int result = safe_start_init( );
	if ( result == 1 ) {
		spdlog::warn( "Init completed with some failures (see log above)\n" );
	} else if ( result > 1 ) {
		spdlog::critical( "CRASH during initialization! Exception code: 0x{:X}\n", (unsigned)result );
		spdlog::critical( "This usually means game patterns/offsets are outdated.\n" );
		MessageBoxA( nullptr, "Cheat crashed during init.\nPatterns may be outdated for this Dota 2 version.",
			"GAMBODJAN Error", MB_ICONERROR );
	}
	std::cout.flush( );
	return 0;
}

bool __stdcall DllMain( HINSTANCE hModule, DWORD reason, void* ) {
	if ( reason == DLL_PROCESS_ATTACH ) {
		DisableThreadLibraryCalls( hModule );
		CreateThread( nullptr, 0, cheat_thread, hModule, 0, nullptr );
	}
	return TRUE;
}