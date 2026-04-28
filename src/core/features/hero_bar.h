#pragma once

#include "../sdk_game/CDOTA_BaseNPC.hpp"
#include "../context.hpp"

class CHeroBar {
	bool can_draw_for( C_DOTA_BaseNPC_Hero* );
public:
	void draw_health( const bool );
	void draw_mana( );
	void draw_abilities_and_items( );
};


namespace features {
	inline CHeroBar hero_bar{};
}