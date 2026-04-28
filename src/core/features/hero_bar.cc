#include "hero_bar.h"

bool CHeroBar::can_draw_for( C_DOTA_BaseNPC_Hero* h ) {
	if ( !h ||
		 h->IsAllyWith( context.local_entity ) ||
		 h->GetIdentity( )->IsDormant( ) ||
		 h->GetLifeState( ) != 0 ||
		 h->IsIllusion( )
		 )
		return false;

	return true;
}

void CHeroBar::draw_health( const bool off ) {
	static constexpr auto font_size = 17;
	for ( auto& hero : context.entities.heroes ) {
		if ( !can_draw_for( hero ) )
			continue;

		vector2d scr;
		vector3d pos = hero->GetAbsoluteOrigin( );
		pos.z += hero->GetHBOffset( );

		if ( !CRenderGameSystem::GetInstance( )->GetVectorInScreenSpace( pos, scr ) )
			continue;

		off ? scr.y -= 43 : scr.y -= 39;

		drawing::DrawTextForeground( DrawData.GetFont( "Monofonto", font_size ), std::to_string( hero->GetHealth( ) ), ImVec2{ scr.x, scr.y }, font_size, ImColor{ 255,255,255 }, true );
	}
}

void CHeroBar::draw_mana( ) {
	constexpr static ImVec2 manabarSize{ 138, 12 };
	for ( auto& hero : context.entities.heroes ) {
		if ( !can_draw_for( hero ) )
			continue;

		vector2d scr;
		vector3d pos = hero->GetAbsoluteOrigin( );
		pos.z += hero->GetHBOffset( );

		if ( !CRenderGameSystem::GetInstance( )->GetVectorInScreenSpace( pos, scr ) )
			continue;

		const ImVec2 drawPos{ scr.x - 15, scr.y - 16 };

		// Background
		drawing::DrawRectFilled(
			drawPos - ImVec2( 110, manabarSize.y ) / 2,
			manabarSize, ImVec4( 0, 0, 0, 1 ) );
		// Manabar
		drawing::DrawRectFilled(
			drawPos - ImVec2( 110, manabarSize.y ) / 2 + ImVec2( 1, 1 ),
			ImVec2( manabarSize.x * ( hero->GetMana( ) / hero->GetMaxMana( ) ) - 2, manabarSize.y - 2 ), ImVec4( 0, 0.5, 1, 1 ) );
	}
}

void CHeroBar::draw_abilities_and_items( ) {
	static constexpr int MAX_ABILITIES = 7;
	static constexpr int MAX_ITEMS = 6;
	static constexpr float ICON_SIZE = 22.0f;
	static constexpr float ICON_GAP = 2.0f;
	static constexpr float ITEM_SIZE = 20.0f;
	static constexpr int CD_FONT_SIZE = 12;

	CGlobalVars* gpGlobals = CGlobalVars::get( );
	const float curtime = gpGlobals ? gpGlobals->m_curtime : 0.f;

	for ( auto& hero : context.entities.heroes ) {
		if ( !can_draw_for( hero ) )
			continue;

		vector2d scr;
		vector3d pos = hero->GetAbsoluteOrigin( );
		pos.z += hero->GetHBOffset( );

		if ( !CRenderGameSystem::GetInstance( )->GetVectorInScreenSpace( pos, scr ) )
			continue;

		// Count visible abilities to center them
		int abilityCount = 0;
		struct AbilityInfo {
			int level;
			float cooldownRemaining;
			bool onCooldown;
			bool hidden;
		};
		AbilityInfo abilities[MAX_ABILITIES] = {};

		for ( int i = 0; i < MAX_ABILITIES; i++ ) {
			EntityIndex_t abilIdx = hero->GetAbility( i );
			if ( !abilIdx.is_valid( ) ) continue;

			C_BaseEntity* abilEnt = context.entities[ abilIdx ];
			if ( !abilEnt || !util::exists( abilEnt ) ) continue;

			bool isHidden = abilEnt->Member<bool>( schema::C_DOTABaseAbility::m_bHidden );
			int level = abilEnt->Member<int>( schema::C_DOTABaseAbility::m_iLevel );

			if ( isHidden ) continue;

			float cdEndTime = abilEnt->Member<float>( schema::C_DOTABaseAbility::m_fCooldown );
			float cdRemaining = 0.f;
			bool onCd = false;

			if ( cdEndTime > curtime ) {
				cdRemaining = cdEndTime - curtime;
				onCd = true;
			}

			abilities[abilityCount].level = level;
			abilities[abilityCount].cooldownRemaining = cdRemaining;
			abilities[abilityCount].onCooldown = onCd;
			abilities[abilityCount].hidden = false;
			abilityCount++;
		}

		if ( abilityCount == 0 ) continue;

		// Draw abilities above health bar
		float totalAbilWidth = abilityCount * ICON_SIZE + ( abilityCount - 1 ) * ICON_GAP;
		float startX = scr.x - totalAbilWidth / 2.0f;
		float startY = scr.y - 55.0f;

		for ( int i = 0; i < abilityCount; i++ ) {
			float x = startX + i * ( ICON_SIZE + ICON_GAP );
			ImVec2 iconPos{ x, startY };
			ImVec2 iconSize{ ICON_SIZE, ICON_SIZE };

			// Background
			ImVec4 bgColor;
			if ( abilities[i].level == 0 ) {
				bgColor = ImVec4( 0.15f, 0.15f, 0.15f, 0.85f );
			} else if ( abilities[i].onCooldown ) {
				bgColor = ImVec4( 0.35f, 0.08f, 0.08f, 0.90f );
			} else {
				bgColor = ImVec4( 0.10f, 0.35f, 0.10f, 0.90f );
			}

			drawing::DrawRectFilled( iconPos, iconSize, bgColor );

			// Border
			ImVec4 borderColor = abilities[i].level > 0
				? ImVec4( 0.6f, 0.6f, 0.6f, 0.8f )
				: ImVec4( 0.3f, 0.3f, 0.3f, 0.5f );
			drawing::DrawRect( iconPos, iconSize, borderColor, 1.0f );

			// Level dots at the bottom
			if ( abilities[i].level > 0 ) {
				float dotY = iconPos.y + ICON_SIZE - 3.0f;
				int maxDots = abilities[i].level > 4 ? 4 : abilities[i].level;
				float dotSpacing = ICON_SIZE / ( maxDots + 1 );
				auto* drawList = ImGui::GetForegroundDrawList( );
				for ( int d = 0; d < maxDots; d++ ) {
					float dotX = iconPos.x + dotSpacing * ( d + 1 );
					drawList->AddCircleFilled(
						ImVec2( dotX, dotY ), 1.5f,
						IM_COL32( 255, 255, 255, 200 ) );
				}
			}

			// Cooldown text
			if ( abilities[i].onCooldown ) {
				char cdText[16];
				if ( abilities[i].cooldownRemaining >= 10.0f )
					snprintf( cdText, sizeof( cdText ), "%.0f", abilities[i].cooldownRemaining );
				else
					snprintf( cdText, sizeof( cdText ), "%.1f", abilities[i].cooldownRemaining );

				ImVec2 textPos{ x + ICON_SIZE / 2.0f, startY + ICON_SIZE / 2.0f - CD_FONT_SIZE / 2.0f };
				drawing::DrawTextForeground(
					DrawData.GetFont( "Monofonto", CD_FONT_SIZE ),
					cdText, textPos, CD_FONT_SIZE,
					ImColor{ 255, 255, 255 }, true );
			}
		}

		// Draw items below abilities
		int itemCount = 0;
		struct ItemInfo {
			float cooldownRemaining;
			bool onCooldown;
		};
		ItemInfo items[MAX_ITEMS] = {};

		for ( int i = 0; i < MAX_ITEMS; i++ ) {
			EntityIndex_t itemIdx = hero->GetItemSlot( i );
			if ( !itemIdx.is_valid( ) ) {
				items[itemCount].onCooldown = false;
				items[itemCount].cooldownRemaining = 0.f;
				itemCount++;
				continue;
			}

			C_BaseEntity* itemEnt = context.entities[ itemIdx ];
			if ( !itemEnt || !util::exists( itemEnt ) ) {
				items[itemCount].onCooldown = false;
				items[itemCount].cooldownRemaining = 0.f;
				itemCount++;
				continue;
			}

			float cdEndTime = itemEnt->Member<float>( schema::C_DOTABaseAbility::m_fCooldown );
			float cdRemaining = 0.f;
			bool onCd = false;

			if ( cdEndTime > curtime ) {
				cdRemaining = cdEndTime - curtime;
				onCd = true;
			}

			items[itemCount].cooldownRemaining = cdRemaining;
			items[itemCount].onCooldown = onCd;
			itemCount++;
		}

		float totalItemWidth = MAX_ITEMS * ITEM_SIZE + ( MAX_ITEMS - 1 ) * ICON_GAP;
		float itemStartX = scr.x - totalItemWidth / 2.0f;
		float itemStartY = startY + ICON_SIZE + 3.0f;

		for ( int i = 0; i < MAX_ITEMS; i++ ) {
			float x = itemStartX + i * ( ITEM_SIZE + ICON_GAP );
			ImVec2 iconPos{ x, itemStartY };
			ImVec2 iconSize{ ITEM_SIZE, ITEM_SIZE };

			EntityIndex_t itemIdx = hero->GetItemSlot( i );
			bool hasItem = itemIdx.is_valid( ) && context.entities[ itemIdx ] != nullptr;

			// Background
			ImVec4 bgColor;
			if ( !hasItem ) {
				bgColor = ImVec4( 0.12f, 0.12f, 0.12f, 0.60f );
			} else if ( items[i].onCooldown ) {
				bgColor = ImVec4( 0.35f, 0.15f, 0.05f, 0.90f );
			} else {
				bgColor = ImVec4( 0.15f, 0.25f, 0.35f, 0.90f );
			}

			drawing::DrawRectFilled( iconPos, iconSize, bgColor );

			// Border
			ImVec4 borderColor = hasItem
				? ImVec4( 0.5f, 0.5f, 0.5f, 0.7f )
				: ImVec4( 0.2f, 0.2f, 0.2f, 0.4f );
			drawing::DrawRect( iconPos, iconSize, borderColor, 1.0f );

			// Cooldown text
			if ( items[i].onCooldown && hasItem ) {
				char cdText[16];
				if ( items[i].cooldownRemaining >= 10.0f )
					snprintf( cdText, sizeof( cdText ), "%.0f", items[i].cooldownRemaining );
				else
					snprintf( cdText, sizeof( cdText ), "%.1f", items[i].cooldownRemaining );

				ImVec2 textPos{ x + ITEM_SIZE / 2.0f, itemStartY + ITEM_SIZE / 2.0f - CD_FONT_SIZE / 2.0f };
				drawing::DrawTextForeground(
					DrawData.GetFont( "Monofonto", CD_FONT_SIZE ),
					cdText, textPos, CD_FONT_SIZE,
					ImColor{ 255, 220, 150 }, true );
			}
		}
	}
}
