#include "overwolf.h"

#ifdef GAMBODJAN_WINDOWS
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

static std::string http_get( const std::string& url_str ) {
	std::string result;
#ifdef GAMBODJAN_WINDOWS
	HINTERNET hSession = InternetOpenA( "GAMBODJAN", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
	if ( !hSession ) return result;
	HINTERNET hUrl = InternetOpenUrlA( hSession, url_str.c_str( ), NULL, 0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0 );
	if ( hUrl ) {
		char buf[ 4096 ]; DWORD bytesRead = 0;
		while ( InternetReadFile( hUrl, buf, sizeof( buf ), &bytesRead ) && bytesRead ) {
			result.append( buf, bytesRead );
			bytesRead = 0;
		}
		InternetCloseHandle( hUrl );
	}
	InternetCloseHandle( hSession );
#endif
	return result;
}

void COverWolf::process_lobby_members( ) {
	auto& gc = CGCClient::get( );
	if ( &gc && gc.GetLobbyManager( )->lobby_data ) {
		auto& dotaLobby = gc.GetLobbyManager( )->lobby_data->m_dota_lobby->get_dynamic_lobby( )->so_dynamic_lobby;
		std::string url = "http://127.0.0.1:5000/?players=";
		uint16_t pl_count = 0;

		for ( auto& member : dotaLobby.all_members( ) ) {
			if ( !member.has_id( ) || !member.has_name( ) ) continue;

			url += std::to_string( member.id( ) ) + ",";
			pl_count++;
		}
		if ( url[ url.size( ) - 1 ] == ',' ) url.pop_back( );
		url += "&count=" + std::to_string( pl_count );

		http_get( url );
	}
}

void COverWolf::process_member( const CSODOTALobbyMember& member, std::string* bf ) {

}
