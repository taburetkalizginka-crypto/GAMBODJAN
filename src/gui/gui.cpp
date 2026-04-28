#define STB_IMAGE_IMPLEMENTATION

#include <vector>
#include <string>
#include <map>

#include "../core/lib/imgui/stb_image.h"

#include "../core/hook/hook.hpp"

#include "gui.hpp"
#include "menu.h"

ID3D11DeviceContext* CGui::pContext = NULL;
ID3D11RenderTargetView* CGui::pRenderTargetView = NULL;
bool CGui::show = false;
extern CContext context;

#define MAX(a, b)    (((a) < (b)) ? (b) : (a))
#define INTERNAL     static

struct MultilineScrollState
{
	// Input.
	float scrollRegionX;
	float scrollX;
	ImGuiStorage* storage;

	// Output.
	bool newScrollPositionAvailable;
	float newScrollX;
};

INTERNAL int MultilineScrollCallback( ImGuiInputTextCallbackData* data )
{
	MultilineScrollState* scrollState = (MultilineScrollState*)data->UserData;
	ImGuiID cursorId = ImGui::GetID( "cursor" );
	int oldCursorIndex = scrollState->storage->GetInt( cursorId, 0 );

	if ( oldCursorIndex != data->CursorPos )
	{
		int begin = data->CursorPos;

		while ( ( begin > 0 ) && ( data->Buf[begin - 1] != '\n' ) )
		{
			--begin;
		}

		float cursorOffset = ImGui::CalcTextSize( data->Buf + begin, data->Buf + data->CursorPos ).x;
		float SCROLL_INCREMENT = scrollState->scrollRegionX * 0.25f;

		if ( cursorOffset < scrollState->scrollX )
		{
			scrollState->newScrollPositionAvailable = true;
			scrollState->newScrollX = MAX( 0.0f, cursorOffset - SCROLL_INCREMENT );
		}
		else if ( ( cursorOffset - scrollState->scrollRegionX ) >= scrollState->scrollX )
		{
			scrollState->newScrollPositionAvailable = true;
			scrollState->newScrollX = cursorOffset - scrollState->scrollRegionX + SCROLL_INCREMENT;
		}
	}

	scrollState->storage->SetInt( cursorId, data->CursorPos );

	return 0;
}

INTERNAL bool ImGuiInputTextMultiline( const char* label, char* buf, size_t buf_size, float height, ImGuiInputTextFlags flags = 0 )
{
	float scrollbarSize = ImGui::GetStyle( ).ScrollbarSize;
	float labelWidth = ImGui::CalcTextSize( label ).x + scrollbarSize;
	float SCROLL_WIDTH = 2000.0f; // Very large scrolling width to allow for very long lines.
	MultilineScrollState scrollState = {};

	// Set up child region for horizontal scrolling of the text box.
	ImGui::BeginChild( label, ImVec2( -labelWidth, height ), false, ImGuiWindowFlags_HorizontalScrollbar );
	scrollState.scrollRegionX = MAX( 0.0f, ImGui::GetWindowWidth( ) - scrollbarSize );
	scrollState.scrollX = ImGui::GetScrollX( );
	scrollState.storage = ImGui::GetStateStorage( );
	bool changed = ImGui::InputTextMultiline( label, buf, buf_size, ImVec2( SCROLL_WIDTH, MAX( 0.0f, height - scrollbarSize ) ),
											  flags | ImGuiInputTextFlags_CallbackAlways, MultilineScrollCallback, &scrollState );

	if ( scrollState.newScrollPositionAvailable )
	{
		ImGui::SetScrollX( scrollState.newScrollX );
	}

	ImGui::EndChild( );
	ImGui::SameLine( );
	ImGui::Text( label );

	return changed;
}

//https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#Example-for-DirectX11-users
bool CGui::LoadTextureFromFile( const std::string filename, ID3D11ShaderResourceView** out_srv ) noexcept
{
	auto image_width = 0;
	auto image_height = 0;
	unsigned char* image_data = stbi_load( filename.c_str( ), &image_width, &image_height, 0, 4 );
	if ( !image_data ) {
		stbi_image_free( image_data );
		return false;
	}

	// Create texture
	D3D11_TEXTURE2D_DESC desc{};

	desc.Width = image_width;
	desc.Height = image_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	ID3D11Texture2D* pTexture{};
	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = image_data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	this->pDevice->CreateTexture2D( &desc, &subResource, &pTexture );

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	this->pDevice->CreateShaderResourceView( pTexture, &srvDesc, out_srv );
	pTexture->Release( );

	stbi_image_free( image_data );

	return true;
}

bool CGui::init( IDXGISwapChain* pSwapchain ) {
	if ( SUCCEEDED( pSwapchain->GetDevice( __uuidof( ID3D11Device ), reinterpret_cast<LPVOID*>( &this->pDevice ) ) ) ) {
		this->pDevice->GetImmediateContext( &this->pContext );

		DXGI_SWAP_CHAIN_DESC sd;
		pSwapchain->GetDesc( &sd );
		this->hWnd = sd.OutputWindow;

		ID3D11Texture2D* pBackBuffer{ nullptr };
		pSwapchain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<LPVOID*>( &pBackBuffer ) );
		if ( !pBackBuffer ) return false;

		D3D11_RENDER_TARGET_VIEW_DESC desc = {};
		memset( &desc, 0, sizeof( desc ) );

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // most important change!
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		pDevice->CreateRenderTargetView( pBackBuffer, &desc, &this->pRenderTargetView );
		pBackBuffer->Release( );

		hook::original::fpWndProc = reinterpret_cast<decltype( hook::original::fpWndProc )>( SetWindowLongPtrA( this->hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( &hook::functions::WndProc ) ) );
		ImGui::CreateContext( );

		// this->ApplyStyle( );

		ImGuiIO io = ImGui::GetIO( );

		io.WantSaveIniSettings = false;
		io.IniFilename = NULL;
		io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

		this->menu_font = io.Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\tahomabd.ttf", 23.0f, 0, io.Fonts->GetGlyphRangesCyrillic( ) );
		this->hp_mana_font = io.Fonts->AddFontDefault( );

		// d2c paste
		auto defaultFont = ImGui::GetIO( ).Fonts->AddFontDefault( );
		{
			ImFontConfig fontCfg{};
			fontCfg.FontDataOwnedByAtlas = false;
			for ( int i = 2; i < 30; i += 2 ) {
				DrawData.Fonts["MSTrebuchet"][i] = ImGui::GetIO( ).Fonts->AddFontFromFileTTF( R"(C:\Windows\Fonts\trebuc.ttf)", i, nullptr, ImGui::GetIO( ).Fonts->GetGlyphRangesDefault( ) );
				DrawData.Fonts["Monofonto"][i] = ImGui::GetIO( ).Fonts->AddFontFromMemoryTTF( (void*)Fonts::Monofonto, IM_ARRAYSIZE( Fonts::Monofonto ), i, &fontCfg, ImGui::GetIO( ).Fonts->GetGlyphRangesDefault( ) );
			}
		}
		
		ImGui_ImplWin32_Init( this->hWnd );
		ImGui_ImplDX11_Init( this->pDevice, this->pContext );
		return true;
	}
	return false;
}

void CGui::Render( ) {
	if ( !show ) return;

	// Sync CGui::show with menu::state.show
	menu::state.show = show;
	menu::state.game_found = true;
	snprintf( menu::state.status_text, sizeof( menu::state.status_text ), "Connected to Dota 2" );

	// Render the nice GAMBODJAN menu
	menu::Render( );

	// Sync back — if user closed the menu via menu::Render
	show = menu::state.show;
}

void CGui::ApplyStyle( ) {
	ImGuiStyle* style = &ImGui::GetStyle( );
	
	style->WindowBorderSize = 0;
	style->WindowTitleAlign = ImVec2( 0.5, 0.5 );
	style->WindowMinSize = ImVec2( 600, 400 );

	style->FramePadding = ImVec2( 8, 6 );

	style->WindowPadding = ImVec2( 15, 15 );
	style->WindowRounding = 0;;
	style->FramePadding = ImVec2( 5, 5 );
	style->FrameRounding = 4.0f;
	style->ItemSpacing = ImVec2( 12, 8 );
	style->ItemInnerSpacing = ImVec2( 8, 6 );
	style->IndentSpacing = 25.0f;
	style->ScrollbarSize = 15.0f;
	style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 1.0f;

	style->Colors[ImGuiCol_Text] = ImVec4( 0.80f, 0.80f, 0.83f, 1.00f );
	style->Colors[ImGuiCol_TextDisabled] = ImVec4( 0.24f, 0.23f, 0.29f, 1.00f );
	style->Colors[ImGuiCol_WindowBg] = ImVec4( 0.06f, 0.05f, 0.07f, 1.00f );
	style->Colors[ImGuiCol_PopupBg] = ImVec4( 0.07f, 0.07f, 0.09f, 1.00f );
	style->Colors[ImGuiCol_Border] = ImVec4( 0.80f, 0.80f, 0.83f, 0.88f );
	style->Colors[ImGuiCol_BorderShadow] = ImVec4( 0.92f, 0.91f, 0.88f, 0.00f );
	style->Colors[ImGuiCol_FrameBg] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.24f, 0.23f, 0.29f, 1.00f );
	style->Colors[ImGuiCol_FrameBgActive] = ImVec4( 0.56f, 0.56f, 0.58f, 1.00f );
	style->Colors[ImGuiCol_TitleBg] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 1.00f, 0.98f, 0.95f, 0.75f );
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4( 0.07f, 0.07f, 0.09f, 1.00f );
	style->Colors[ImGuiCol_MenuBarBg] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.80f, 0.80f, 0.83f, 0.31f );
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.56f, 0.56f, 0.58f, 1.00f );
	style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.06f, 0.05f, 0.07f, 1.00f );
	style->Colors[ImGuiCol_CheckMark] = ImVec4( 0.80f, 0.80f, 0.83f, 0.31f );
	style->Colors[ImGuiCol_SliderGrab] = ImVec4( 0.80f, 0.80f, 0.83f, 0.31f );
	style->Colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.06f, 0.05f, 0.07f, 1.00f );
	style->Colors[ImGuiCol_Button] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_ButtonHovered] = ImVec4( 0.24f, 0.23f, 0.29f, 1.00f );
	style->Colors[ImGuiCol_ButtonActive] = ImVec4( 0.56f, 0.56f, 0.58f, 1.00f );
	style->Colors[ImGuiCol_Header] = ImVec4( 0.10f, 0.09f, 0.12f, 1.00f );
	style->Colors[ImGuiCol_HeaderHovered] = ImVec4( 0.56f, 0.56f, 0.58f, 1.00f );
	style->Colors[ImGuiCol_HeaderActive] = ImVec4( 0.06f, 0.05f, 0.07f, 1.00f );
	style->Colors[ImGuiCol_ResizeGrip] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.56f, 0.56f, 0.58f, 1.00f );
	style->Colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.06f, 0.05f, 0.07f, 1.00f );
	style->Colors[ImGuiCol_PlotLines] = ImVec4( 0.40f, 0.39f, 0.38f, 0.63f );
	style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4( 0.25f, 1.00f, 0.00f, 1.00f );
	style->Colors[ImGuiCol_PlotHistogram] = ImVec4( 0.40f, 0.39f, 0.38f, 0.63f );
	style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 0.25f, 1.00f, 0.00f, 1.00f );
	style->Colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.25f, 1.00f, 0.00f, 0.43f );
}