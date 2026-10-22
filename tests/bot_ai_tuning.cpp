#include <math.h>
#include <limits>
#include <cstdlib>

#include "ai_combat_tuning.h"
#include "ai_strafejump.h"
#include "ai_traversal.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( bot_ai )

BOOST_AUTO_TEST_SUITE( tuning )

BOOST_AUTO_TEST_CASE( merc_ranged_modes_respect_ammo_and_forced_fire )
{
	BOOST_CHECK( BotCombat_ModeUsable( 1, 5, 5, 0, 0 ) );
	BOOST_CHECK( !BotCombat_ModeUsable( 1, 4, 5, 0, 0 ) );
	BOOST_CHECK( !BotCombat_ModeUsable( 0, 100, 5, 0, 0 ) );
	BOOST_CHECK( !BotCombat_ModeUsable( 1, 5, 5, 1, 1 ) );
	BOOST_CHECK( !BotCombat_ModeUsable( 1, 5, 5, 2, 0 ) );
	BOOST_CHECK( BotCombat_ModeUsable( 1, 5, 5, 2, 1 ) );
}

BOOST_AUTO_TEST_CASE( merc_weapon_preferences_are_soft_and_distance_aware )
{
	for (int skill = 1; skill <= 2; ++skill)
		BOOST_CHECK_EQUAL( BotCombat_WeaponScore( skill, 200, 1, 7, 0, 0 ), 7 );
	for (int skill = 3; skill <= 6; ++skill)
	{
		BOOST_CHECK( BotCombat_WeaponScore( skill, 75, 1, 1, 0, 0 ) >
			BotCombat_WeaponScore( skill, 200, 1, 10, 0, 0 ) );
		BOOST_CHECK( BotCombat_WeaponScore( skill, 200, 1, 10, 0, 0 ) > -10000 );
	}
	for (int skill = 7; skill <= 10; ++skill)
	{
		BOOST_CHECK( BotCombat_WeaponScore( skill, 200, 1, 1, 0, 0 ) >
			BotCombat_WeaponScore( skill, 75, 1, 10, 0, 0 ) );
		BOOST_CHECK( BotCombat_WeaponScore( skill, 200, 0.1f, 1, 0, 0 ) <
			BotCombat_WeaponScore( skill, 75, 1, 10, 0, 0 ) );
	}
}

BOOST_AUTO_TEST_CASE( merc_close_range_safety_beats_dps_without_banning_fallback )
{
	for (int skill = 1; skill <= 10; ++skill)
		BOOST_CHECK( BotCombat_WeaponScore( skill, 25, 1, 1, 1, 0 ) >
			BotCombat_WeaponScore( skill, 200, 1, 10, 1, 1 ) );
	BOOST_CHECK( BotCombat_WeaponScore( 10, 200, 1, 10, 1, 1 ) > -1.0e20f );
}

BOOST_AUTO_TEST_CASE( merc_splash_probe_keeps_firing_pitch_and_muzzle_offset )
{
	const float muzzle[] = { 12, 8, 64 };
	const float downward[] = { 0.6f, 0, -0.8f };
	const float upward[] = { 0.6f, 0, 0.8f };
	float end[3];
	BotCombat_SplashTraceEnd( muzzle, downward, 208, end );
	BOOST_CHECK_CLOSE( end[0], 136.8f, 0.001f );
	BOOST_CHECK_EQUAL( end[1], 8 );
	BOOST_CHECK_CLOSE( end[2], -102.4f, 0.001f );
	BotCombat_SplashTraceEnd( muzzle, upward, 208, end );
	BOOST_CHECK_CLOSE( end[2], 230.4f, 0.001f );
}

BOOST_AUTO_TEST_CASE( merc_resupply_prioritizes_ammo_that_restores_owned_ranged_weapon )
{
	BOOST_CHECK_EQUAL( BotCombat_PickupPriority( 0, 1, 0, 0 ), 0 );
	BOOST_CHECK_EQUAL( BotCombat_PickupPriority( 1, 0, 1, 0 ), 0 );
	BOOST_CHECK( BotCombat_PickupPriority( 1, 0, 1, 1 ) >
		BotCombat_PickupPriority( 1, 1, 0, 0 ) );
	BOOST_CHECK( BotCombat_PickupPriority( 1, 1, 0, 0 ) > 0 );
}

BOOST_AUTO_TEST_CASE( saber_idle_approach_releases_cached_zero_footing_not_defense )
{
	BOOST_CHECK( BotCombat_RefreshIdleFooting( 1, 0, 0, 200, 1, 0 ) );
	BOOST_CHECK( BotCombat_RefreshIdleFooting( 1, 0, 0, 150, 1, -1 ) );
	BOOST_CHECK( !BotCombat_RefreshIdleFooting( 0, 0, 0, 200, 1, 0 ) );
	BOOST_CHECK( !BotCombat_RefreshIdleFooting( 1, 1, 0, 200, 1, 0 ) );
	BOOST_CHECK( !BotCombat_RefreshIdleFooting( 1, 0, 1, 200, 1, 0 ) );
	BOOST_CHECK( !BotCombat_RefreshIdleFooting( 1, 0, 0, 80, 1, 0 ) );
	BOOST_CHECK( !BotCombat_RefreshIdleFooting( 1, 0, 0, 200, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_blocked_approach_tries_forward_lanes_before_safe_exits )
{
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 1, 0, 0 ), 1 );
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 1, 0, 1 ), 1 );
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 1, 0, 2 ), 0 );
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 1, 0, 4 ), -1 );
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 1, 1, 0 ), -1 );
	BOOST_CHECK_EQUAL( BotCombat_FootingFallbackForward( 0, 0, 0 ), -1 );
}

BOOST_AUTO_TEST_CASE( saber_and_force_duels_approach_from_safe_distance_without_empty_swings )
{
	for (int saberOnly = 0; saberOnly <= 1; ++saberOnly)
	{
		for (int distance = 150; distance <= 250; distance += 50)
		{
			newbotai_saber_tactic_context_t context = {};
			context.saberOnlyDuel = saberOnly;
			context.saberCombat = 1;
			context.skill = 7;
			context.ourTotalHealth = context.enemyTotalHealth = 100;
			context.enemyDistance = (float)distance;
			const auto tactic = NewBotAI_GetCorrectSaberTactic( context );
			const auto command = NewBotAI_PlanSaberCommand( context, tactic,
				NEWBOTAI_SABER_BASIC, 0, 1, 1, 1, 1, 0 );
			BOOST_CHECK_EQUAL( command.forward, 1 );
			BOOST_CHECK_EQUAL( command.attack, 0 );
		}
	}
}

BOOST_AUTO_TEST_CASE( strafejump_frequency_and_waypoint_skip_safety )
{
	BOOST_CHECK_EQUAL( BotSFJ_StartIntervalMs( 0 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_StartIntervalMs( -1 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_StartIntervalMs( 100 ), 1000 );
	BOOST_CHECK_EQUAL( BotSFJ_StartIntervalMs( 200 ), 500 );
	BOOST_CHECK_EQUAL( BotSFJ_StartIntervalMs( 2000 ), 100 );
	BOOST_CHECK( BotSFJ_WaypointSkipAllows( 0, 0.0f, 1.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_WaypointSkipAllows( 1, 0.0f, 1.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_WaypointSkipAllows( 0, 33.0f, 1.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_WaypointSkipAllows( 0, 0.0f, 0.8f, 1 ) );
	BOOST_CHECK( !BotSFJ_WaypointSkipAllows( 0, 0.0f, 1.0f, 0 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_settings_and_drop_limits_are_finite_and_bounded )
{
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( -1, 0, 200 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( 80, 0, 200 ), 80 );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( 201, 0, 200 ), 200 );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( std::numeric_limits<float>::quiet_NaN(), 32, 200 ), 32 );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( std::numeric_limits<float>::infinity(), 32, 200 ), 32 );
	BOOST_CHECK( BotSFJ_HeightAllows( -200, 32, 200 ) );
	BOOST_CHECK( BotSFJ_HeightAllows( 32, 32, 200 ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( -201, 32, 200 ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( 33, 32, 200 ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( std::numeric_limits<float>::quiet_NaN(), 32, 200 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_track_nodes_measure_progress_not_exact_touchdown )
{
	const float from[] = { 0, 0, 0 };
	const float node[] = { 100, 0, 0 };
	const float beyond[] = { 200, 80, 200 };
	const float offRoute[] = { 200, 81, 200 };
	const float wrongHeight[] = { 100, 0, 65 };
	const float near[] = { 20, 0, 0 };
	BOOST_CHECK( BotSFJ_TrackNodePassed( beyond, from, node, 0, 80, 64 ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( offRoute, from, node, 0, 80, 64 ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( wrongHeight, from, node, 1, 80, 64 ) );
	BOOST_CHECK( BotSFJ_TrackNodePassed( wrongHeight, from, node, 0, 80, 64 ) );
	BOOST_CHECK( BotSFJ_TrackNodePassed( near, from, node, 1, 80, 64 ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( beyond, node, node, 0, 80, 64 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_continuity_uses_speed_hysteresis )
{
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420, 250, 250 ) );
	BOOST_CHECK( BotSFJ_ContinuitySlowedOut( 420, 180, 250 ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 260, 180, 250 ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420, 200, 250 ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420, 180, 0 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_drop_links_keep_all_existing_route_gates )
{
	BOOST_CHECK( BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -200, -200, 0.9f, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 33, 0, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -201, 0, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 0, -201, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 1, 1, 1, 0, 0, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 0, 1, 0, 0, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 0, 0, 0, 1, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 0, 0, 0.89f, 0, 200 ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 0, 0, 1, -1, 200 ) );
}

BOOST_AUTO_TEST_CASE( navigation_soft_overrides_require_live_validated_strafe_ownership )
{
	BOOST_CHECK( BotNav_PreserveValidatedStrafe( 1, 1, 1, 1, 1, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 0, 1, 1, 1, 1, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 0, 1, 1, 1, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 0, 1, 1, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 1, 0, 1, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 1, 1, 0, 1, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 1, 1, 1, 0, 0, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 1, 1, 1, 1, 1, 0 ) );
	BOOST_CHECK( !BotNav_PreserveValidatedStrafe( 1, 1, 1, 1, 1, 1, 0, 1 ) );
}

BOOST_AUTO_TEST_CASE( navigation_floor_safety_only_waives_ordinary_gaps_for_validated_jumps )
{
	BOOST_CHECK( BotNav_FloorAllows( 1, 0, 1, 200, 200, 0, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 1, 0, 1, 201, 200, 0, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 0, 0, 1, 0, 200, 0, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 1, 1, 1, 0, 200, 0, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 1, 0, 0.69f, 0, 200, 0, 0 ) );
	BOOST_CHECK( BotNav_FloorAllows( 0, 0, 0, 500, 200, 1, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 0, 0, 0, 500, 200, 1, 1 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 1, 0, 1, 0, 200, 1, 1 ) );
	BOOST_CHECK( BotNav_FloorAllows( 0, 1, 0, 500, 200, 1, 0 ) );
	BOOST_CHECK( !BotNav_FloorAllows( 0, 1, 0, 500, 200, 1, 1 ) );
}

BOOST_AUTO_TEST_CASE( traversal_fall_prediction_matches_event_thresholds_and_damage_cap )
{
	botnav_fall_context_t context = {};
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 650, &context ), 0 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 700, &context ), 7 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 16 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 10000, &context ), 96 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( sqrtf( 449000.0f ), &context ), 0 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( sqrtf( 451000.0f ), &context ), 7 );
	context.maxDamage = 10;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 10 );
	context.siege = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 100 );
	context.knockdown = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 10 );
	context.maxDamage = 0;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 400, &context ), 16 );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 370, &context ), 0 );
}

BOOST_AUTO_TEST_CASE( traversal_fall_prediction_accounts_for_crouch_water_and_force_jump )
{
	botnav_fall_context_t context = {};
	context.ducked = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 700, &context ), 15 );
	context.waterLevel = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 700, &context ), 7 );
	context.waterLevel = 2;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 700, &context ), 0 );
	context.waterLevel = 3;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 10000, &context ), 0 );
	context = {};
	context.forceJumpStart = 256;
	context.forceJumpHeight = 384;
	context.landingHeight = 256;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 0 );
	context.landingHeight = 128;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 0 );
	context.landingHeight = -128;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 1000, &context ), 16 );
	context.noFallingDamage = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 10000, &context ), 0 );
	context.noFallingDamage = 0;
	context.noDamageSurface = 1;
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( 10000, &context ), 0 );
}

BOOST_AUTO_TEST_CASE( traversal_accepts_only_confirmed_nonlethal_landings_at_current_health )
{
	botnav_fall_context_t context = {};
	const float impact = BotNav_ImpactSpeed( 0, 800, 400 );
	const int damage = BotNav_PredictFallDamage( impact, &context );
	BOOST_CHECK_CLOSE( impact, 800.0f, 0.001f );
	BOOST_CHECK_EQUAL( damage, 10 );
	BOOST_CHECK( BotNav_LandingAllows( 1, 0, 0.7f, 0, damage, 11 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, 1, 0, damage, 10 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, 1, 0, damage, 1 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 0, 0, 1, 0, 0, 100 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 1, 1, 0, 0, 100 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, 0.69f, 0, 0, 100 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, 1, 1, 0, 100 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, 1, 0, 0, 0 ) );
	BOOST_CHECK( !BotNav_LandingAllows( 1, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0, 100 ) );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( -1, &context ), INT_MAX );
	BOOST_CHECK_EQUAL( BotNav_PredictFallDamage( std::numeric_limits<float>::infinity(), &context ), INT_MAX );
	BOOST_CHECK_EQUAL( BotNav_ImpactSpeed( 0, 0, 400 ), -1 );
	BOOST_CHECK_EQUAL( BotNav_ImpactSpeed( 0, 800, -1 ), -1 );
}

BOOST_AUTO_TEST_CASE( traversal_stairs_use_ground_steps_not_legacy_jump_restarts )
{
	BOOST_CHECK( BotNav_StepAllows( 1, 1, 1, 1, 0, 0.7f, 18, 18, 0 ) );
	BOOST_CHECK( BotNav_StepAllows( 1, 1, 1, 1, 0, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 0, 1, 1, 1, 0, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 0, 1, 1, 0, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 0, 1, 0, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 0, 0, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 1, 1, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 0, 0.69f, 8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 0, 1, 19, 18, 0 ) );
	BOOST_CHECK( BotNav_StepAllows( 1, 1, 1, 1, 0, 1, -8, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 0, 1, -19, 18, 0 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 0, 1, 8, 18, 1 ) );
	BOOST_CHECK( !BotNav_StepAllows( 1, 1, 1, 1, 0, 1,
		std::numeric_limits<float>::quiet_NaN(), 18, 0 ) );
}

BOOST_AUTO_TEST_CASE( traversal_step_corridors_are_bounded_and_sample_every_eight_units )
{
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 4, 1024 ), 1 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 48, 1024 ), 6 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 49, 1024 ), 7 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 1024, 1024 ), 128 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 1025, 2048 ), 0 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 49, 48 ), 0 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( 0, 1024 ), 0 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( -1, 1024 ), 0 );
	BOOST_CHECK_EQUAL( BotNav_StepSegmentSamples( std::numeric_limits<float>::infinity(), 1024 ), 0 );
	BOOST_CHECK( BotNav_StepEndpointAllows( 64, 64, 18 ) );
	BOOST_CHECK( BotNav_StepEndpointAllows( 64, 82, 18 ) );
	BOOST_CHECK( !BotNav_StepEndpointAllows( 64, 83, 18 ) );
	BOOST_CHECK( !BotNav_StepEndpointAllows( 64, 45, 18 ) );
	BOOST_CHECK( !BotNav_StepEndpointAllows( 64, std::numeric_limits<float>::quiet_NaN(), 18 ) );
}

BOOST_AUTO_TEST_CASE( navigation_gap_dispatch_never_falls_back_to_unsafe_grounded_walking )
{
	BOOST_CHECK( BotNav_GapDispatchAllows( 1, 1, 1, 1, 1, 1 ) );
	BOOST_CHECK( BotNav_GapDispatchAllows( 0, 0, 0, 0, 0, 0 ) );
	BOOST_CHECK( !BotNav_GapDispatchAllows( 1, 0, 1, 1, 1, 1 ) );
	BOOST_CHECK( !BotNav_GapDispatchAllows( 1, 1, 0, 1, 1, 1 ) );
	BOOST_CHECK( !BotNav_GapDispatchAllows( 1, 1, 1, 0, 1, 1 ) );
	BOOST_CHECK( !BotNav_GapDispatchAllows( 1, 1, 1, 1, 0, 1 ) );
	BOOST_CHECK( !BotNav_GapDispatchAllows( 1, 1, 1, 1, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_steering_caps_shortest_wrapped_yaw_change )
{
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 350, 10, 12 ), 362 );
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 10, 350, 12 ), -2 );
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 350, 355, 12 ), 355 );
}

BOOST_AUTO_TEST_CASE( strafejump_dense_waypoint_budget_is_independent_and_bounded )
{
	BOOST_CHECK_EQUAL( BotSFJ_WaypointBudget( -1 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_WaypointBudget( 0 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_WaypointBudget( 128 ), 128 );
	BOOST_CHECK_EQUAL( BotSFJ_WaypointBudget( 1024 ), 512 );
	// Eight-unit trails need more than the old 16-point limit even to start.
	BOOST_CHECK( 16 * 8 < 160 );
	BOOST_CHECK_EQUAL( BotSFJ_WaypointBudget( 128 ) * 8, 1024 );
}

BOOST_AUTO_TEST_CASE( strafejump_soft_conflicts_only_block_initiation )
{
	const bot_sfj_phase_t startPhases[] = { BOT_SFJ_PHASE_OFF, BOT_SFJ_PHASE_PREPARE,
		BOT_SFJ_PHASE_ABORT };
	const bot_sfj_phase_t activePhases[] = { BOT_SFJ_PHASE_TAKEOFF, BOT_SFJ_PHASE_AIR,
		BOT_SFJ_PHASE_LANDING, BOT_SFJ_PHASE_REJUMP };

	for (bot_sfj_phase_t phase : startPhases)
	{
		BOOST_CHECK( !BotSFJ_PhaseInProgress( phase ) );
		BOOST_CHECK( !BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_NONE, phase ) );
		BOOST_CHECK( BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_SOFT, phase ) );
		BOOST_CHECK( BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_HARD, phase ) );
	}
	for (bot_sfj_phase_t phase : activePhases)
	{
		BOOST_CHECK( BotSFJ_PhaseInProgress( phase ) );
		BOOST_CHECK( !BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_NONE, phase ) );
		BOOST_CHECK( !BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_SOFT, phase ) );
		BOOST_CHECK( BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_HARD, phase ) );
		// Ignoring a soft conflict keeps ownership through the air phase.
		BOOST_CHECK( BotSFJ_CanOwnInput( 1, 1,
			!BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_SOFT, phase ), phase ) );
		BOOST_CHECK( !BotSFJ_CanOwnInput( 1, 1,
			!BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_HARD, phase ), phase ) );
	}
}

BOOST_AUTO_TEST_CASE( strafejump_passed_waypoints_ignore_valid_flight_height_not_lateral_error )
{
	const float waypoint[3] = { 80.0f, 0.0f, 0.0f };
	const float direction[3] = { 1.0f, 0.0f, 0.0f };
	const float reverse[3] = { -1.0f, 0.0f, 0.0f };
	const float airborne[3] = { 96.0f, 20.0f, 140.0f };
	const float ahead[3] = { 72.0f, 0.0f, 0.0f };
	const float outside[3] = { 96.0f, 65.0f, 140.0f };

	BOOST_CHECK( BotSFJ_WaypointPassed( airborne, waypoint, direction, 64.0f ) );
	BOOST_CHECK( !BotSFJ_WaypointPassed( ahead, waypoint, direction, 64.0f ) );
	BOOST_CHECK( !BotSFJ_WaypointPassed( outside, waypoint, direction, 64.0f ) );
	BOOST_CHECK( BotSFJ_WaypointPassed( ahead, waypoint, reverse, 64.0f ) );
	BOOST_CHECK( !BotSFJ_WaypointPassed( airborne, waypoint, reverse, 64.0f ) );
}

BOOST_AUTO_TEST_CASE( strafejump_dense_advancement_preserves_every_link_constraint )
{
	BOOST_CHECK( BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 0.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 1, 1, 1, 0.0f, 0.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 0, 1, 0.0f, 0.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 0, 0.0f, 0.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 33.0f, 0.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 65.0f, 1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 0.0f, -1.0f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 0.0f, 0.8f, 8.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 0.0f, 1.0f, -1.0f ) );

	const float origin[3] = { 600.0f, 0.0f, 100.0f };
	const float direction[3] = { 1.0f, 0.0f, 0.0f };
	int advanced = 0;
	for (int i = 1; i < BotSFJ_WaypointBudget( 128 ); i++)
	{
		const float point[3] = { i * 8.0f, 0.0f, 0.0f };
		if (!BotSFJ_WaypointPassed( origin, point, direction, 64.0f ))
			break;
		advanced++;
	}
	BOOST_CHECK_EQUAL( advanced, 75 );
}

BOOST_AUTO_TEST_CASE( strafejump_passed_source_keeps_forward_target_inside_clipped_corridor )
{
	const float origin[3] = { 24.0f, 0.0f, 100.0f };
	const float passed[3] = { 8.0f, 0.0f, 0.0f };
	const float direction[3] = { 1.0f, 0.0f, 0.0f };
	const int isPassed = BotSFJ_WaypointPassed( origin, passed, direction, 64.0f );

	BOOST_CHECK( BotSFJ_UseClippedForwardTarget( isPassed, 1024.0f - 1200.0f, 1176.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceLinkAllows( 0, 1, 1, 0.0f, 0.0f, 1.0f, -176.0f ) );
	BOOST_CHECK( BotSFJ_LandingWithinCorridor( 0.0f, 0.0f, 1024.0f, 0.0f,
		origin[0], origin[1], 64.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_UseClippedForwardTarget( 0, -176.0f, 1176.0f ) );
	BOOST_CHECK( !BotSFJ_UseClippedForwardTarget( isPassed, 0.0f, 640.0f ) );
	BOOST_CHECK( !BotSFJ_UseClippedForwardTarget( isPassed, 176.0f, 640.0f ) );
	BOOST_CHECK( BotSFJ_UseClippedForwardTarget( isPassed, 176.0f, 641.0f ) );
}

BOOST_AUTO_TEST_CASE( strafejump_advanced_waypoint_preserves_hostile_field_visibility )
{
	BOOST_CHECK( !BotSFJ_WaypointVisible( 0 ) );
	BOOST_CHECK( BotSFJ_WaypointVisible( 1 ) );
	BOOST_CHECK( !BotSFJ_WaypointVisible( 2 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_state_machine_has_release_edges )
{
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_PREPARE, 1, 1, 1, 1, 0 ),
		BOT_SFJ_PHASE_PREPARE );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_PREPARE, 1, 1, 1, 1, 16 ),
		BOT_SFJ_PHASE_TAKEOFF );
	BOOST_CHECK( BotSFJ_JumpPressed( BOT_SFJ_PHASE_TAKEOFF, 1 ) );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_TAKEOFF, 1, 1, 1, 0, 16 ),
		BOT_SFJ_PHASE_AIR );
	BOOST_CHECK( !BotSFJ_JumpPressed( BOT_SFJ_PHASE_AIR, 0 ) );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_AIR, 1, 1, 1, 1, 300 ),
		BOT_SFJ_PHASE_LANDING );
	BOOST_CHECK( !BotSFJ_JumpPressed( BOT_SFJ_PHASE_LANDING, 1 ) );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_LANDING, 1, 1, 1, 1, 16 ),
		BOT_SFJ_PHASE_REJUMP );
}

BOOST_AUTO_TEST_CASE( strafejump_disable_and_conflict_drop_ownership )
{
	BOOST_CHECK( BotSFJ_CanOwnInput( 1, 1, 1, BOT_SFJ_PHASE_AIR ) );
	BOOST_CHECK( !BotSFJ_CanOwnInput( 0, 1, 1, BOT_SFJ_PHASE_AIR ) );
	BOOST_CHECK( !BotSFJ_CanOwnInput( 1, 1, 0, BOT_SFJ_PHASE_AIR ) );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_AIR, 0, 1, 1, 0, 30 ),
		BOT_SFJ_PHASE_OFF );
	BOOST_CHECK_EQUAL( BotSFJ_NextPhase( BOT_SFJ_PHASE_AIR, 1, 0, 1, 0, 30 ),
		BOT_SFJ_PHASE_ABORT );
}

BOOST_AUTO_TEST_CASE( strafejump_intent_freshness_is_bounded )
{
	BOOST_CHECK( BotSFJ_IntentIsFresh( 1200, 1000 ) );
	BOOST_CHECK( !BotSFJ_IntentIsFresh( 1251, 1000 ) );
	BOOST_CHECK( !BotSFJ_IntentIsFresh( 999, 1000 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_pursuit_uses_start_stop_hysteresis )
{
	BOOST_CHECK( BotSFJ_UpdatePursuitLatch( 0, 540.0f, 500.0f, 200 ) );
	BOOST_CHECK( BotSFJ_UpdatePursuitLatch( 1, 548.0f, 540.0f, 200 ) );
	BOOST_CHECK( !BotSFJ_UpdatePursuitLatch( 1, 550.0f, 548.0f, 200 ) );
	BOOST_CHECK( !BotSFJ_UpdatePursuitLatch( 1, 380.0f, 360.0f, 200 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_pursuit_start_distance_is_configurable )
{
	// bot_minstrafe 0: chase as soon as the enemy pulls away, no distance floor.
	BOOST_CHECK( BotSFJ_UpdatePursuitLatchEx( 0, 60.0f, 40.0f, 200, 0.0f, BotSFJ_StopDistanceFor( 0.0f ) ) );
	BOOST_CHECK( !BotSFJ_UpdatePursuitLatchEx( 0, 60.0f, 58.0f, 200, 0.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_UpdatePursuitLatchEx( 0, 300.0f, 260.0f, 200, 320.0f, BotSFJ_StopDistanceFor( 320.0f ) ) );
	BOOST_CHECK( BotSFJ_UpdatePursuitLatchEx( 0, 340.0f, 300.0f, 200, 320.0f, BotSFJ_StopDistanceFor( 320.0f ) ) );
	BOOST_CHECK_CLOSE( BotSFJ_StopDistanceFor( 512.0f ), BOT_SFJ_PURSUIT_STOP_DISTANCE, 0.01f );
	BOOST_CHECK_EQUAL( BotSFJ_StopDistanceFor( -5.0f ), 0.0f );
}

BOOST_AUTO_TEST_CASE( strafejump_yaw_uses_command_dt_and_speed )
{
	const float lowSpeedYaw = BotSFJ_CommandYaw( 0.0f, 20.0f, 0.0f, 250.0f, 1.0f, 0.016f, 1 );
	const float highSpeedShort = BotSFJ_CommandYaw( 0.0f, 600.0f, 0.0f, 250.0f, 1.0f, 0.008f, 1 );
	const float highSpeedLong = BotSFJ_CommandYaw( 0.0f, 600.0f, 0.0f, 250.0f, 1.0f, 0.05f, 1 );

	BOOST_CHECK_SMALL( lowSpeedYaw, 0.001f );
	BOOST_CHECK( highSpeedShort < highSpeedLong );
	BOOST_CHECK( isfinite( BotSFJ_CommandYaw( 90.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.016f, -1 ) ) );
	BOOST_CHECK_EQUAL( BotSFJ_SelectSide( 30.0f, 500.0f, 0.0f, -1 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_SelectSide( 330.0f, 500.0f, 0.0f, 1 ), -1 );
}

BOOST_AUTO_TEST_CASE( strafejump_launch_matches_jka_force_jump_substep )
{
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 0, 0.0f, 1 ), 225.0f );
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 1, 0.0f, 1 ), 267.0f );
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 2, 0.0f, 1 ), 284.0f );
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 3, 0.0f, 1 ), 309.0f );
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 3, 0.0f, 0 ), 225.0f );
	BOOST_CHECK_EQUAL( BotSFJ_JKALaunchVelocity( 4, 0.0f, 1 ), 225.0f );
}

BOOST_AUTO_TEST_CASE( strafejump_launch_prediction_accounts_for_command_slices )
{
	float shortHeight, shortVelocity, longHeight, longVelocity;

	BotSFJ_PredictReleasedVertical( 3, 1, 1, 800.0f, 8, 8,
		&shortHeight, &shortVelocity );
	BotSFJ_PredictReleasedVertical( 3, 1, 1, 800.0f, 32, 8,
		&longHeight, &longVelocity );
	BOOST_CHECK( longHeight > shortHeight );
	BOOST_CHECK( longVelocity < shortVelocity );
	BOOST_CHECK_EQUAL( BotSFJ_PmoveSliceMsec( 50, 1, 8, 0 ), 8 );
	BOOST_CHECK_EQUAL( BotSFJ_PmoveSliceMsec( 50, 0, 8, 0 ), 50 );
	BOOST_CHECK_EQUAL( BotSFJ_PmoveSliceMsec( 80, 0, 8, 0 ), 66 );
	BOOST_CHECK_EQUAL( BotSFJ_PmoveSliceMsec( 50, 0, 8, 1 ), 8 );
}

BOOST_AUTO_TEST_CASE( strafejump_effective_movement_honors_direction_overrides )
{
	float x, y;

	BOOST_CHECK( BotSFJ_EffectiveMovement( 1.0f, 0.0f, 400.0f, 90.0f,
		0, 0, 0, 0, &x, &y ) > 0.0f );
	BOOST_CHECK_CLOSE_FRACTION( x, 1.0f, 0.0001f );
	BOOST_CHECK_SMALL( y, 0.0001f );
	BOOST_CHECK( BotSFJ_EffectiveMovement( 0.0f, 0.0f, 0.0f, 90.0f,
		1, 127, 0, 0, &x, &y ) > 0.0f );
	BOOST_CHECK_SMALL( x, 0.0001f );
	BOOST_CHECK_CLOSE_FRACTION( y, 1.0f, 0.0001f );
	BOOST_CHECK_SMALL( BotSFJ_EffectiveMovement( 1.0f, 0.0f, 0.0f, 0.0f,
		0, 0, 0, 0, &x, &y ), 0.0001f );
}

BOOST_AUTO_TEST_CASE( strafejump_landing_cannot_bypass_route_endpoint )
{
	BOOST_CHECK( BotSFJ_LandingWithinCorridor( 0.0f, 0.0f, 100.0f, 0.0f,
		80.0f, 20.0f, 24.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_LandingWithinCorridor( 0.0f, 0.0f, 100.0f, 0.0f,
		101.0f, 0.0f, 24.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_LandingWithinCorridor( 0.0f, 0.0f, 100.0f, 0.0f,
		80.0f, 25.0f, 24.0f, 0.0f ) );
}

BOOST_AUTO_TEST_CASE( strafejump_random_use_does_not_mask_deliberate_use )
{
	BOOST_CHECK( !BotSFJ_UseIsConflict( 0, 1, 1 ) );
	BOOST_CHECK( BotSFJ_UseIsConflict( 0, 1, 0 ) );
	BOOST_CHECK( BotSFJ_UseIsConflict( 1, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_route_safety_rejects_each_hazard_class )
{
	BOOST_CHECK( BotSFJ_RouteSafetyAllows( 1, 1, 1, 0.8f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_RouteSafetyAllows( 0, 1, 1, 0.8f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_RouteSafetyAllows( 1, 0, 1, 0.8f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_RouteSafetyAllows( 1, 1, 0, 0.8f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_RouteSafetyAllows( 1, 1, 1, 0.6f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_RouteSafetyAllows( 1, 1, 1, 0.8f, 0.7f ) );
}

BOOST_AUTO_TEST_CASE( strafejump_route_hint_matches_start_and_route_body )
{
	const float start[3] = { 0.0f, 0.0f, 0.0f };
	const float end[3] = { 1000.0f, 0.0f, 0.0f };
	float progress = -1.0f;
	const float nearStart[3] = { -50.0f, 30.0f, 10.0f };
	const float alongside[3] = { 400.0f, 80.0f, 0.0f };
	const float tooWide[3] = { 400.0f, 120.0f, 0.0f };
	const float pastEnd[3] = { 900.0f, 0.0f, 0.0f };
	const float farBehind[3] = { -200.0f, 0.0f, 0.0f };
	const float tooHigh[3] = { 400.0f, 0.0f, 100.0f };

	BOOST_CHECK( BotSFJ_RouteHintProgress( nearStart, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
	BOOST_CHECK( progress < 0.0f );
	BOOST_CHECK( BotSFJ_RouteHintProgress( alongside, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
	BOOST_CHECK_CLOSE( progress, 0.4f, 0.01f );
	BOOST_CHECK( !BotSFJ_RouteHintProgress( tooWide, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
	BOOST_CHECK( !BotSFJ_RouteHintProgress( pastEnd, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
	BOOST_CHECK( !BotSFJ_RouteHintProgress( farBehind, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
	BOOST_CHECK( !BotSFJ_RouteHintProgress( tooHigh, start, end, 96.0f, 96.0f, 64.0f, 0.85f, &progress ) );
}

BOOST_AUTO_TEST_CASE( strafejump_route_hint_min_speed_gates_late_takeoff )
{
	BOOST_CHECK( BotSFJ_RouteHintSpeedAllows( 0.2f, 1, 300.0f, 450.0f ) );
	BOOST_CHECK( !BotSFJ_RouteHintSpeedAllows( 0.6f, 1, 300.0f, 450.0f ) );
	BOOST_CHECK( BotSFJ_RouteHintSpeedAllows( 0.6f, 1, 460.0f, 450.0f ) );
	BOOST_CHECK( BotSFJ_RouteHintSpeedAllows( 0.6f, 0, 300.0f, 450.0f ) );
	BOOST_CHECK( BotSFJ_RouteHintSpeedAllows( 0.6f, 1, 300.0f, 0.0f ) );
}

BOOST_AUTO_TEST_CASE( ptk_armor_penalty_tracks_force_lead )
{
	BOOST_CHECK_EQUAL( NewBotAI_AdjustPTKWeightForArmor( 50, 0, 0 ), 50 );
	BOOST_CHECK_EQUAL( NewBotAI_AdjustPTKWeightForArmor( 50, 25, -5 ), -10 );
	BOOST_CHECK_EQUAL( NewBotAI_AdjustPTKWeightForArmor( 50, 25, 10 ), 15 );
	BOOST_CHECK_EQUAL( NewBotAI_AdjustPTKWeightForArmor( 50, 25, 25 ), 60 );
}

BOOST_AUTO_TEST_CASE( drainlock_force_choice_prefers_pull_when_it_matches_or_beats_drain )
{
	BOOST_CHECK_EQUAL(
		NewBotAI_GetDrainlockForceChoice( { 0, 1, 0, 70, 70, 40 } ),
		NEWBOTAI_DRAINLOCK_FORCE_PULL );
	BOOST_CHECK_EQUAL(
		NewBotAI_GetDrainlockForceChoice( { 0, 0, 1, 80, 80, 10 } ),
		NEWBOTAI_DRAINLOCK_FORCE_PULL );
}

BOOST_AUTO_TEST_CASE( drainlock_force_choice_falls_back_to_drain_when_pull_loses )
{
	BOOST_CHECK_EQUAL(
		NewBotAI_GetDrainlockForceChoice( { 0, 1, 0, 90, 60, 40 } ),
		NEWBOTAI_DRAINLOCK_FORCE_DRAIN );
	BOOST_CHECK_EQUAL(
		NewBotAI_GetDrainlockForceChoice( { 0, 0, 1, 90, 60, 40 } ),
		NEWBOTAI_DRAINLOCK_FORCE_DRAIN );
	BOOST_CHECK_EQUAL(
		NewBotAI_GetDrainlockForceChoice( { 0, 0, 1, 90, 80, 10 } ),
		NEWBOTAI_DRAINLOCK_FORCE_DRAIN );
}

BOOST_AUTO_TEST_CASE( saber_throw_immediate_hop_thresholds_match_tuning )
{
	BOOST_CHECK( NewBotAI_ShouldForceImmediateSaberThrowHop( 55.0f, 40.0f, 0, 3.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldForceImmediateSaberThrowHop( 75.0f, 20.0f, 1, 3.0f ) );
	BOOST_CHECK( NewBotAI_ShouldForceImmediateSaberThrowHop( 75.0f, 20.0f, 1, 6.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldForceImmediateSaberThrowHop( 95.0f, 30.0f, 0, 8.0f ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_ptk_bonus_stays_low_until_free_pull_window )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberThrowPTKBonus( 0, 0, 100, 50, 40 ), 10 );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberThrowPTKBonus( 0, 1, 100, 50, 40 ), 20 );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberThrowPTKBonus( 1, 0, 100, 60, 20 ), 80 );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberThrowPTKBonus( 1, 1, 100, 60, 20 ), 120 );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberThrowPTKBonus( 1, 1, 20, 20, 40 ), 75 );
}

BOOST_AUTO_TEST_CASE( being_pulled_ptk_bonus_stays_heavy_only_in_exposed_window )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetPulledTowardEnemyPTKBonus( 0, 180.0f ), 90 );
	BOOST_CHECK_EQUAL( NewBotAI_GetPulledTowardEnemyPTKBonus( 1, 180.0f ), 140 );
	BOOST_CHECK_EQUAL( NewBotAI_GetPulledTowardEnemyPTKBonus( 1, 240.0f ), 0 );
}

BOOST_AUTO_TEST_CASE( absorb_bait_window_uses_pullkick_spacing_not_immediate_flipkick_spacing )
{
	BOOST_CHECK( NewBotAI_IsAbsorbBaitWindow( 1, 0, 180.0f, 135.0f, 220.0f ) );
	BOOST_CHECK( NewBotAI_IsAbsorbBaitWindow( 0, 1, 200.0f, 135.0f, 220.0f ) );
	BOOST_CHECK( !NewBotAI_IsAbsorbBaitWindow( 1, 0, 120.0f, 135.0f, 220.0f ) );
	BOOST_CHECK( !NewBotAI_IsAbsorbBaitWindow( 0, 0, 180.0f, 135.0f, 220.0f ) );
}

BOOST_AUTO_TEST_CASE( absorb_bias_bonus_scales_with_bias_and_window_state )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetAbsorbBiasBonus( 0, 1, 0, 180.0f, 135.0f, 220.0f ), 30 );
	BOOST_CHECK_EQUAL( NewBotAI_GetAbsorbBiasBonus( 100, 1, 1, 180.0f, 135.0f, 220.0f ), 80 );
	BOOST_CHECK_EQUAL( NewBotAI_GetAbsorbBiasBonus( 50, 0, 0, 180.0f, 135.0f, 220.0f ), 0 );
}

BOOST_AUTO_TEST_CASE( anti_dark_push_and_drain_bonuses_require_advantage_windows )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetAntiDarkPushBonus( 1, 1, 20, 150.0f, 220.0f ), 95 );
	BOOST_CHECK_EQUAL( NewBotAI_GetAntiDarkPushBonus( 1, 0, 20, 150.0f, 220.0f ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetAntiDarkDrainBonus( 1, 1, 15, 1 ), 90 );
	BOOST_CHECK_EQUAL( NewBotAI_GetAntiDarkDrainBonus( 1, 0, 15, 1 ), 0 );
}

BOOST_AUTO_TEST_CASE( login_reminder_cadence_starts_and_repeats_on_interval )
{
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 2, 3, 3, 0 ) );
	BOOST_CHECK( NewBotAI_ShouldQueueLoginReminder( 3, 3, 3, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 3, 3, 3, 3 ) );
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 4, 3, 3, 0 ) );
	BOOST_CHECK( NewBotAI_ShouldQueueLoginReminder( 6, 3, 3, 3 ) );
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 5, 3, 3, 6 ) );
}

BOOST_AUTO_TEST_CASE( login_reminder_state_updates_prevent_repeat_until_next_interval )
{
	int lastPromptDuel = 0;

	BOOST_CHECK( NewBotAI_ShouldQueueLoginReminder( 3, 3, 3, lastPromptDuel ) );
	lastPromptDuel = 3;
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 4, 3, 3, lastPromptDuel ) );
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 5, 3, 3, lastPromptDuel ) );
	BOOST_CHECK( NewBotAI_ShouldQueueLoginReminder( 6, 3, 3, lastPromptDuel ) );
	lastPromptDuel = 6;
	BOOST_CHECK( !NewBotAI_ShouldQueueLoginReminder( 6, 3, 3, lastPromptDuel ) );
}

BOOST_AUTO_TEST_CASE( immediate_flipkick_contact_widens_yaw_tolerance )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetImmediateFlipkickYawTolerance( 0 ), 35.0f );
	BOOST_CHECK_EQUAL( NewBotAI_GetImmediateFlipkickYawTolerance( 1 ), 60.0f );
}

BOOST_AUTO_TEST_CASE( speed_attack_abort_respects_grip_and_drain_traps )
{
	BOOST_CHECK( NewBotAI_ShouldAbortSpeedAttack( 1, 0, 0, 80, 20, 200.0f, 256.0f ) );
	BOOST_CHECK( NewBotAI_ShouldAbortSpeedAttack( 0, 1, 0, 80, 20, 200.0f, 256.0f ) );
	BOOST_CHECK( NewBotAI_ShouldAbortSpeedAttack( 0, 0, 1, 35, 0, 200.0f, 256.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldAbortSpeedAttack( 0, 0, 1, 60, 20, 300.0f, 256.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldAbortSpeedAttack( 0, 0, 0, 80, 20, 200.0f, 256.0f ) );
}

BOOST_AUTO_TEST_CASE( speed_finisher_window_requires_true_kill_pressure )
{
	BOOST_CHECK( NewBotAI_IsSpeedFinisherWindow( 30, 0 ) );
	BOOST_CHECK( NewBotAI_IsSpeedFinisherWindow( 34, 15 ) );
	BOOST_CHECK( !NewBotAI_IsSpeedFinisherWindow( 45, 4 ) );
	BOOST_CHECK( !NewBotAI_IsSpeedFinisherWindow( 40, 10 ) );
	BOOST_CHECK( !NewBotAI_IsSpeedFinisherWindow( 30, 20 ) );
}

BOOST_AUTO_TEST_CASE( speed_resource_gate_uses_total_health_lead_with_armor )
{
	BOOST_CHECK( NewBotAI_PassesSpeedAttackResourceLeadGate( 80, 30, 35, 40, 0.5f, 90, 60 ) );
	BOOST_CHECK( NewBotAI_PassesSpeedAttackResourceLeadGate( 65, 35, 35, 40, 0.5f, 90, 60 ) );
	BOOST_CHECK( NewBotAI_PassesSpeedAttackResourceLeadGate( 70, 1, 35, 11, 0.5f, 85, 70 ) ); // total 71, lead 25, force lead 15
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 80, 0, 35, 40, 0.5f, 90, 60 ) );
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 80, 30, 35, 40, 0.2f, 90, 60 ) );
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 80, 30, 35, 40, 0.5f, 60, 50 ) );
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 65, 5, 35, 40, 0.5f, 90, 60 ) );
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 70, 0, 35, 10, 0.5f, 85, 70 ) ); // total 70 cutoff
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 70, 1, 35, 12, 0.5f, 85, 70 ) ); // lead 24 cutoff
	BOOST_CHECK( !NewBotAI_PassesSpeedAttackResourceLeadGate( 70, 1, 35, 11, 0.5f, 84, 70 ) ); // force lead 14 cutoff
}

BOOST_AUTO_TEST_CASE( escape_yaw_override_forces_fixed_turn_rate )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetViewAngleAxisFactor( 0.35f, 1, 1 ), 1.0f );
	BOOST_CHECK_EQUAL( NewBotAI_GetViewAngleAxisFactor( 0.35f, 0, 1 ), 0.35f );
	BOOST_CHECK_EQUAL( NewBotAI_GetViewAngleAxisMaxChange( 90.0f, 0.05f, 1, 1 ), NEWBOTAI_TUNING_ESCAPE_YAW_SPEED * 0.05f );
	BOOST_CHECK_EQUAL( NewBotAI_GetViewAngleAxisMaxChange( 90.0f, 0.05f, 1, 0 ), 90.0f );
}

BOOST_AUTO_TEST_CASE( accidental_special_guard_uses_effective_inputs )
{
	BOOST_CHECK( NewBotAI_ShouldBlockOrthogonalSaberSpecialInput( 1, -1, 0, 1, 0, 1 ) );
	BOOST_CHECK_EQUAL( NewBotAI_GetEffectiveMoveInput( 0, -1 ), -1 );
	BOOST_CHECK( !NewBotAI_ShouldBlockOrthogonalSaberSpecialInput( 1, -1, 1, 1, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldBlockOrthogonalSaberSpecialInput( 1, 0, 0, 1, 0, 1 ) );
}

BOOST_AUTO_TEST_CASE( jump_attack_gate_covers_post_jump_window )
{
	BOOST_CHECK( NewBotAI_IsJumpAttackSuppressionWindowActive( 1000, 1000, 40 ) );
	BOOST_CHECK( !NewBotAI_IsJumpAttackSuppressionWindowActive( 960, 1000, 40 ) );
	BOOST_CHECK( NewBotAI_IsJumpAttackSuppressionWindowActive( 1040, 1000, 40 ) );
	BOOST_CHECK( !NewBotAI_IsJumpAttackSuppressionWindowActive( 959, 1000, 40 ) );
	BOOST_CHECK( !NewBotAI_IsJumpAttackSuppressionWindowActive( 1041, 1000, 40 ) );
}

BOOST_AUTO_TEST_CASE( jump_attack_gate_clears_primary_and_alt_flags )
{
	int doAttack = 1;
	int doAltAttack = 1;
	NewBotAI_ApplyJumpAttackSuppression( 1, &doAttack, &doAltAttack );
	BOOST_CHECK_EQUAL( doAttack, 0 );
	BOOST_CHECK_EQUAL( doAltAttack, 0 );
}

BOOST_AUTO_TEST_CASE( contextual_strafe_frequency_applies_modifiers_and_clamps )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetContextualStrafeFrequency( 60, 1, 1, 0, 0 ), 100 );
	BOOST_CHECK_EQUAL( NewBotAI_GetContextualStrafeFrequency( 60, 0, 0, 1, 1 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetContextualStrafeFrequency( 40, 0, 1, 1, 0 ), 35 );
}

BOOST_AUTO_TEST_CASE( pull_skip_for_natural_flipkick_respects_range_and_readiness )
{
	BOOST_CHECK( NewBotAI_ShouldSkipPullForNaturalFlipkick( 1, 1, 1, 1, 260.0f, -1.0f ) );
	BOOST_CHECK( NewBotAI_ShouldSkipPullForNaturalFlipkick( 1, 1, 1, 0, 200.0f, 250.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldSkipPullForNaturalFlipkick( 0, 1, 1, 1, 120.0f, 100.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldSkipPullForNaturalFlipkick( 1, 1, 1, 0, 260.0f, 250.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldSkipPullForNaturalFlipkick( 1, 1, 1, 0, 200.0f, 500.0f ) );
}

BOOST_AUTO_TEST_CASE( duel_request_cooldown_is_mode_and_participant_specific )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetDuelRequestCooldownMs( 7000, 120000, 1, 1, 1 ), 120000 );
	BOOST_CHECK_EQUAL( NewBotAI_GetDuelRequestCooldownMs( 7000, 120000, 0, 1, 1 ), 7000 );
	BOOST_CHECK_EQUAL( NewBotAI_GetDuelRequestCooldownMs( 7000, 120000, 1, 0, 1 ), 7000 );
}

BOOST_AUTO_TEST_CASE( bot_saber_loss_guard_respects_cvar )
{
	BOOST_CHECK( NewBotAI_ShouldIgnoreBotSaberLoss( 1, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldIgnoreBotSaberLoss( 1, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldIgnoreBotSaberLoss( 0, 1 ) );
}

BOOST_AUTO_TEST_CASE( fan_wobble_activation_respects_fan_attack_delay_and_axes )
{
	BOOST_CHECK( !NewBotAI_ShouldApplyFanWobble( 0, 1, 200, 120, 6.0f, 2.0f, 2.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldApplyFanWobble( 1, 0, 200, 120, 6.0f, 2.0f, 2.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldApplyFanWobble( 1, 1, 100, 120, 6.0f, 2.0f, 2.0f ) );
	BOOST_CHECK( NewBotAI_ShouldApplyFanWobble( 1, 1, 120, 120, 6.0f, 2.0f, 2.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldApplyFanWobble( 1, 1, 200, 120, 0.0f, 0.0f, 2.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldApplyFanWobble( 1, 1, 200, 120, 6.0f, 2.0f, 0.0f ) );
	BOOST_CHECK( NewBotAI_ShouldApplyFanWobble( 1, 1, 200, 120, 6.0f, 0.0f, 2.0f ) );
	BOOST_CHECK( NewBotAI_ShouldApplyFanWobble( 1, 1, 200, 120, 0.0f, 2.0f, 2.0f ) );
}

BOOST_AUTO_TEST_CASE( fan_wobble_offsets_match_phase_and_axis_amplitudes )
{
	float yawOffset = 0.0f;
	float pitchOffset = 0.0f;

	NewBotAI_GetFanWobbleOffsets( 0.0f, 6.0f, 2.0f, 2.0f, &yawOffset, &pitchOffset );
	BOOST_CHECK_CLOSE_FRACTION( yawOffset, 6.0f, 0.0001f );
	BOOST_CHECK_SMALL( pitchOffset, 0.0001f );

	NewBotAI_GetFanWobbleOffsets( 125.0f, 6.0f, 2.0f, 2.0f, &yawOffset, &pitchOffset );
	BOOST_CHECK_SMALL( yawOffset, 0.0001f );
	BOOST_CHECK_CLOSE_FRACTION( pitchOffset, -2.0f, 0.0001f );

	NewBotAI_GetFanWobbleOffsets( 125.0f, 6.0f, 0.0f, 2.0f, &yawOffset, &pitchOffset );
	BOOST_CHECK_SMALL( yawOffset, 0.0001f );
	BOOST_CHECK_SMALL( pitchOffset, 0.0001f );

	NewBotAI_GetFanWobbleOffsets( 300.0f, 6.0f, 2.0f, 2.0f, &yawOffset, &pitchOffset );
	BOOST_CHECK( yawOffset < 0.0f );
}

BOOST_AUTO_TEST_CASE( saber_tactic_mistakes_scale_down_to_zero_at_level_ten )
{
	BOOST_CHECK( NewBotAI_GetSaberTacticMistakeChance( 2, 70 ) >
		NewBotAI_GetSaberTacticMistakeChance( 8, 70 ) );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberTacticMistakeChance( 10, 100 ), 0 );
}

static newbotai_saber_tactic_context_t MakeSaberDuelContext( float distance )
{
	newbotai_saber_tactic_context_t context = {};
	context.saberOnlyDuel = 1;
	context.skill = 10;
	context.ourTotalHealth = 150;
	context.enemyTotalHealth = 150;
	context.enemyDistance = distance;
	return context;
}

BOOST_AUTO_TEST_CASE( saber_duels_always_use_saber_duel_path )
{
	BOOST_CHECK( NewBotAI_UsesSaberDuelPath( 1, 0, 1, 1 ) );
	BOOST_CHECK( NewBotAI_UsesSaberDuelPath( 1, 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_UsesSaberDuelPath( 0, 163837, 0, 1 ) );
	BOOST_CHECK( NewBotAI_UsesSaberDuelPath( 0, 163839, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_UsesSaberDuelPath( 0, 163837, 1, 1 ) );
	BOOST_CHECK( !NewBotAI_UsesSaberDuelPath( 0, 163837, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_UsesSaberDuelPath( 0, 0, 0, 1 ) );
}

BOOST_AUTO_TEST_CASE( saber_tactic_ranges_match_winner_spacing )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 220.0f );
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_ADVANCE );

	context.enemyDistance = 140.0f;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_STEP_IN );

	context.enemyDistance = 80.0f;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_ATTACK );

	context.enemyDistance = 90.0f;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_STEP_IN );

	context = MakeSaberDuelContext( 0.0f );
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_ATTACK );
	context.saberOnlyDuel = 0;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_HOLD );
}

BOOST_AUTO_TEST_CASE( saber_tactic_swing_starts_are_limited_to_reach )
{
	BOOST_CHECK( NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_STEP_IN, 80.0f ) );
	// No 85-100u dead zone: step in and swing together.
	BOOST_CHECK( NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_STEP_IN, 90.0f ) );
	BOOST_CHECK( NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_STEP_IN, 100.0f ) );
	BOOST_CHECK( !NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_STEP_IN, 105.0f ) );
	BOOST_CHECK( !NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_ATTACK, 129.0f ) );
	BOOST_CHECK( !NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_ADVANCE, 60.0f ) );
	BOOST_CHECK( !NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_RESET, 60.0f ) );
	BOOST_CHECK( NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_LONG_SWING, 200.0f ) );
	BOOST_CHECK( NewBotAI_SaberTacticHoldsChain( NEWBOTAI_SABER_TACTIC_CHAIN ) );
	BOOST_CHECK( !NewBotAI_SaberTacticHoldsChain( NEWBOTAI_SABER_TACTIC_REPOSITION ) );
}

BOOST_AUTO_TEST_CASE( saber_tactic_continues_safe_hits_but_bounds_exposed_pressure )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	context.selfAttacking = 1;
	context.landedHit = 1;
	context.chainLength = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_CHAIN );
	context.chainLength = 12;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_CHAIN );
	context.landedHit = 0;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );
	context.landedHit = 1;
	context.chainLength = 2;
	context.enemyAttacking = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );

	context.enemyAttacking = 0;
	context.chainLength = 0;
	context.selfAttacking = 0;
	context.enemyDistance = 140.0f;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_STEP_IN );
}

BOOST_AUTO_TEST_CASE( saber_tactic_counters_recovery_not_every_incoming_swing )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	context.enemyVulnerable = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_COUNTER );

	context.enemyVulnerable = 0;
	context.recentlyHurt = 1;
	context.ourTotalHealth = 60;
	context.counterReady = 1;
	context.enemyRecovering = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_COUNTER );

	context.recentlyHurt = 0;
	context.enemyRecovering = 0;
	context.enemyAttacking = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_REPOSITION );

	context.enemyDistance = 150.0f;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_STEP_IN );

	context.enemyDistance = 80.0f;
	context.recentlyHurt = 1;
	context.ourTotalHealth = 25;
	// Saber-only cannot heal: low health alone does not send the bot away.
	BOOST_CHECK( NewBotAI_SelectSaberTactic( context ) != NEWBOTAI_SABER_TACTIC_RESET );
	context.chainLength = 2;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );
	context.chainLength = 0;
	context.saberOnlyDuel = 0;
	context.saberCombat = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );
}

BOOST_AUTO_TEST_CASE( saber_choice_grade_is_always_correct_at_level_ten )
{
	for ( int roll = 1; roll <= 100; roll++ )
	{
		BOOST_CHECK_EQUAL( NewBotAI_GetSaberChoiceGrade( 10, 100, roll ), NEWBOTAI_SABER_GRADE_CORRECT );
	}
}

BOOST_AUTO_TEST_CASE( saber_choice_grade_tiers_scale_with_skill_and_bias )
{
	int lowErrors = 0, highErrors = 0, lowDeep = 0, highDeep = 0;

	for ( int roll = 1; roll <= 100; roll++ )
	{
		const newbotai_saber_grade_t low = NewBotAI_GetSaberChoiceGrade( 2, 80, roll );
		const newbotai_saber_grade_t high = NewBotAI_GetSaberChoiceGrade( 8, 80, roll );
		lowErrors += ( low != NEWBOTAI_SABER_GRADE_CORRECT );
		highErrors += ( high != NEWBOTAI_SABER_GRADE_CORRECT );
		lowDeep += ( low == NEWBOTAI_SABER_GRADE_BAD || low == NEWBOTAI_SABER_GRADE_MISTAKE );
		highDeep += ( high == NEWBOTAI_SABER_GRADE_BAD || high == NEWBOTAI_SABER_GRADE_MISTAKE );
	}
	BOOST_CHECK( lowErrors > highErrors );
	BOOST_CHECK( lowDeep > highDeep );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberChoiceGrade( 2, 80, 1 ), NEWBOTAI_SABER_GRADE_MISTAKE );
	BOOST_CHECK( NewBotAI_GetSaberChoiceGrade( 5, 0, 100 ) == NEWBOTAI_SABER_GRADE_CORRECT );
}

BOOST_AUTO_TEST_CASE( saber_choice_grades_map_to_human_outcomes )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );

	context.chainLength = 3;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_GRADE_GOOD ),
		NEWBOTAI_SABER_TACTIC_REPOSITION );
	context.chainLength = 1;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_GRADE_GOOD ),
		NEWBOTAI_SABER_TACTIC_CHAIN );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_STEP_IN, NEWBOTAI_SABER_GRADE_MEDIOCRE ),
		NEWBOTAI_SABER_TACTIC_STAND_SWING );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_GRADE_BAD ),
		NEWBOTAI_SABER_TACTIC_RESET );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_COUNTER, NEWBOTAI_SABER_GRADE_BAD ),
		NEWBOTAI_SABER_TACTIC_REPOSITION );
	// Backpedal swings are a low-skill mistake only (humans: 0% of swing starts).
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_GRADE_MISTAKE ),
		NEWBOTAI_SABER_TACTIC_STAND_SWING );
	context.skill = 4;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_GRADE_MISTAKE ),
		NEWBOTAI_SABER_TACTIC_BACK_SWING );
	context.skill = 10;
	context.enemyDistance = 200.0f;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_ADVANCE, NEWBOTAI_SABER_GRADE_MISTAKE ),
		NEWBOTAI_SABER_TACTIC_LONG_SWING );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_ADVANCE, NEWBOTAI_SABER_GRADE_CORRECT ),
		NEWBOTAI_SABER_TACTIC_ADVANCE );
}

BOOST_AUTO_TEST_CASE( red_stance_swing_start_and_attack_suppression )
{
	BOOST_CHECK( NewBotAI_ShouldStartRedStanceSwing( 80.0f, 300, 100 ) );
	BOOST_CHECK( NewBotAI_ShouldStartRedStanceSwing( 150.0f, 200, 100 ) );
	BOOST_CHECK( !NewBotAI_ShouldStartRedStanceSwing( 150.0f, 300, 100 ) );
	BOOST_CHECK( !NewBotAI_ShouldStartRedStanceSwing( 80.0f, 0, 20 ) );

	BOOST_CHECK( !NewBotAI_ShouldSuppressSaberAttackForDeficit( -1, 1 ) );
	BOOST_CHECK( NewBotAI_ShouldSuppressSaberAttackForDeficit( -30, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldSuppressSaberAttackForDeficit( -30, 10 ) );
	BOOST_CHECK( NewBotAI_ShouldSuppressSaberAttackForDeficit( -70, 10 ) );
}

BOOST_AUTO_TEST_CASE( fan_footwork_follows_human_spacing )
{
	BOOST_CHECK( !NewBotAI_IsFanEntrySpacing( 40.0f ) );
	BOOST_CHECK( NewBotAI_IsFanEntrySpacing( 48.0f ) );
	BOOST_CHECK( NewBotAI_IsFanEntrySpacing( 110.0f ) );
	BOOST_CHECK( !NewBotAI_IsFanEntrySpacing( 160.0f ) );

	BOOST_CHECK( !NewBotAI_FanHoldUsesForward( 0, 90.0f ) );
	BOOST_CHECK( NewBotAI_FanHoldUsesForward( 1, 90.0f ) );
	BOOST_CHECK( !NewBotAI_FanHoldUsesForward( 1, 40.0f ) );

	BOOST_CHECK_EQUAL( NewBotAI_ScaleSaberDuelFanBias( 0.0f, 50 ), 0.0f );
	BOOST_CHECK_CLOSE( NewBotAI_ScaleSaberDuelFanBias( 60.0f, 0 ), 60.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_ScaleSaberDuelFanBias( 60.0f, -40 ), 45.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_ScaleSaberDuelFanBias( 60.0f, -200 ), 45.0f, 0.001f );
	BOOST_CHECK( NewBotAI_ScaleSaberDuelFanBias( 60.0f, -60 ) > 0.0f );
}

BOOST_AUTO_TEST_CASE( human_technique_bias_weights_families_not_primary_attack_permission )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	int simple = 0, complex = 0;
	for ( int roll = 1; roll <= 100; roll++ )
	{
		BOOST_CHECK_EQUAL( NewBotAI_SelectSaberFamily( context, 0, roll ), NEWBOTAI_SABER_BASIC );
		simple += NewBotAI_SelectSaberFamily( context, 25, roll ) != NEWBOTAI_SABER_BASIC;
		complex += NewBotAI_SelectSaberFamily( context, 75, roll ) != NEWBOTAI_SABER_BASIC;
		const newbotai_saber_command_t command = NewBotAI_PlanSaberCommand( context,
			NEWBOTAI_SABER_TACTIC_ATTACK, NewBotAI_SelectSaberFamily( context, 0, roll ),
			0, 1, 1, 1, 1, 0 );
		BOOST_CHECK_EQUAL( command.attack, 1 );
	}
	BOOST_CHECK_EQUAL( simple, 25 );
	BOOST_CHECK_EQUAL( complex, 75 );
	context.enemyTotalHealth = 30;
	context.landedHit = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberFamily( context, 100, 100 ), NEWBOTAI_SABER_FINISH );
	context.enemyTotalHealth = 150;
	context.recentlyHurt = 1;
	context.counterReady = 1;
	context.enemyRecovering = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberFamily( context, 100, 1 ), NEWBOTAI_SABER_COUNTER_ENTRY );
	context.enemyRecovering = 0;
	context.enemyAttacking = 1;
	context.ourTotalHealth = 100;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberFamily( context, 100, 1 ), NEWBOTAI_SABER_BURST );
}

BOOST_AUTO_TEST_CASE( human_saber_sequence_uses_engine_accepted_attacks_only )
{
	BOOST_CHECK( NewBotAI_SaberMoveAccepted( 10, 9, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberMoveAccepted( 10, 10, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberMoveAccepted( 11, 10, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstComplete( NEWBOTAI_SABER_BURST, 1, 1 ) );
	BOOST_CHECK( NewBotAI_SaberBurstComplete( NEWBOTAI_SABER_BURST, 2, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstComplete( NEWBOTAI_SABER_BURST, 12, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstComplete( NEWBOTAI_SABER_HORIZONTAL, 12, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstComplete( NEWBOTAI_SABER_FINISH, 12, 1 ) );
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	const newbotai_saber_command_t first = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_DIAGONAL_VERTICAL, 0, -1, 1, 1, 1, 0 );
	const newbotai_saber_command_t second = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_DIAGONAL_VERTICAL, 1, 1, 1, 1, 1, 0 );
	const newbotai_saber_command_t finisher = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_DIAGONAL_VERTICAL, 2, -1, 1, 1, 1, 0 );
	BOOST_CHECK_EQUAL( first.forward, 1 );
	BOOST_CHECK_EQUAL( first.right, -1 );
	BOOST_CHECK_EQUAL( second.forward, 1 );
	BOOST_CHECK_EQUAL( second.right, 0 );
	BOOST_CHECK_EQUAL( finisher.forward, 0 );
	BOOST_CHECK_EQUAL( finisher.right, -1 );
	BOOST_CHECK_EQUAL( finisher.attack, 1 );
	const newbotai_saber_command_t running = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_HORIZONTAL, 1, 1, 1, 0, 1, 0 );
	BOOST_CHECK_EQUAL( running.forward, 1 );
	BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_ATTACK,
		NEWBOTAI_SABER_HORIZONTAL, 0, 1, 1, 1, 1, 0 ).forward, 0 );
}

BOOST_AUTO_TEST_CASE( experimental_recipes_preserve_finish_counter_burst_and_zero_bias )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	BOOST_CHECK( NewBotAI_SaberFamilyMaySelectRecipe( NEWBOTAI_SABER_HORIZONTAL ) );
	BOOST_CHECK( NewBotAI_SaberFamilyMaySelectRecipe( NEWBOTAI_SABER_DIAGONAL_VERTICAL ) );
	for (auto family : { NEWBOTAI_SABER_BASIC, NEWBOTAI_SABER_FINISH,
		NEWBOTAI_SABER_COUNTER_ENTRY, NEWBOTAI_SABER_BURST })
		BOOST_CHECK( !NewBotAI_SaberFamilyMaySelectRecipe( family ) );
	for (int roll = 1; roll <= 100; ++roll)
	{
		const auto family = NewBotAI_SelectSaberFamily( context, 0, roll );
		BOOST_CHECK_EQUAL( family, NEWBOTAI_SABER_BASIC );
		BOOST_CHECK( !NewBotAI_SaberFamilyMaySelectRecipe( family ) );
		BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_ATTACK,
			family, 0, 1, 1, 1, 1, 0 ).attack, 1 );
	}
	context.fanStance = 1;
	context.enemyTotalHealth = 30;
	context.landedHit = 1;
	auto family = NewBotAI_SelectSaberFamily( context, 100, 1 );
	BOOST_CHECK_EQUAL( family, NEWBOTAI_SABER_FINISH );
	BOOST_CHECK( !NewBotAI_SaberFamilyMaySelectRecipe( family ) );
	const auto finish = NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_CHAIN,
		family, NEWBOTAI_SABER_FAN_FINISH_STAGE, 1, 1, 1, 1, 0 );
	BOOST_CHECK_EQUAL( finish.attack, 1 );
	BOOST_CHECK_EQUAL( finish.forward, 1 );
	BOOST_CHECK_EQUAL( finish.right, 0 );
	context.enemyTotalHealth = 150;
	context.landedHit = 0;
	context.recentlyHurt = 1;
	context.counterReady = 1;
	context.enemyRecovering = 1;
	family = NewBotAI_SelectSaberFamily( context, 100, 1 );
	BOOST_CHECK_EQUAL( family, NEWBOTAI_SABER_COUNTER_ENTRY );
	BOOST_CHECK( !NewBotAI_SaberFamilyMaySelectRecipe( family ) );
	BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_COUNTER,
		family, 0, 1, 1, 1, 1, 0 ).attack, 1 );
}

BOOST_AUTO_TEST_CASE( human_saber_mix_is_deliberate_and_horizontal_sweeps_remain_primary )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	int horizontal = 0, mixed = 0;
	for ( int roll = 1; roll <= 100; roll++ )
		BOOST_CHECK_EQUAL( NewBotAI_SelectSaberFamily( context, 100, roll ), NEWBOTAI_SABER_HORIZONTAL );
	context.enemyRecovering = 1;
	for ( int roll = 1; roll <= 100; roll++ )
	{
		const newbotai_saber_family_t family = NewBotAI_SelectSaberFamily( context, 100, roll );
		horizontal += family == NEWBOTAI_SABER_HORIZONTAL;
		mixed += family == NEWBOTAI_SABER_DIAGONAL_VERTICAL;
	}
	BOOST_CHECK_EQUAL( horizontal, 90 );
	BOOST_CHECK_EQUAL( mixed, 10 );
	for ( int family : { NEWBOTAI_SABER_BASIC, NEWBOTAI_SABER_DIAGONAL_VERTICAL } )
	{
		int horizontalStarts = 0, diagonalStarts = 0, verticalStarts = 0;
		for ( int stage = 0; stage < 3; stage++ )
		{
			// Stage is an accepted-attack count, never a timer/held-input counter.
			for ( int preparationFrame = 0; preparationFrame < 5; preparationFrame++ )
			{
				const newbotai_saber_command_t command = NewBotAI_PlanSaberCommand( context,
					NEWBOTAI_SABER_TACTIC_ATTACK, (newbotai_saber_family_t)family,
					stage, -1, 1, 1, 1, 0 );
				BOOST_CHECK_EQUAL( command.attack, 1 );
				if (!preparationFrame)
				{
					horizontalStarts += command.forward == 0 && command.right == -1;
					diagonalStarts += command.forward == 1 && command.right == -1;
					verticalStarts += command.forward == 1 && command.right == 0;
				}
				int forward, right;
				NewBotAI_SaberSelectionInputs((newbotai_saber_family_t)family, stage, -1, &forward, &right);
				BOOST_CHECK_EQUAL( command.forward, forward );
				BOOST_CHECK_EQUAL( command.right, right );
			}
		}
		BOOST_CHECK_EQUAL( horizontalStarts, 1 );
		BOOST_CHECK_EQUAL( diagonalStarts, 1 );
		BOOST_CHECK_EQUAL( verticalStarts, 1 );
	}
	newbotai_saber_command_t vertical = { 1, 0, 0, 1 };
	NewBotAI_SaberGuardSelection( &vertical, 1, 1, 0 );
	BOOST_CHECK_EQUAL( vertical.attack, 1 );
	BOOST_CHECK_EQUAL( vertical.forward, 1 );
	newbotai_saber_command_t suppressedDiagonal = { 1, 0, 0, 1 };
	NewBotAI_SaberGuardSelection( &suppressedDiagonal, 1, 1, -1 );
	BOOST_CHECK_EQUAL( suppressedDiagonal.attack, 0 );
}

BOOST_AUTO_TEST_CASE( slow_red_start_and_transition_keep_lateral_selection_until_active )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	context.selfAttacking = 1; // Also true during engine starts and transitions.
	for ( int remaining : { 1200, 800, 300, 99, 0 } )
	{
		const newbotai_saber_yaw_phase_t phase =
			NewBotAI_SaberYawPhase( 0, 1, remaining, 1200 );
		BOOST_CHECK_EQUAL( phase, NEWBOTAI_SABER_YAW_PREPARE );
		for ( int family : { NEWBOTAI_SABER_HORIZONTAL, NEWBOTAI_SABER_COUNTER_ENTRY,
			NEWBOTAI_SABER_FINISH, NEWBOTAI_SABER_BURST } )
		{
			const newbotai_saber_command_t command = NewBotAI_PlanSaberCommand( context,
				NEWBOTAI_SABER_TACTIC_CHAIN, (newbotai_saber_family_t)family, 1, -1,
				1, phase != NEWBOTAI_SABER_YAW_ACTIVE, 1, 0 );
			BOOST_CHECK_EQUAL( command.forward, 0 );
			BOOST_CHECK_EQUAL( command.right, -1 );
			BOOST_CHECK_EQUAL( command.attack, 1 );
		}
	}
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawPhase( 1, 0, 1000, 1000 ), NEWBOTAI_SABER_YAW_PREPARE );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawPhase( 1, 0, 600, 1000 ), NEWBOTAI_SABER_YAW_ACTIVE );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawPhase( 1, 0, 150, 1000 ), NEWBOTAI_SABER_YAW_NEXT );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawPhase( 1, 0, 0, 1000 ), NEWBOTAI_SABER_YAW_NEXT );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawPhase( 0, 0, 500, 800 ), NEWBOTAI_SABER_YAW_RECOVER );
	// A held expired animation never retains forward pressure at selection.
	const newbotai_saber_command_t next = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_HORIZONTAL, 1, -1, 1,
		NewBotAI_SaberYawPhase( 1, 0, 0, 1000 ) != NEWBOTAI_SABER_YAW_ACTIVE, 1, 0 );
	BOOST_CHECK_EQUAL( next.forward, 0 );
	BOOST_CHECK( NewBotAI_SaberCanApplyPressure( NEWBOTAI_SABER_YAW_ACTIVE, 1, 400 ) );
	BOOST_CHECK( !NewBotAI_SaberCanApplyPressure( NEWBOTAI_SABER_YAW_ACTIVE, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanApplyPressure( NEWBOTAI_SABER_YAW_ACTIVE, 0, 400 ) );
	BOOST_CHECK( !NewBotAI_SaberCanApplyPressure( NEWBOTAI_SABER_YAW_PREPARE, 1, 400 ) );
	BOOST_CHECK( !NewBotAI_SaberCanApplyPressure( NEWBOTAI_SABER_YAW_NEXT, -1, 50 ) );
}

BOOST_AUTO_TEST_CASE( actual_horizontal_move_drives_sweep_sign_and_next_preparation )
{
	// Model the engine accepting L2R even if the request was R2L.
	const int accepted = NewBotAI_SaberHorizontalDirection( 5, 5, 8 );
	BOOST_CHECK_EQUAL( accepted, 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberNextHorizontalDirection( accepted, -1 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberNextHorizontalDirection( -1, -1 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberNextHorizontalDirection( 0, -1 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHorizontalDirection( 8, 5, 8 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHorizontalDirection( 4, 5, 8 ), 0 );
	for ( int direction : { -1, 1 } )
	{
		const float amplitude = NEWBOTAI_SABER_SWEEP_DEGREES;
		BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_PREPARE,
			0.0f, direction, -direction, 1 ), direction * amplitude, 0.001f );
		BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_ACTIVE,
			0.15f, direction, -direction, 1 ), direction * amplitude, 0.001f );
		BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_ACTIVE,
			0.8f, direction, -direction, 1 ), -direction * amplitude, 0.001f );
		BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_NEXT,
			1.0f, direction, -direction, 1 ), -direction * amplitude, 0.001f );
		BOOST_CHECK_EQUAL( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_RECOVER,
			1.0f, direction, -direction, 1 ), 0.0f );
		BOOST_CHECK_EQUAL( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_ACTIVE,
			0.5f, direction, -direction, 0 ), 0.0f );
	}
}

BOOST_AUTO_TEST_CASE( coordinated_saber_yaw_wraps_and_bounds_final_command_rate )
{
	BOOST_CHECK_CLOSE( NewBotAI_SaberApplyYawOffset( 359.0f, 2.0f ), 1.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberApplyYawOffset( 1.0f, -2.0f ), 359.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberStepYawOffset( 0.0f, 18.0f, 10 ), 2.4f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberStepYawOffset( 18.0f, -18.0f, 1000 ), -6.0f, 0.001f );
	BOOST_CHECK_EQUAL( NewBotAI_SaberStepYawOffset( 10.0f, 18.0f, -1 ), 10.0f );
	BOOST_CHECK_EQUAL( NewBotAI_SaberStepYawOffset( 0.0f, 180.0f, 100 ), 18.0f );
	// Detach last command's offset before normal base aim, then reapply exactly once.
	float yaw = 359.0f;
	for ( int frame = 0; frame < 20; frame++ )
	{
		yaw = NewBotAI_SaberApplyYawOffset( yaw, 18.0f );
		BOOST_CHECK_CLOSE( yaw, 17.0f, 0.001f );
		yaw = NewBotAI_SaberApplyYawOffset( yaw, -18.0f );
		BOOST_CHECK_CLOSE( yaw, 359.0f, 0.001f );
	}
	// Moving the base aim still tracks a moving enemy throughout a held sweep.
	BOOST_CHECK_CLOSE( NewBotAI_SaberApplyYawOffset( 10.0f, 18.0f ), 28.0f, 0.001f );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAnimationProgress( 500, 0 ), 0.0f );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAnimationProgress( 1500, 1000 ), 0.0f );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAnimationProgress( -20, 1000 ), 1.0f );
}

BOOST_AUTO_TEST_CASE( saber_command_ownership_replaces_conflicts_and_preserves_force_actions )
{
	const int movement = 1 | 2 | 4 | 8;
	const int attack = 16;
	const int altAttack = 32;
	const int force = 64;
	const int use = 128;
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( movement | altAttack | force | use,
		movement | attack | altAttack, 4 | attack ), force | use | 4 | attack );
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( movement | attack | force | use,
		movement | attack | altAttack, 0 ), force | use );
	// Losing ownership removes only our queued primary attack, not a legal
	// force/navigation owner's movement, alternate attack, force or use command.
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( movement | attack | altAttack | force | use,
		attack, 0 ), movement | altAttack | force | use );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDuelActionFlags( altAttack | attack | force, altAttack, 1 ), attack | force );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDuelActionFlags( altAttack | force, altAttack, 0 ), altAttack | force );
	BOOST_CHECK( NewBotAI_SaberCanOwnInputs( 1, 1, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanOwnInputs( 0, 1, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanOwnInputs( 1, 0, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanOwnInputs( 1, 1, 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanOwnInputs( 1, 1, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCanOwnInputs( 1, 1, 0, 0, 1 ) );
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	newbotai_saber_command_t command = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_HORIZONTAL, 0, 1, 1, 1, 1, 0 );
	NewBotAI_SaberSuppressStrafe( &command, 1 );
	BOOST_CHECK_EQUAL( command.right, 0 );
	BOOST_CHECK_EQUAL( command.attack, 1 );
	// If navigation or strafe suppression removes lateral input, do not silently
	// turn the horizontal request into an engine-selected vertical/diagonal.
	NewBotAI_SaberGuardSelection( &command, 1, 0, 1 );
	BOOST_CHECK_EQUAL( command.attack, 0 );
	command = { 0, -1, 0, 1 };
	NewBotAI_SaberGuardSelection( &command, 1, 0, -1 );
	BOOST_CHECK_EQUAL( command.forward, 0 );
	BOOST_CHECK_EQUAL( command.right, -1 );
	BOOST_CHECK_EQUAL( command.attack, 1 );
	command.forward = 1;
	NewBotAI_SaberGuardSelection( &command, 0, 0, -1 );
	BOOST_CHECK_EQUAL( command.forward, 1 );
}

BOOST_AUTO_TEST_CASE( saber_no_strafe_gate_keeps_deliberate_basic_fallback_without_changing_committed_start )
{
	for ( int family : { NEWBOTAI_SABER_BASIC, NEWBOTAI_SABER_HORIZONTAL, NEWBOTAI_SABER_DIAGONAL_VERTICAL } )
	{
		int forward, right;
		NewBotAI_SaberSelectionInputs((newbotai_saber_family_t)family, 0, -1, &forward, &right);
		NewBotAI_SaberNoStrafeFallback( 1, 0, &forward, &right );
		BOOST_CHECK_EQUAL( forward, 1 );
		BOOST_CHECK_EQUAL( right, 0 );
		newbotai_saber_command_t command = { forward, right, 0, 1 };
		NewBotAI_SaberGuardSelection( &command, 1, forward, right );
		BOOST_CHECK_EQUAL( command.attack, 1 );
	}
	int forward = 0, right = 1;
	NewBotAI_SaberNoStrafeFallback( 1, 1, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 0 );
	BOOST_CHECK_EQUAL( right, 1 );
	// The engine's committed start/transition takes precedence over a requested
	// family stage, without counting it as an accepted attack.
	BOOST_CHECK( NewBotAI_SaberPreparationInputs( 6, &forward, &right ) );
	BOOST_CHECK_EQUAL( forward, 1 );
	BOOST_CHECK_EQUAL( right, 0 );
	BOOST_CHECK( NewBotAI_SaberPreparationInputs( 1, &forward, &right ) );
	BOOST_CHECK_EQUAL( forward, 0 );
	BOOST_CHECK_EQUAL( right, 1 );
	BOOST_CHECK( NewBotAI_SaberPreparationInputs( 4, &forward, &right ) );
	BOOST_CHECK_EQUAL( forward, 0 );
	BOOST_CHECK_EQUAL( right, -1 );
	BOOST_CHECK( !NewBotAI_SaberPreparationInputs( 7, &forward, &right ) );
	BOOST_CHECK( !NewBotAI_SaberPreparationInputs( -1, &forward, &right ) );
	BOOST_CHECK( !NewBotAI_SaberMoveAccepted( 6, 5, 0 ) );
	BOOST_CHECK( NewBotAI_SaberOrdinaryAttackSafe( 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberOrdinaryAttackSafe( 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberOrdinaryAttackSafe( 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberOrdinaryAttackSafe( 0, 0, 1 ) );
}

BOOST_AUTO_TEST_CASE( saber_final_command_boundary_replaces_stale_pressure_through_engine_phases )
{
	newbotai_saber_final_context_t finalContext = { 1, 0, 1, 1, 1, 0, 1 };
	newbotai_saber_command_t queued = { 1, -1, 0, 1 };
	for ( int remaining : { 1500, 800, 99 } )
	{
		const newbotai_saber_yaw_phase_t phase = NewBotAI_SaberYawPhase( 0, 1, remaining, 1500 );
		finalContext.selecting = !NewBotAI_SaberCanApplyPressure( phase, 0, remaining );
		BOOST_REQUIRE( NewBotAI_SaberPreparationInputs( 1,
			&finalContext.selectedForward, &finalContext.selectedRight ) );
		const newbotai_saber_command_t command = NewBotAI_FinalizeSaberCommand( queued, finalContext );
		BOOST_CHECK_EQUAL( command.forward, 0 );
		BOOST_CHECK_EQUAL( command.right, 1 );
		BOOST_CHECK_EQUAL( command.attack, 1 );
		BOOST_CHECK( !NewBotAI_SaberMoveAccepted( 63, 1, 0 ) );
	}
	// The actual L2R acceptance, not its requested direction or start timer, advances us.
	BOOST_REQUIRE( NewBotAI_SaberMoveAccepted( 5, 63, 1 ) );
	const int actual = NewBotAI_SaberHorizontalDirection( 5, 5, 8 );
	const int next = NewBotAI_SaberNextHorizontalDirection( actual, 1 );
	finalContext.pressureRight = next;
	finalContext.selecting = !NewBotAI_SaberCanApplyPressure(
		NewBotAI_SaberYawPhase( 1, 0, 600, 1200 ), 1, 600 );
	newbotai_saber_command_t active = NewBotAI_FinalizeSaberCommand( queued, finalContext );
	BOOST_CHECK_EQUAL( active.forward, 1 );
	BOOST_CHECK_EQUAL( active.right, -1 );
	BOOST_CHECK_EQUAL( active.attack, 1 );
	float offset = NEWBOTAI_SABER_SWEEP_DEGREES;
	const float targetOffset = NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_ACTIVE,
		0.5f, actual, next, 1 );
	const float nextOffset = NewBotAI_SaberStepYawOffset( offset, targetOffset, 16 );
	BOOST_CHECK_LE( fabs(nextOffset - offset), NEWBOTAI_SABER_SWEEP_RATE * 0.016f + 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberApplyYawOffset( 355.0f, nextOffset ),
		NewBotAI_SaberApplyYawOffset( 350.0f, nextOffset ) + 5.0f, 0.001f );
	finalContext.selecting = !NewBotAI_SaberCanApplyPressure(
		NewBotAI_SaberYawPhase( 1, 0, 0, 1200 ), 1, 0 );
	NewBotAI_SaberSelectionInputs( NEWBOTAI_SABER_HORIZONTAL, 1, next,
		&finalContext.selectedForward, &finalContext.selectedRight );
	const newbotai_saber_command_t boundary = NewBotAI_FinalizeSaberCommand( active, finalContext );
	BOOST_CHECK_EQUAL( boundary.forward, 0 );
	BOOST_CHECK_EQUAL( boundary.right, -1 );
	BOOST_CHECK_EQUAL( boundary.attack, 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_NEXT, 1.0f, actual, next, 1 ),
		-NEWBOTAI_SABER_SWEEP_DEGREES );
	BOOST_REQUIRE( NewBotAI_SaberPreparationInputs( 4,
		&finalContext.selectedForward, &finalContext.selectedRight ) );
	const newbotai_saber_command_t transition = NewBotAI_FinalizeSaberCommand( active, finalContext );
	BOOST_CHECK_EQUAL( transition.forward, 0 );
	BOOST_CHECK_EQUAL( transition.right, -1 );
	BOOST_CHECK( !NewBotAI_SaberMoveAccepted( 80, 5, 0 ) );
	BOOST_CHECK_EQUAL( NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_RECOVER, 1.0f, actual, next, 1 ), 0.0f );
}

BOOST_AUTO_TEST_CASE( saber_final_command_boundary_preserves_mix_and_gated_fallback_but_not_unsafe_attacks )
{
	newbotai_saber_final_context_t finalContext = { 1, 0, 1, 1, -1, 0, 1 };
	for ( int family : { NEWBOTAI_SABER_BASIC, NEWBOTAI_SABER_DIAGONAL_VERTICAL } )
		for ( int stage = 0; stage < 3; stage++ )
		{
			NewBotAI_SaberSelectionInputs((newbotai_saber_family_t)family, stage, -1,
				&finalContext.selectedForward, &finalContext.selectedRight );
			const newbotai_saber_command_t command = NewBotAI_FinalizeSaberCommand(
				{ 1, 1, 0, 1 }, finalContext );
			BOOST_CHECK_EQUAL( command.forward, finalContext.selectedForward );
			BOOST_CHECK_EQUAL( command.right, finalContext.selectedRight );
			BOOST_CHECK_EQUAL( command.attack, 1 );
		}
	finalContext.selectedForward = 0;
	finalContext.selectedRight = 1;
	finalContext.strafeSuppressed = 1;
	NewBotAI_SaberNoStrafeFallback( 1, 0,
		&finalContext.selectedForward, &finalContext.selectedRight );
	const newbotai_saber_command_t fallback = NewBotAI_FinalizeSaberCommand( { 0, 1, 0, 1 }, finalContext );
	BOOST_CHECK_EQUAL( fallback.forward, 1 );
	BOOST_CHECK_EQUAL( fallback.right, 0 );
	BOOST_CHECK_EQUAL( fallback.attack, 1 );
	finalContext.attackSafe = 0; // jump/knockdown/duck/rising/DFA guard at the live boundary
	const newbotai_saber_command_t unsafe = NewBotAI_FinalizeSaberCommand( fallback, finalContext );
	BOOST_CHECK_EQUAL( unsafe.attack, 0 );
	finalContext.attackSafe = 1;
	const newbotai_saber_command_t vetoed = NewBotAI_FinalizeSaberCommand( { 0, 0, 0, 1 }, finalContext );
	BOOST_CHECK_EQUAL( vetoed.attack, 0 ); // a collision veto is not a fallback permission
	const int primary = 16, force = 64, use = 128, movement = 1 | 2 | 4 | 8;
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( primary | force | use | movement,
		primary | movement, unsafe.attack ? primary : 0 ), force | use );
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( primary | force | use | movement,
		primary, 0 ), force | use | movement );
}

BOOST_AUTO_TEST_CASE( saber_final_live_movement_veto_runs_after_pressure_refresh_and_preserves_safe_escape )
{
	newbotai_saber_final_context_t finalContext = { 0, 0, 1, 1, -1, 0, 1 };
	// The think-time lateral plan was safe; the final active phase adds forward
	// pressure and flips side. A live trace of that new command can veto it.
	newbotai_saber_command_t command = NewBotAI_FinalizeSaberCommand( { 0, 1, 0, 1 }, finalContext );
	BOOST_CHECK_EQUAL( command.forward, 1 );
	BOOST_CHECK_EQUAL( command.right, -1 );
	NewBotAI_SaberApplyMovementSafety( &command, 0 );
	BOOST_CHECK_EQUAL( command.forward, 0 );
	BOOST_CHECK_EQUAL( command.right, 0 );
	BOOST_CHECK_EQUAL( command.attack, 0 );
	BOOST_CHECK_EQUAL( command.jump, 0 );
	// No fallback attack may be selected from the now-stationary input.
	const int attack = 16, movement = 1 | 2 | 4 | 8, force = 64;
	BOOST_CHECK_EQUAL( NewBotAI_SaberOwnedActionFlags( attack | movement | force,
		attack | movement, command.attack ? attack : 0 ), force );
	for ( int side : { -1, 1 } )
	{
		command = { -1, side, 1, 0 };
		NewBotAI_SaberApplyMovementSafety( &command, 1 );
		BOOST_CHECK_EQUAL( command.forward, -1 );
		BOOST_CHECK_EQUAL( command.right, side );
		BOOST_CHECK_EQUAL( command.jump, 1 );
		NewBotAI_SaberApplyMovementSafety( &command, 0 );
		BOOST_CHECK_EQUAL( command.forward, 0 );
		BOOST_CHECK_EQUAL( command.right, 0 );
		BOOST_CHECK_EQUAL( command.jump, 0 );
		BOOST_CHECK_EQUAL( command.attack, 0 );
	}
	// Deliberate vertical fallback survives only a safe final path, never a veto.
	command = { 1, 0, 0, 1 };
	NewBotAI_SaberApplyMovementSafety( &command, 1 );
	BOOST_CHECK_EQUAL( command.attack, 1 );
	NewBotAI_SaberApplyMovementSafety( &command, 0 );
	BOOST_CHECK_EQUAL( command.attack, 0 );
}

BOOST_AUTO_TEST_CASE( saber_airborne_footwork_and_bounded_escape_have_deliberate_reentry )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	const newbotai_saber_command_t airborne = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_HORIZONTAL, 0, -1, 0, 1, 1, 0 );
	BOOST_CHECK_EQUAL( airborne.attack, 0 );
	BOOST_CHECK_EQUAL( airborne.forward, -1 );
	BOOST_CHECK_EQUAL( airborne.right, -1 );
	const int exitUntil = 1000 + NEWBOTAI_SABER_EXIT_MS;
	const int reentryUntil = exitUntil + NEWBOTAI_SABER_REENTRY_MS;
	BOOST_CHECK_EQUAL( NewBotAI_SaberEscapePhase( 1000, exitUntil, reentryUntil, 0, 0 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberEscapePhase( exitUntil, exitUntil, reentryUntil, 0, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberEscapePhase( reentryUntil, exitUntil, reentryUntil, 0, 0 ), 0 );
	const int airExitUntil = 1000 + NEWBOTAI_SABER_AIR_EXIT_MAX_MS;
	BOOST_CHECK_EQUAL( NewBotAI_SaberEscapePhase( exitUntil + 100, exitUntil, reentryUntil, 1, airExitUntil ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberEscapePhase( airExitUntil, exitUntil, airExitUntil + 650, 1, airExitUntil ), 1 );
	const newbotai_saber_command_t airborneExit = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_RESET, NEWBOTAI_SABER_BURST, 2, 1, 0, 1, 1,
		NewBotAI_SaberEscapePhase( exitUntil + 100, exitUntil, reentryUntil, 1, airExitUntil ) );
	BOOST_CHECK_EQUAL( airborneExit.forward, -1 );
	BOOST_CHECK_EQUAL( airborneExit.right, 1 );
	BOOST_CHECK_EQUAL( airborneExit.attack, 0 );
	context.enemyDistance = 220.0f;
	BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_RESET,
		NEWBOTAI_SABER_BURST, 2, 1, 0, 1, 1, -1 ).forward, -1 );
	context.enemyDistance = 80.0f;
	const newbotai_saber_command_t retreat = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_RESET, NEWBOTAI_SABER_BURST, 2, 1, 0, 1, 1, -1 );
	BOOST_CHECK_EQUAL( retreat.forward, -1 );
	BOOST_CHECK_EQUAL( retreat.right, 1 );
	BOOST_CHECK_EQUAL( retreat.attack, 0 );
	context.ourTotalHealth = 25; // Re-entry does not wait for healing.
	context.enemyDistance = 140.0f;
	const newbotai_saber_command_t reentry = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_RESET, NEWBOTAI_SABER_BURST, 0, 1, 1, 1, 1, 1 );
	BOOST_CHECK_EQUAL( reentry.forward, 1 );
	BOOST_CHECK_EQUAL( reentry.attack, 0 );
	context.enemyDistance = 80.0f;
	BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_RESET,
		NEWBOTAI_SABER_BASIC, 0, 1, 1, 1, 1, 1 ).attack, 1 );
	const newbotai_saber_command_t airborneReentry = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_RESET, NEWBOTAI_SABER_BASIC, 0, 1, 0, 1, 1, 1 );
	BOOST_CHECK_EQUAL( airborneReentry.forward, 1 );
	BOOST_CHECK_EQUAL( airborneReentry.attack, 0 );
	context.recentlyHurt = 1;
	// Saber-only: low health does not make re-entry unsafe.
	BOOST_CHECK( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_RESET,
		NEWBOTAI_SABER_BASIC, 0, 1, 1, 1, 1, 1 ).forward != -1 );
	context.saberOnlyDuel = 0;
	context.saberCombat = 1;
	const newbotai_saber_command_t unsafeReentry = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_RESET, NEWBOTAI_SABER_BASIC, 0, 1, 1, 1, 1, 1 );
	BOOST_CHECK_EQUAL( unsafeReentry.forward, -1 );
	BOOST_CHECK_EQUAL( unsafeReentry.attack, 0 );
	context.saberOnlyDuel = 1;
	context.saberCombat = 0;
	BOOST_CHECK( NewBotAI_SaberCanEscapeJump( 1, 1, 1, 1, 1, 1 ) );
	for ( int guard = 0; guard < 6; guard++ )
	{
		int ready[6] = { 1, 1, 1, 1, 1, 1 };
		ready[guard] = 0;
		BOOST_CHECK( !NewBotAI_SaberCanEscapeJump( ready[0], ready[1], ready[2],
			ready[3], ready[4], ready[5] ) );
	}
}

BOOST_AUTO_TEST_CASE( saber_grades_do_not_freeze_approach_or_override_necessary_escape )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 140.0f );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_STEP_IN,
		NEWBOTAI_SABER_GRADE_MEDIOCRE ), NEWBOTAI_SABER_TACTIC_STEP_IN );
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_ADVANCE,
		NEWBOTAI_SABER_GRADE_BAD ), NEWBOTAI_SABER_TACTIC_STEP_IN );
	context.enemyDistance = 80.0f;
	context.selfBlocked = 1;
	context.skill = 2;
	context.mistakeBias = 100;
	context.mistakeRoll = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );
	BOOST_CHECK_EQUAL( NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_ATTACK,
		NEWBOTAI_SABER_BASIC, 0, 1, 1, 1, 0, 0 ).attack, 0 );
}

BOOST_AUTO_TEST_CASE( general_saber_combat_shares_policy_and_respects_counter_reaction )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	context.saberOnlyDuel = 0;
	context.saberCombat = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_ATTACK );
	context.recentlyHurt = 1;
	context.enemyRecovering = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_REPOSITION );
	context.counterReady = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_COUNTER );
	context.enemyAttacking = 1;
	context.enemyRecovering = 0;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_REPOSITION );
	context.chainLength = 2;
	context.landedHit = 1;
	BOOST_CHECK_EQUAL( NewBotAI_SelectSaberTactic( context ), NEWBOTAI_SABER_TACTIC_RESET );
}

BOOST_AUTO_TEST_CASE( saber_throw_anticipation_excludes_impossible_duel_throws_and_basic_starts )
{
	BOOST_CHECK( NewBotAI_ShouldAnticipateSaberThrow( 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldAnticipateSaberThrow( 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldAnticipateSaberThrow( 1, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldAnticipateSaberThrow( 0, 0, 1 ) );
	// In-flight sabers bypass anticipation and still use the actual trajectory threat test.
	BOOST_CHECK( !NewBotAI_ShouldAnticipateSaberThrow( 1, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldAnticipateSaberThrow( 0, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_primary_legality_accepts_partial_staff_dual_holster )
{
	BOOST_CHECK( NewBotAI_SaberPrimaryBladeAvailable( 0 ) );
	BOOST_CHECK( NewBotAI_SaberPrimaryBladeAvailable( 1 ) );
	BOOST_CHECK( !NewBotAI_SaberPrimaryBladeAvailable( 2 ) );
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	const newbotai_saber_command_t partial = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_BASIC, 0, 1, 1, 1,
		NewBotAI_SaberPrimaryBladeAvailable( 1 ), 0 );
	const newbotai_saber_command_t full = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_BASIC, 0, 1, 1, 1,
		NewBotAI_SaberPrimaryBladeAvailable( 2 ), 0 );
	BOOST_CHECK_EQUAL( partial.attack, 1 );
	BOOST_CHECK_EQUAL( full.attack, 0 );
}

BOOST_AUTO_TEST_CASE( fan_dwell_links_immediately_at_high_skill )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetFanDwellMs( 250, 7.0f, 0.0f ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetFanDwellMs( 250, 10.0f, 50.0f ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetFanDwellMs( 250, 1.0f, 0.0f ), 250 );
	BOOST_CHECK( NewBotAI_GetFanDwellMs( 250, 4.0f, 0.0f ) < 250 );
	BOOST_CHECK( NewBotAI_GetFanDwellMs( 250, 4.0f, 100.0f ) < NewBotAI_GetFanDwellMs( 250, 4.0f, 0.0f ) );
}

BOOST_AUTO_TEST_CASE( fan_sweep_tracks_human_arc_and_crosses_target_mid_swing )
{
	const float highArc = NewBotAI_GetFanSweepHalfArc( 7.0f, 100.0f, 60.0f );
	const float lowArc = NewBotAI_GetFanSweepHalfArc( 2.0f, 0.0f, 60.0f );

	BOOST_CHECK( highArc >= 55.0f && highArc <= NEWBOTAI_FAN_SWEEP_MAX_HALF_ARC );
	BOOST_CHECK( lowArc < highArc );
	BOOST_CHECK( NewBotAI_GetFanSweepHalfArc( 7.0f, 100.0f, 200.0f ) < highArc );
	BOOST_CHECK( NewBotAI_GetFanSweepHalfArc( 10.0f, 100.0f, 0.0f ) <= NEWBOTAI_FAN_SWEEP_MAX_HALF_ARC );

	// L2R (+1) starts on the left (+yaw), crosses the target at 150ms and ends on the right.
	BOOST_CHECK_CLOSE( NewBotAI_GetFanSweepOffset( 60.0f, 1, 0, 300 ), 60.0f, 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_GetFanSweepOffset( 60.0f, 1, 150, 300 ), 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetFanSweepOffset( 60.0f, 1, 300, 300 ), -60.0f, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetFanSweepOffset( 60.0f, -1, 0, 300 ), -60.0f, 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_GetFanSweepOffset( 60.0f, 0, 0, 300 ), 0.01f );
}

BOOST_AUTO_TEST_CASE( fan_chain_runs_past_three_seconds_only_while_hitting )
{
	BOOST_CHECK( NewBotAI_FanChainMayContinue( 2500, -1 ) );
	BOOST_CHECK( !NewBotAI_FanChainMayContinue( 3500, -1 ) );
	BOOST_CHECK( !NewBotAI_FanChainMayContinue( 3500, 900 ) );
	BOOST_CHECK( NewBotAI_FanChainMayContinue( 3500, 300 ) );
	BOOST_CHECK( !NewBotAI_FanChainMayContinue( 6500, 100 ) );
}

BOOST_AUTO_TEST_CASE( saber_planner_sweep_scales_from_conservative_envelope )
{
	const float amp = NewBotAI_GetSaberSweepAmplitude( 8.0f, 50.0f );

	BOOST_CHECK( amp > NEWBOTAI_SABER_SWEEP_DEGREES );
	// Human envelope: ~60-70 degrees swept per swing at 200-450 deg/s.
	BOOST_CHECK( 2.0f * NewBotAI_GetSaberSweepAmplitude( 10.0f, 100.0f ) <= 70.0f );
	BOOST_CHECK( 2.0f * NewBotAI_GetSaberSweepAmplitude( 7.0f, 100.0f ) >= 60.0f );
	BOOST_CHECK( NewBotAI_GetSaberSweepAmplitude( 2.0f, 0.0f ) < amp );
	BOOST_CHECK( NewBotAI_GetSaberSweepRate( amp ) >= NEWBOTAI_SABER_SWEEP_MIN_RATE );
	BOOST_CHECK( NewBotAI_GetSaberSweepRate( amp ) <= NEWBOTAI_SABER_SWEEP_MAX_RATE );
	BOOST_CHECK_CLOSE( NewBotAI_GetSaberSweepRateScaled( 1.0f, 0.0f ), NEWBOTAI_SABER_SWEEP_MIN_RATE, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetSaberSweepRateScaled( 10.0f, 100.0f ), NEWBOTAI_SABER_SWEEP_MAX_RATE, 0.01f );
	// Each swing starts ~20 degrees off the target on its starting side...
	BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffsetScaled( NEWBOTAI_SABER_YAW_PREPARE, 0.0f, 1, 1, 1, amp ),
		NEWBOTAI_SABER_SWING_START_OFFSET, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffsetScaled( NEWBOTAI_SABER_YAW_PREPARE, 0.0f, 0, -1, 1, amp ),
		-NEWBOTAI_SABER_SWING_START_OFFSET, 0.01f );
	// ...and sweeps the full 2*amp arc across the target.
	BOOST_CHECK_CLOSE( NewBotAI_SaberYawOffsetScaled( NEWBOTAI_SABER_YAW_ACTIVE, 0.8f, 1, 1, 1, amp ),
		NEWBOTAI_SABER_SWING_START_OFFSET - 2.0f * amp, 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_SaberYawOffsetScaled( NEWBOTAI_SABER_YAW_PREPARE, 0.0f, 1, 1, 0, amp ), 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberStepYawOffsetScaled( 0.0f, 100.0f, 1000, amp, 1000.0f ), 2.0f * amp, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberStepYawOffsetScaled( 0.0f, 50.0f, 10, amp, 1000.0f ), 10.0f, 0.01f );
}

BOOST_AUTO_TEST_CASE( swing_footing_uses_predicted_peak_range )
{
	// 90u away closing at 300u/s -> 45u at the swing peak.
	const float predicted = NewBotAI_PredictRange2D( 90.0f, 0.0f, -300.0f, 0.0f, NEWBOTAI_SWING_PEAK_LEAD_MS );

	BOOST_CHECK_CLOSE( predicted, 45.0f, 0.01f );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 90.0f, predicted, 300.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_START );
	// 70-100u only starts while closing faster than ~150u/s; otherwise keep stepping in.
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 90.0f, 90.0f, 0.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_STEP_IN );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 110.0f, 90.0f, 200.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_START );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 68.0f, 68.0f, 0.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_START );
	// Beyond ~130u: bait/step in even when the fast close predicts reach.
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 140.0f, 60.0f, 500.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_STEP_IN );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 90.0f, 90.0f, -120.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_STEP_IN );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 120.0f, 120.0f, 0.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_STEP_IN );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 66.0f, 66.0f, 0.0f, 0, 1 ), NEWBOTAI_SWING_FOOTING_START );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 80.0f, 80.0f, 0.0f, 1, 0 ), NEWBOTAI_SWING_FOOTING_START );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingFooting( 110.0f, 50.0f, -120.0f, 0, 0 ), NEWBOTAI_SWING_FOOTING_HOLD );
}

BOOST_AUTO_TEST_CASE( saber_planner_fresh_swing_follows_start_window )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 90.0f );

	// Unknown closing speed keeps the old range-only behaviour.
	BOOST_CHECK( NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_STEP_IN ) );
	context.closingKnown = 1;
	context.currentDistance = 105.0f;
	context.closingSpeed = 40.0f;
	BOOST_CHECK( !NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_STEP_IN ) );
	BOOST_CHECK( !NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_ATTACK ) );
	// Chains and counters are not fresh starts.
	BOOST_CHECK( NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_CHAIN ) );
	BOOST_CHECK( NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_COUNTER ) );
	context.closingSpeed = 200.0f;
	BOOST_CHECK( NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_STEP_IN ) );
	context.currentDistance = 140.0f;
	BOOST_CHECK( !NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_STEP_IN ) );
	context.currentDistance = 80.0f;
	context.enemyDistance = 60.0f;
	context.closingSpeed = 0.0f;
	BOOST_CHECK( NewBotAI_SaberFreshSwingWindowAllows( context, NEWBOTAI_SABER_TACTIC_ATTACK ) );
}

BOOST_AUTO_TEST_CASE( fan_stances_alternate_horizontals_and_keep_t2b_for_finisher )
{
	for ( int family : { NEWBOTAI_SABER_BASIC, NEWBOTAI_SABER_DIAGONAL_VERTICAL, NEWBOTAI_SABER_HORIZONTAL } )
		for ( int stage = 0; stage < 6; stage++ )
		{
			int forward, right;
			NewBotAI_SaberSelectionInputs( (newbotai_saber_family_t)family, stage, -1, &forward, &right );
			NewBotAI_SaberFanStanceInputs( 1, (newbotai_saber_family_t)family, stage, -1, &forward, &right );
			// Never a T2B (forward only) outside the finisher.
			BOOST_CHECK( !(forward > 0 && right == 0) );
		}
	int forward = 0, right = -1;
	NewBotAI_SaberFanStanceInputs( 1, NEWBOTAI_SABER_FINISH, 0, -1, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 0 );
	BOOST_CHECK_EQUAL( right, -1 );
	NewBotAI_SaberFanStanceInputs( 1, NEWBOTAI_SABER_FINISH, NEWBOTAI_SABER_FAN_FINISH_STAGE, -1, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 1 );
	BOOST_CHECK_EQUAL( right, 0 );
	// Other stances are unchanged.
	forward = 1; right = 0;
	NewBotAI_SaberFanStanceInputs( 0, NEWBOTAI_SABER_BASIC, 1, -1, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 1 );
	BOOST_CHECK_EQUAL( right, 0 );
	// jundon: R2L opens ~54%, L2R->R2L links ~2/3 as often as R2L->L2R.
	BOOST_CHECK_EQUAL( NewBotAI_FanStartDirection( 54 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_FanStartDirection( 55 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_FanLinkChance( -1 ), 100 );
	BOOST_CHECK_EQUAL( NewBotAI_FanLinkChance( 1 ), 66 );
}

BOOST_AUTO_TEST_CASE( swing_end_sidestep_and_enemy_swing_reactions )
{
	// Unproductive, unchained swing return: sidestep back.
	BOOST_CHECK( NewBotAI_ShouldSidestepAfterSwing( 1, 0, -1, 0 ) );
	BOOST_CHECK( NewBotAI_ShouldSidestepAfterSwing( 1, 0, 900, 0 ) );
	// Landing hits, chaining, escaping or not in a return: keep the current footwork.
	BOOST_CHECK( !NewBotAI_ShouldSidestepAfterSwing( 1, 0, 300, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldSidestepAfterSwing( 1, 1, -1, 0 ) );
	BOOST_CHECK( !NewBotAI_ShouldSidestepAfterSwing( 1, 0, -1, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldSidestepAfterSwing( 0, 0, -1, 0 ) );

	int advances = 0;
	for ( int roll = 1; roll <= 100; roll++ )
		advances += NewBotAI_EnemySwingAdvances( roll );
	BOOST_CHECK_EQUAL( advances, NEWBOTAI_ENEMY_SWING_ADVANCE_PERCENT );

	BOOST_CHECK( !NewBotAI_CounterSwingReady( 50, 80.0f, 0 ) );
	BOOST_CHECK( NewBotAI_CounterSwingReady( 100, 80.0f, 0 ) );
	BOOST_CHECK( NewBotAI_CounterSwingReady( 300, 90.0f, 0 ) );
	BOOST_CHECK( !NewBotAI_CounterSwingReady( 400, 80.0f, 0 ) );
	BOOST_CHECK( !NewBotAI_CounterSwingReady( 120, 95.0f, 0 ) );
	BOOST_CHECK( !NewBotAI_CounterSwingReady( 120, 80.0f, 1 ) );
}

BOOST_AUTO_TEST_CASE( swing_dodge_prefers_lateral_or_jump_over_backpedal )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingDodgeChoice( 200.0f, 1, 1, 0, 50, 50 ), NEWBOTAI_SWING_DODGE_NONE );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingDodgeChoice( 100.0f, 1, 1, 0, 50, 50 ), NEWBOTAI_SWING_DODGE_LATERAL );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingDodgeChoice( 100.0f, 1, 1, 0, 50, 10 ), NEWBOTAI_SWING_DODGE_JUMP );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingDodgeChoice( 100.0f, 0, 1, 0, 50, 10 ), NEWBOTAI_SWING_DODGE_LATERAL );
	BOOST_CHECK_EQUAL( NewBotAI_GetSwingDodgeChoice( 100.0f, 1, 1, 30, 20, 50 ), NEWBOTAI_SWING_DODGE_BACKPEDAL );
	BOOST_CHECK( NewBotAI_AllowAirborneSwingStart( 1, 0, 50 ) );
	BOOST_CHECK( !NewBotAI_AllowAirborneSwingStart( 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_AllowAirborneSwingStart( 0, 30, 10 ) );
}

BOOST_AUTO_TEST_CASE( combo_gaps_match_jundon_at_high_skill )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_PULL_KICK, 7.0f, 90, 20 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_THROW_PULL, 8.0f, 0, 0 ), 66 );
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_GRIP_THROW, 10.0f, 0, 0 ), 100 );
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_DRAIN_FOLLOWUP, 7.0f, 0, 0 ), 66 );
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_PULL_KICK, 6.0f, 0, 0 ), 100 );
	BOOST_CHECK_EQUAL( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_PULL_KICK, 1.0f, 0, 0 ), 250 );
	BOOST_CHECK( NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_PULL_KICK, 3.0f, 90, 20 ) >
		NewBotAI_GetComboGapMs( NEWBOTAI_COMBO_PULL_KICK, 3.0f, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( reaction_bonuses_follow_observed_counters )
{
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_DRAIN, BOTLEARN_TOK_PULL ) >
		NewBotAI_GetReactionBonus( BOTLEARN_TOK_DRAIN, BOTLEARN_TOK_THROW ) );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_DRAIN, BOTLEARN_TOK_PUSH ) < 0 );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_GRIP, BOTLEARN_TOK_PUSH ) < 0 );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_THROW, BOTLEARN_TOK_THROW ) > 0 );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_THROW, BOTLEARN_TOK_JUMP ) < 0 );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_PULL, BOTLEARN_TOK_DRAIN ) < 0 );
	BOOST_CHECK( NewBotAI_GetReactionBonus( BOTLEARN_TOK_PUSH, BOTLEARN_TOK_DRAIN ) > 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetReactionBonus( BOTLEARN_TOK_IDLE, BOTLEARN_TOK_PULL ), 0 );
	BOOST_CHECK( NewBotAI_GetForceEconomyBonus( BOTLEARN_TOK_PULL, 65 ) > 0 );
	BOOST_CHECK( NewBotAI_GetForceEconomyBonus( BOTLEARN_TOK_DRAIN, 18 ) > 0 );
	BOOST_CHECK( NewBotAI_GetForceEconomyBonus( BOTLEARN_TOK_DRAIN, 80 ) < 0 );
}

BOOST_AUTO_TEST_CASE( saber_throw_situation_matches_human_hit_rates )
{
	BOOST_CHECK( NewBotAI_GetSaberThrowSituationBonus( 40, 200.0f, 0, 0, 0, 1 ) > 0 );
	BOOST_CHECK( NewBotAI_GetSaberThrowSituationBonus( 90, 200.0f, 1, 0, 0, 0 ) < 0 );
	BOOST_CHECK( NewBotAI_GetSaberThrowSituationBonus( 60, 200.0f, 1, 1, 0, 0 ) >= 0 );
	BOOST_CHECK( NewBotAI_GetSaberThrowSituationBonus( 60, 300.0f, 0, 0, 1, 0 ) < 0 );
	BOOST_CHECK( NewBotAI_GetSaberThrowSituationBonus( 90, 200.0f, 0, 0, 0, 0 ) <
		NewBotAI_GetSaberThrowSituationBonus( 40, 200.0f, 0, 0, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_range_buffer_holds_reach_decision )
{
	const float threshold = NEWBOTAI_SABER_STEP_IN_SWING_RANGE;
	const float band = NEWBOTAI_SABER_RANGE_HYSTERESIS;

	// Outside: the range is used as is.
	BOOST_CHECK_CLOSE( NewBotAI_SaberRangeWithHysteresis( threshold + 5.0f, threshold, band, 0 ),
		threshold + 5.0f, 0.001f );
	// Already inside: small drifts past the threshold stay inside.
	BOOST_CHECK_CLOSE( NewBotAI_SaberRangeWithHysteresis( threshold + 5.0f, threshold, band, 1 ),
		threshold, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberRangeWithHysteresis( threshold + band + 1.0f, threshold, band, 1 ),
		threshold + band + 1.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberRangeWithHysteresis( 60.0f, threshold, band, 1 ), 60.0f, 0.001f );
	BOOST_CHECK( NewBotAI_SaberTacticAllowsSwingStart( NEWBOTAI_SABER_TACTIC_STEP_IN,
		NewBotAI_SaberRangeWithHysteresis( threshold + 8.0f, threshold, band, 1 ) ) );
}

BOOST_AUTO_TEST_CASE( saber_grade_is_held_through_the_swing )
{
	BOOST_CHECK( NewBotAI_SaberGradeShouldReroll( 1000, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberGradeShouldReroll( 1300, 1000, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberGradeShouldReroll( 1550, 1000, 1 ) );
	BOOST_CHECK( NewBotAI_SaberGradeShouldReroll( 1550, 1000, 0 ) );
	BOOST_CHECK( NewBotAI_SaberGradeShouldReroll( 1000 + NEWBOTAI_SABER_GRADE_HOLD_MAX_MS, 1000, 1 ) );
}

BOOST_AUTO_TEST_CASE( saber_attacks_commit_forward_and_handover_keeps_intent )
{
	BOOST_CHECK_EQUAL( NewBotAI_SaberAttackForward( NEWBOTAI_SABER_TACTIC_ATTACK, 70.0f ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAttackForward( NEWBOTAI_SABER_TACTIC_ATTACK, 40.0f ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAttackForward( NEWBOTAI_SABER_TACTIC_STAND_SWING, 70.0f ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAttackForward( NEWBOTAI_SABER_TACTIC_STAND_SWING, 50.0f ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAttackForward( NEWBOTAI_SABER_TACTIC_BACK_SWING, 70.0f ), -1 );
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 95.0f );
	const newbotai_saber_command_t stepIn = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_STEP_IN, NEWBOTAI_SABER_HORIZONTAL, 1, 1, 1, 0, 1, 0 );
	BOOST_CHECK_EQUAL( stepIn.attack, 1 );
	BOOST_CHECK_EQUAL( stepIn.forward, 1 );

	BOOST_CHECK_EQUAL( NewBotAI_SaberHandoverForward( 1, -1, 100, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHandoverForward( 1, 0, 100, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHandoverForward( 1, -1, 100, 1 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHandoverForward( 1, -1, NEWBOTAI_SABER_HANDOVER_MS + 1, 0 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberHandoverForward( -1, 1, 100, 0 ), 1 );

	BOOST_CHECK_EQUAL( NewBotAI_SaberSwingStartForward( 8, 1, -1, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberSwingStartForward( 8, 1, -1, 1 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberSwingStartForward( 8, 0, -1, 0 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberSwingStartForward( 4, 1, -1, 0 ), -1 );
}

BOOST_AUTO_TEST_CASE( saber_advance_through_swing_phases )
{
	// Walk in through windup / apex / cooldown when out of reach.
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_WINDUP, 90.0f, 0, 0, 300, 0, 0, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_APEX, 80.0f, -1, 0, 200, 0, 0, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_COOLDOWN, 75.0f, 0, 0, 200, 0, 0, 0 ), 1 );
	// Already on top of the target: keep the planned input.
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_APEX, 44.0f, 0, 0, 200, 0, 0, 0 ), 0 );
	// Hand the stick back before the chained swing direction is read.
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_COOLDOWN, 90.0f, 0, 1, NEWBOTAI_SABER_ADVANCE_RELINK_MS, 0, 0, 0 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_COOLDOWN, 90.0f, 0, 1, NEWBOTAI_SABER_ADVANCE_RELINK_MS + 1, 0, 0, 0 ), 1 );
	// Swing-start frame and deliberate escapes are never overridden.
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_IDLE, 120.0f, 0, 0, 0, 1, 0, 0 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_WINDUP, 120.0f, -1, 0, 300, 0, 0, 1 ), -1 );
	// Between swings: close to swing range, but keep a dodge back from an incoming swing.
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_IDLE, 150.0f, 0, 0, 0, 0, 0, 0 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_IDLE, 150.0f, -1, 0, 0, 0, 1, 0 ), -1 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_IDLE, 60.0f, 0, 0, 0, 0, 0, 0 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberAdvanceForward( NEWBOTAI_SABER_PHASE_IDLE, NEWBOTAI_SABER_ADVANCE_MAX_RANGE + 1.0f, 0, 0, 0, 0, 0, 0 ), 0 );
}

BOOST_AUTO_TEST_CASE( fan_chain_drives_on_while_landing_hits )
{
	BOOST_CHECK_EQUAL( NewBotAI_GetFanLinkDwellMs( 200, 300 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_GetFanLinkDwellMs( 200, NEWBOTAI_FAN_CHAIN_HIT_WINDOW_MS + 1 ), 200 );
	BOOST_CHECK_EQUAL( NewBotAI_GetFanLinkDwellMs( 200, -1 ), 200 );
	BOOST_CHECK( NewBotAI_FanChainIsLanding( 0 ) );
	BOOST_CHECK( !NewBotAI_FanChainIsLanding( -1 ) );
}

BOOST_AUTO_TEST_CASE( approach_aim_is_held_off_centre )
{
	BOOST_CHECK_CLOSE( NewBotAI_GetApproachAimOffset( 1.0f, 150.0f, 1 ), NEWBOTAI_APPROACH_AIM_MIN_OFFSET, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetApproachAimOffset( 10.0f, 150.0f, -1 ), -NEWBOTAI_APPROACH_AIM_MAX_OFFSET, 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_GetApproachAimOffset( 10.0f, 40.0f, 1 ), 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_GetApproachAimOffset( 10.0f, 600.0f, 1 ), 0.01f );
}

BOOST_AUTO_TEST_CASE( saber_throw_is_held_through_first_pass )
{
	// Always held for the minimum hold.
	BOOST_CHECK( NewBotAI_SaberThrowHoldProtected( 300, 1, 0 ) );
	// Held past the minimum while still flying toward the target...
	BOOST_CHECK( NewBotAI_SaberThrowHoldProtected( 900, 0, 1 ) );
	// ...but not once it passed, not when heading elsewhere, and never past the cap.
	BOOST_CHECK( !NewBotAI_SaberThrowHoldProtected( 900, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowHoldProtected( 900, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowHoldProtected( NEWBOTAI_THROW_MAX_PASS_HOLD_MS, 0, 1 ) );
	// Early-release rules wait for the pass; drain-lock and lethal danger do not.
	BOOST_CHECK( !NewBotAI_SaberThrowMayRelease( 400, 0, 1, 0, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayRelease( 400, 0, 1, 1, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayRelease( 400, 0, 1, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayRelease( 800, 1, 0, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_trails_then_switches_side_after_pass )
{
	const float farTrail = NewBotAI_GetSaberThrowAimOffset( 1, 0, 500.0f );
	const float nearTrail = NewBotAI_GetSaberThrowAimOffset( 1, 0, 0.0f );

	// Trails behind the target's motion by 5-10 degrees...
	BOOST_CHECK_CLOSE( farTrail, -NEWBOTAI_THROW_TRAIL_MIN_DEG, 0.01f );
	BOOST_CHECK_CLOSE( nearTrail, -NEWBOTAI_THROW_TRAIL_MAX_DEG, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetSaberThrowAimOffset( -1, 0, 500.0f ), NEWBOTAI_THROW_TRAIL_MIN_DEG, 0.01f );
	// ...then switches ~9 degrees to the other side once the saber passed.
	BOOST_CHECK_CLOSE( NewBotAI_GetSaberThrowAimOffset( 1, 1, 200.0f ), NEWBOTAI_THROW_PASS_SWITCH_DEG, 0.01f );
	BOOST_CHECK_CLOSE( NewBotAI_GetSaberThrowAimOffset( -1, 1, 200.0f ), -NEWBOTAI_THROW_PASS_SWITCH_DEG, 0.01f );
	BOOST_CHECK_SMALL( NewBotAI_GetSaberThrowAimOffset( 0, 1, 200.0f ), 0.01f );
}

BOOST_AUTO_TEST_CASE( learning_score_is_net_damage_first_relative_to_source )
{
	// Humans win ~80% of tracked duels: a human row at that rate is neutral, not positive.
	BOOST_CHECK_SMALL( BotLearn_ScoreRelative( 50.0f, BotLearn_ExcessWins( 50.0f, 40.0f, 0.8f ), 0.0f ), 0.001f );
	BOOST_CHECK( BotLearn_ScoreRelative( 50.0f, BotLearn_ExcessWins( 50.0f, 40.0f, 0.8f ), 300.0f ) > 0.0f );
	// A bot row is down-weighted, so the same raw outcome moves the score less.
	const float w = BotLearn_SourceWeight( BOTLEARN_SOURCE_BOT );
	BOOST_CHECK( BotLearn_ScoreRelative( 20.0f * w, 0.0f, -200.0f * w ) >
		BotLearn_ScoreRelative( 20.0f, 0.0f, -200.0f ) );
}

BOOST_AUTO_TEST_CASE( high_skill_mistakes_are_small_and_vanish_at_ten )
{
	BOOST_CHECK( NewBotAI_GetSaberTacticMistakeChance( 7, 30 ) <= 3 );
	BOOST_CHECK( NewBotAI_GetSaberTacticMistakeChance( 7, 100 ) <= 10 );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberTacticMistakeChance( 10, 100 ), 0 );
	BOOST_CHECK( NewBotAI_GetSaberTacticMistakeChance( 5, 30 ) > NewBotAI_GetSaberTacticMistakeChance( 7, 30 ) );
}

BOOST_AUTO_TEST_CASE( broken_parry_reentry_waits_for_engine_and_enemy_followup )
{
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1200, 1250, 1180, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1250, 1250, 1180, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1250, 1250, 1180, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1300, 1250, 1380, 0, 0 ) );
	BOOST_CHECK( NewBotAI_SaberDefenseReady( 1380, 1250, 1380, 0, 0 ) );
	// Ordinary parry/bounce does not create a broken-parry recovery deadline.
	BOOST_CHECK( NewBotAI_SaberDefenseReady( 1000, 0, 0, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_defense_categories_do_not_conflate_bounce_or_normal_recovery )
{
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseCategory( 0, 0, 0 ), NEWBOTAI_SABER_DEFENSE_NONE );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseCategory( 0, 0, 1 ), NEWBOTAI_SABER_DEFENSE_PARRY );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseCategory( 0, 1, 1 ), NEWBOTAI_SABER_DEFENSE_BOUNCE );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseCategory( 1, 1, 1 ), NEWBOTAI_SABER_DEFENSE_BROKEN );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseState( 1, 1, 1, 1, 0 ), NEWBOTAI_SABER_DEFENSE_KNOCKDOWN );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseState( 1, 1, 1, 1, 1 ), NEWBOTAI_SABER_DEFENSE_LOST );
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseState( 0, 0, 0, 0, 1 ), NEWBOTAI_SABER_DEFENSE_LOST );
	// A normal saber return is tracked separately; it is not a defensive block.
	BOOST_CHECK_EQUAL( NewBotAI_SaberDefenseCategory( 0, 0, 0 ), NEWBOTAI_SABER_DEFENSE_NONE );
}

BOOST_AUTO_TEST_CASE( broken_defense_followup_ends_after_leaving_reach_not_distant_swings )
{
	BOOST_CHECK( NewBotAI_SaberDefenseFollowupThreat( 1, 90.0f, 200.0f ) );
	BOOST_CHECK( NewBotAI_SaberDefenseFollowupThreat( 1, 240.0f, 100.0f ) );
	BOOST_CHECK( NewBotAI_SaberDefenseFollowupThreat( 1, 150.0f, 200.0f ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseFollowupThreat( 1, 240.0f, 200.0f ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseFollowupThreat( 0, 90.0f, 50.0f ) );
	int followupUntil = 1180;
	for (int now = 1000; now <= 1500; now += 50)
	{
		if (NewBotAI_SaberDefenseFollowupThreat( 1, 400.0f, 400.0f ))
			followupUntil = now + NEWBOTAI_SABER_FOLLOWUP_CLEAR_MS;
	}
	BOOST_CHECK( NewBotAI_SaberDefenseReady( 1500, 1250, followupUntil, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1500, 1250, followupUntil, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberDefenseReady( 1500, 1250, followupUntil, 0, 100 ) );
}

BOOST_AUTO_TEST_CASE( saber_footing_world_direction_survives_final_aim_rotation )
{
	float x, y;
	NewBotAI_SaberWorldDirection( 0.0f, -1, 1, &x, &y );
	BOOST_CHECK_CLOSE( x, -sqrtf( 0.5f ), 0.001f );
	BOOST_CHECK_CLOSE( y, -sqrtf( 0.5f ), 0.001f );
	// With the view turned 90 degrees, the same world lane becomes back-left.
	float forward, right;
	NewBotAI_ProjectWorldMovement( x, y, 90.0f, &forward, &right );
	BOOST_CHECK_CLOSE( forward, -sqrtf( 0.5f ), 0.001f );
	BOOST_CHECK_CLOSE( right, -sqrtf( 0.5f ), 0.001f );
	float oppositeX, oppositeY;
	NewBotAI_SaberWorldDirection( 90.0f, -1, -1, &oppositeX, &oppositeY );
	BOOST_CHECK_CLOSE( oppositeX, x, 0.001f );
	BOOST_CHECK_CLOSE( oppositeY, y, 0.001f );
	// Projection retains analog proportions, not just the signs of movement keys.
	NewBotAI_ProjectWorldMovement( 1.0f, 0.0f, 20.0f, &forward, &right );
	BOOST_CHECK_CLOSE( forward, cosf( 20.0f * 0.017453292519943295f ), 0.001f );
	BOOST_CHECK_CLOSE( right, sinf( 20.0f * 0.017453292519943295f ), 0.001f );
	BOOST_CHECK_LT( right, forward * 0.5f );
	NewBotAI_SaberWorldDirection( 359.0f, 0, 0, &x, &y );
	BOOST_CHECK_SMALL( x, 0.001f );
	BOOST_CHECK_SMALL( y, 0.001f );
}

BOOST_AUTO_TEST_CASE( saber_footing_is_held_until_a_phase_or_safety_boundary )
{
	BOOST_CHECK( !NewBotAI_SaberFootingNeedsUpdate( 1, 1, 1, 10, 10, 2, 2 ) );
	BOOST_CHECK( NewBotAI_SaberFootingNeedsUpdate( 1, 1, 2, 10, 10, 2, 2 ) );
	BOOST_CHECK( NewBotAI_SaberFootingNeedsUpdate( 1, 1, 1, 10, 11, 2, 2 ) );
	BOOST_CHECK( NewBotAI_SaberFootingNeedsUpdate( 1, 1, 1, 10, 10, 2, 3 ) );
	BOOST_CHECK( NewBotAI_SaberFootingNeedsUpdate( 0, 1, 1, 10, 10, 2, 2 ) );
}

BOOST_AUTO_TEST_CASE( saber_learning_records_only_committed_accepted_attacks )
{
	BOOST_CHECK( NewBotAI_SaberCommittedLearningDecision( 1, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedLearningDecision( 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedLearningDecision( 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedLearningDecision( 1, 1, 1 ) );
}

BOOST_AUTO_TEST_CASE( saber_poke_counteryaw_is_phase_aware_and_bounded )
{
	BOOST_CHECK_SMALL( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_PREPARE, 0.475f, 1 ), 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_RECOVER, 0.475f, 1 ), 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_NEXT, 0.9f, 1 ), 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f, 1 ), 6.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f, -1 ), -6.0f, 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f, 0 ), 0.001f );
	for (int direction : { -1, 1 })
	{
		BOOST_CHECK_LT(
			NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE, 0.7f, direction ) *
			NewBotAI_SaberYawOffset( NEWBOTAI_SABER_YAW_ACTIVE, 0.7f, direction, direction, 1 ), 0.0f );
	}
	BOOST_CHECK_SMALL( NewBotAI_SaberPokeCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE, 1.0f, 1 ), 0.001f );
}

BOOST_AUTO_TEST_CASE( saber_active_wiggle_obeys_delay_axes_and_caps )
{
	float yaw, pitch;
	for (int phase = NEWBOTAI_SABER_YAW_PREPARE; phase <= NEWBOTAI_SABER_YAW_NEXT; ++phase)
	{
		if (phase == NEWBOTAI_SABER_YAW_ACTIVE) continue;
		NewBotAI_GetSaberActiveWiggle( (newbotai_saber_yaw_phase_t)phase, 500, 0, 45.0f, 20.0f, 20.0f, &yaw, &pitch );
		BOOST_CHECK_SMALL( yaw, 0.001f );
		BOOST_CHECK_SMALL( pitch, 0.001f );
	}
	NewBotAI_GetSaberActiveWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 119, 120, 45.0f, 20.0f, 2.0f, &yaw, &pitch );
	BOOST_CHECK_SMALL( yaw, 0.001f );
	BOOST_CHECK_SMALL( pitch, 0.001f );
	for (int elapsed = 120; elapsed <= 1000; elapsed += 17)
	{
		NewBotAI_GetSaberActiveWiggle( NEWBOTAI_SABER_YAW_ACTIVE, elapsed, 120, 45.0f, 20.0f, 25.0f, &yaw, &pitch );
		BOOST_CHECK_LE( fabsf( yaw ), 8.0f );
		BOOST_CHECK_LE( fabsf( pitch ), 4.0f );
	}
	NewBotAI_GetSaberActiveWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 200, 0, 8.0f, 4.0f, 0.0f, &yaw, &pitch );
	BOOST_CHECK_SMALL( yaw, 0.001f );
	BOOST_CHECK_SMALL( pitch, 0.001f );
	NewBotAI_GetSaberActiveWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 200, 0, -8.0f, -4.0f, 2.0f, &yaw, &pitch );
	BOOST_CHECK_SMALL( yaw, 0.001f );
	BOOST_CHECK_SMALL( pitch, 0.001f );
}

BOOST_AUTO_TEST_CASE( saber_catalog_has_twelve_distinct_ordinary_grounded_recipes )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	int signatures[BOTLEARN_SABER_SEQUENCE_COUNT] = {};
	for (int sequence = 0; sequence < BOTLEARN_SABER_SEQUENCE_COUNT; ++sequence)
	{
		const auto family = (newbotai_saber_family_t)(NEWBOTAI_SABER_SEQUENCE_FIRST + sequence);
		BOOST_CHECK_EQUAL( NewBotAI_SaberSequenceId( family ), sequence );
		for (int stage = 0; stage < BOTLEARN_SABER_SEQUENCE_LENGTH; ++stage)
		{
			int forward = 0, right = 0, expectedForward = 0, expectedRight = 0;
			const int attack = BotLearn_SaberSequenceAttack( sequence, stage );
			signatures[sequence] = signatures[sequence] * 7 + attack;
			BOOST_REQUIRE( NewBotAI_SaberPreparationInputs( attack, &expectedForward, &expectedRight ) );
			NewBotAI_SaberSelectionInputs( family, stage, -1, &forward, &right );
			NewBotAI_SaberFanStanceInputs( 1, family, stage, -1, &forward, &right );
			BOOST_CHECK_EQUAL( forward, expectedForward );
			BOOST_CHECK_EQUAL( right, expectedRight );
			BOOST_CHECK_GE( forward, 0 ); // Never selects a backward stab or aerial special.
			auto command = NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_ATTACK,
				family, stage, -1, 1, 1, 1, 0 );
			BOOST_CHECK_EQUAL( command.attack, 1 );
			BOOST_CHECK_EQUAL( command.jump, 0 );
			BOOST_CHECK_EQUAL( command.forward, forward );
			BOOST_CHECK_EQUAL( command.right, right );
			NewBotAI_SaberApplyMovementSafety( &command, 0 );
			BOOST_CHECK_EQUAL( command.attack, 0 );
			command = NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_RESET,
				family, stage, -1, 1, 1, 1, -1 );
			BOOST_CHECK_EQUAL( command.attack, 0 );
			command = NewBotAI_PlanSaberCommand( context, NEWBOTAI_SABER_TACTIC_ATTACK,
				family, stage, -1, 0, 1, 1, 0 );
			BOOST_CHECK_EQUAL( command.attack, 0 );
		}
		for (int previous = 0; previous < sequence; ++previous)
			BOOST_CHECK_NE( signatures[sequence], signatures[previous] );
		BOOST_CHECK_EQUAL( NewBotAI_SaberSequenceAllowed( context, sequence ), sequence < 2 );
	}
	context.enemyRecovering = 1;
	for (int sequence = 0; sequence < BOTLEARN_SABER_SEQUENCE_COUNT; ++sequence)
		BOOST_CHECK( NewBotAI_SaberSequenceAllowed( context, sequence ) );
	context.enemyAttacking = 1;
	for (int sequence = 2; sequence < BOTLEARN_SABER_SEQUENCE_COUNT; ++sequence)
		BOOST_CHECK( !NewBotAI_SaberSequenceAllowed( context, sequence ) );
	BOOST_CHECK( NewBotAI_SaberSequenceBoundary( 0 ) );
	BOOST_CHECK( !NewBotAI_SaberSequenceBoundary( 1 ) );
	BOOST_CHECK( !NewBotAI_SaberSequenceBoundary( 2 ) );
	BOOST_CHECK( NewBotAI_SaberSequenceBoundary( 3 ) );
	BOOST_CHECK_EQUAL( NewBotAI_SaberSequenceId( NEWBOTAI_SABER_BURST ), -1 );
}

BOOST_AUTO_TEST_CASE( saber_contact_counteryaw_scales_skill_and_actual_contact_range )
{
	BOOST_CHECK_CLOSE( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE,
		0.475f, 1, 40.0f, 10.0f ), 6.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE,
		0.475f, -1, 40.0f, 1.0f ), -1.5f, 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_RECOVER,
		0.475f, 1, 40.0f, 10.0f ), 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE,
		0.475f, 0, 40.0f, 10.0f ), 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE,
		0.475f, 1, NEWBOTAI_SABER_LONG_SWING_RANGE, 10.0f ), 0.001f );
	BOOST_CHECK_SMALL( NewBotAI_SaberContactCounterYaw( NEWBOTAI_SABER_YAW_ACTIVE,
		0.475f, 1, -1.0f, 10.0f ), 0.001f );
}

BOOST_AUTO_TEST_CASE( saber_animation_wiggle_fades_at_active_and_delay_boundaries )
{
	float yaw = 0, pitch = 0;
	for (float progress : { 0.0f, 0.15f, 0.8f, 1.0f })
	{
		NewBotAI_GetSaberAnimationWiggle( NEWBOTAI_SABER_YAW_ACTIVE, progress,
			1, 250, 120, 80.0f, 40.0f, 2.0f, &yaw, &pitch );
		BOOST_CHECK_SMALL( yaw, 0.001f );
		BOOST_CHECK_SMALL( pitch, 0.001f );
	}
	for (int phase : { NEWBOTAI_SABER_YAW_PREPARE, NEWBOTAI_SABER_YAW_RECOVER, NEWBOTAI_SABER_YAW_NEXT })
	{
		NewBotAI_GetSaberAnimationWiggle( (newbotai_saber_yaw_phase_t)phase, 0.475f,
			1, 250, 120, 80.0f, 40.0f, 2.0f, &yaw, &pitch );
		BOOST_CHECK_SMALL( yaw, 0.001f );
		BOOST_CHECK_SMALL( pitch, 0.001f );
	}
	for (int elapsed = 0; elapsed < 1000; ++elapsed)
	{
		NewBotAI_GetSaberAnimationWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f,
			1, elapsed, 120, 80.0f, 40.0f, 2.0f, &yaw, &pitch );
		BOOST_CHECK_LE( fabsf( yaw ), 8.0f );
		BOOST_CHECK_LE( fabsf( pitch ), 2.0f );
		if (elapsed <= 120)
		{
			BOOST_CHECK_SMALL( yaw, 0.001f );
			BOOST_CHECK_SMALL( pitch, 0.001f );
		}
	}
	NewBotAI_GetSaberAnimationWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f,
		6, 245, 120, 8.0f, 4.0f, 2.0f, &yaw, &pitch );
	BOOST_CHECK_CLOSE( yaw, 2.8f, 0.001f );
	BOOST_CHECK_CLOSE( pitch, -4.0f, 0.001f );
	NewBotAI_GetSaberAnimationWiggle( NEWBOTAI_SABER_YAW_ACTIVE, 0.475f,
		7, 245, 120, 8.0f, 4.0f, 2.0f, &yaw, &pitch );
	BOOST_CHECK_SMALL( yaw, 0.001f );
	BOOST_CHECK_SMALL( pitch, 0.001f );
}

BOOST_AUTO_TEST_CASE( saber_throw_phase_uses_target_facing_and_engine_cadence )
{
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowSteerCadence( 1 ), 0 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowSteerCadence( 2 ), 400 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowSteerCadence( 3 ), 100 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_LAUNCH, 399, 400, 100.0f ), NEWBOTAI_THROW_LAUNCH );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_LAUNCH, 400, 400, 100.0f ), NEWBOTAI_THROW_BYPASS );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_BYPASS, 500, 100, 24.0f ), NEWBOTAI_THROW_BYPASS );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_BYPASS, 500, 100, 0.0f ), NEWBOTAI_THROW_REAR );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_REAR, 500, 100, -49.0f ), NEWBOTAI_THROW_CUT_THROUGH );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_RECALL, 500, 100, -100.0f ), NEWBOTAI_THROW_RECALL );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowNextPhase( NEWBOTAI_THROW_LAUNCH, 500, 0, -100.0f ), NEWBOTAI_THROW_LAUNCH );
}

BOOST_AUTO_TEST_CASE( saber_throw_bypasses_ready_defense_but_intercepts_exposed_targets )
{
	BOOST_CHECK( NewBotAI_SaberThrowTargetGuarded( 1, 1, 0, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 0, 1, 0, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 1, 0, 0, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 1, 1, 1, 0, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 1, 1, 0, 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 1, 1, 0, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTargetGuarded( 1, 1, 0, 0, 0, 1 ) );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_LAUNCH, 1, 1 ), NEWBOTAI_THROW_BYPASS );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_LAUNCH, 0, 1 ), NEWBOTAI_THROW_CUT_THROUGH );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_REAR, 0, 1 ), NEWBOTAI_THROW_CUT_THROUGH );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_REAR, 1, 1 ), NEWBOTAI_THROW_REAR );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_RECALL, 0, 1 ), NEWBOTAI_THROW_RECALL );
	BOOST_CHECK_EQUAL( NewBotAI_SaberThrowTargetPhase( NEWBOTAI_THROW_LAUNCH, 1, 0 ), NEWBOTAI_THROW_LAUNCH );
}

BOOST_AUTO_TEST_CASE( saber_throw_overhead_fallback_preserves_rear_placement_and_cut_through )
{
	float rear, side, height;
	for (int lane : { -1, 1 })
	{
		NewBotAI_SaberThrowLaneOffsets( NEWBOTAI_THROW_BYPASS, lane, 0, &rear, &side, &height );
		BOOST_CHECK_EQUAL( rear, -48.0f );
		BOOST_CHECK_EQUAL( side, lane * 112.0f );
		BOOST_CHECK_SMALL( height, 0.001f );
		NewBotAI_SaberThrowLaneOffsets( NEWBOTAI_THROW_BYPASS, lane, 1, &rear, &side, &height );
		BOOST_CHECK_EQUAL( rear, -48.0f );
		BOOST_CHECK_EQUAL( side, lane * 32.0f );
		BOOST_CHECK_EQUAL( height, 128.0f );
		NewBotAI_SaberThrowLaneOffsets( NEWBOTAI_THROW_REAR, lane, 1, &rear, &side, &height );
		BOOST_CHECK_EQUAL( rear, -128.0f );
		BOOST_CHECK_EQUAL( side, lane * 32.0f );
		BOOST_CHECK_EQUAL( height, 96.0f );
		// Once behind, redirect through the opponent instead of steering farther away.
		NewBotAI_SaberThrowLaneOffsets( NEWBOTAI_THROW_CUT_THROUGH, lane, 1, &rear, &side, &height );
		BOOST_CHECK_SMALL( rear, 0.001f );
		BOOST_CHECK_SMALL( side, 0.001f );
		BOOST_CHECK_SMALL( height, 0.001f );
	}
	// Overhead candidates still obey the same solid/obstruction checks as side lanes.
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 1, 0, 0, 0, NEWBOTAI_THROW_BYPASS ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 0, NEWBOTAI_THROW_REAR ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_routing_hold_survives_lateral_motion_but_is_bounded )
{
	// A planned detour can point away from the target after the legacy first-pass hold.
	BOOST_CHECK( !NewBotAI_SaberThrowHoldProtected( 800, 1, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRoutingHold( 2, NEWBOTAI_THROW_BYPASS, 800 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_REAR, 800 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 1, NEWBOTAI_THROW_BYPASS, 600 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 2, NEWBOTAI_THROW_BYPASS, 1800 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_REAR, 1500 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_REAR, -1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_LAUNCH, 800 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 800 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRoutingHold( 3, NEWBOTAI_THROW_RECALL, 800 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_traces_reject_obstructions_and_front_bypass )
{
	BOOST_CHECK( NewBotAI_SaberThrowTraceSafe( 0, 0, 0, 0, NEWBOTAI_THROW_BYPASS ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 1, 0, 0, 0, NEWBOTAI_THROW_BYPASS ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 1, 0, 0, NEWBOTAI_THROW_REAR ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 0, NEWBOTAI_THROW_CUT_THROUGH ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 1, NEWBOTAI_THROW_BYPASS ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 1, NEWBOTAI_THROW_REAR ) );
	BOOST_CHECK( NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 1, NEWBOTAI_THROW_CUT_THROUGH ) );
	BOOST_CHECK( NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 1, NEWBOTAI_THROW_LAUNCH ) );
	BOOST_CHECK( !NewBotAI_SaberThrowTraceSafe( 0, 0, 0, 0, NEWBOTAI_THROW_RECALL ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_recall_request_is_level_bounded_and_safety_sensitive )
{
	BOOST_CHECK( !NewBotAI_SaberThrowRecallDue( 1, 749, 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 1, 750, 0, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRecallDue( 2, 1799, 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 2, 1800, 0, 0, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowRecallDue( 3, 1499, 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 3, 1500, 0, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 3, 50, 1, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 3, 50, 0, 1, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowRecallDue( 3, 50, 0, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_final_hold_keeps_minimum_and_drainlock_polarity )
{
	BOOST_CHECK( NewBotAI_SaberThrowFinalHoldProtected( 50, 1, 0, 0, 0, 0, 1, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowFinalHoldProtected( 599, 1, 0, 0, 0, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 50, 0, 1, 1, 0, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 50, 0, 1, 0, 1, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 50, 0, 1, 0, 0, 1, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 50, 0, 1, 0, 0, 0, 0, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowFinalHoldProtected( 600, 0, 1, 0, 0, 0, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 600, 1, 1, 0, 0, 0, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_hard_recall_overrides_protected_hold )
{
	const int levelOneDeadline = NewBotAI_SaberThrowRecallDue( 1, 800, 0, 0, 1 );
	BOOST_CHECK( NewBotAI_SaberThrowHoldProtected( 800, 0, 1 ) );
	BOOST_CHECK( levelOneDeadline );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 800, 0, 1, 0, 0, 0, 1, levelOneDeadline ) );
	// The minimum hold still protects policy recalls, never an unsafe traced route.
	BOOST_CHECK( NewBotAI_SaberThrowHoldProtected( 550, 0, 1 ) );
	const int unsafeRoute = !NewBotAI_SaberThrowTraceSafe( 0, 0, 1, 0, NEWBOTAI_THROW_BYPASS );
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 550, 0, 1, 0, 0, 0, 1, unsafeRoute ) );
	// Lost/invalid targets are hard recalls even while the old heading is positive.
	BOOST_CHECK( !NewBotAI_SaberThrowFinalHoldProtected( 550, 0, 1, 0, 0, 0, 1, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowFinalHoldProtected( 550, 0, 1, 0, 0, 0, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_skill_upgrade_preserves_restricted_and_low_skill_loadouts )
{
	for (int configured = 0; configured <= 3; ++configured)
	{
		BOOST_CHECK_EQUAL( NewBotAI_SaberThrowConfiguredLevel( 1.0f, configured, 0, 1 ), configured );
		BOOST_CHECK_EQUAL( NewBotAI_SaberThrowConfiguredLevel( 2.0f, configured, 0, 1 ), configured );
		BOOST_CHECK_EQUAL( NewBotAI_SaberThrowConfiguredLevel( 2.1f, configured, 0, 1 ), 3 );
		BOOST_CHECK_EQUAL( NewBotAI_SaberThrowConfiguredLevel( 10.0f, configured, 1, 1 ), configured );
		BOOST_CHECK_EQUAL( NewBotAI_SaberThrowConfiguredLevel( 10.0f, configured, 0, 0 ), configured );
	}
}

BOOST_AUTO_TEST_CASE( saber_throw_shared_policy_keeps_soft_recall_out_of_protected_routes )
{
	BOOST_CHECK( NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 599, 1, 0, 0, 1, -30, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 1000, 0, 1, 0, 1, 0, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_BYPASS, 1000, 1, 0, 0, 1, -30, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowWantsHold( 2, NEWBOTAI_THROW_REAR, 1799, 1, 0, 0, 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 2, NEWBOTAI_THROW_REAR, 1800, 1, 0, 0, 0, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_BYPASS, 1500, 0, 1, 0, 0, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 1, NEWBOTAI_THROW_LAUNCH, 750, 0, 1, 0, 0, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_RECALL, 50, 0, 1, 0, 0, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_BYPASS, -1, 0, 1, 0, 0, 30, 1 ) );
	// Drainlock, lethal danger, disallowed force, return and route obstruction
	// all arrive as safetyRecall and override both base and learned holds.
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_BYPASS, 50, 0, 1, 1, 0, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_REAR, 800, 1, 0, 1, 0, 30, 1 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_learned_preference_is_context_gated_and_not_duration_learning )
{
	BOOST_CHECK( !NewBotAI_SaberThrowLearnedHold( 599, 30, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowLearnedHold( 600, 8, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowLearnedHold( 899, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowLearnedHold( 900, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowLearnedHold( 800, 7, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowLearnedHold( 800, -30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowLearnedHold( 800, 30, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 800, 1, 0, 0, 1, 0, 1 ) );
	BOOST_CHECK( NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 800, 1, 0, 0, 1, 8, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 900, 1, 0, 0, 1, 30, 1 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowWantsHold( 3, NEWBOTAI_THROW_CUT_THROUGH, 800, 1, 0, 0, 1, 30, 0 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_detours_do_not_count_as_target_passes )
{
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_LAUNCH, 149, 149, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_LAUNCH, 150, 150, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_BYPASS, 800, 400, 100 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_REAR, 800, 400, 100 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_RECALL, 800, 400, 100 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_CUT_THROUGH, 800, 99, 100 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_CUT_THROUGH, 800, 100, 100 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_CUT_THROUGH, 800, 399, 400 ) );
	BOOST_CHECK( NewBotAI_SaberThrowMayMarkPass( NEWBOTAI_THROW_CUT_THROUGH, 800, 400, 400 ) );
}

BOOST_AUTO_TEST_CASE( saber_throw_hold_telemetry_records_transitions_and_bounded_samples )
{
	BOOST_CHECK( NewBotAI_SaberThrowDecisionSampleDue( 0, 1, 0 ) );
	BOOST_CHECK( NewBotAI_SaberThrowDecisionSampleDue( 1, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowDecisionSampleDue( 1, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberThrowDecisionSampleDue( 1, 1, 249 ) );
	BOOST_CHECK( NewBotAI_SaberThrowDecisionSampleDue( 1, 1, 250 ) );
	BOOST_CHECK( NewBotAI_SaberThrowDecisionSampleDue( 1, 1, -1 ) );
}

BOOST_AUTO_TEST_CASE( strafejump_glancing_walls_slide_instead_of_rejecting )
{
	// Wall facing -X; moving mostly along +Y with a slight push into it: a graze.
	BOOST_CHECK( BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, 100.0f, 400.0f ) );
	// Moving away from or parallel to the wall is fine too.
	BOOST_CHECK( BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, -50.0f, 400.0f ) );
	BOOST_CHECK( BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, 0.0f, 400.0f ) );
	// Head-on and steep-angle impacts still reject the arc.
	BOOST_CHECK( !BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, 400.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, 300.0f, 300.0f ) );
	// Floors, ceilings and a standing start are never treated as a graze.
	BOOST_CHECK( !BotSFJ_IsGlancingWall( 0.0f, 0.0f, 1.0f, 0.0f, 400.0f ) );
	BOOST_CHECK( !BotSFJ_IsGlancingWall( 0.0f, 0.3f, -0.95f, 0.0f, 400.0f ) );
	BOOST_CHECK( !BotSFJ_IsGlancingWall( -1.0f, 0.0f, 0.0f, 0.0f, 0.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_wall_contact_keeps_strafe_above_ground_speed )
{
	BOOST_CHECK( BotSFJ_WallContactKeepsStrafe( 300.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_WallContactKeepsStrafe( 250.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_WallContactKeepsStrafe( 200.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_WallContactKeepsStrafe( 400.0f, 0.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_slowed_out_releases_at_ground_speed )
{
	BOOST_CHECK( BotSFJ_SlowedOut( 420.0f, 250.0f, 250.0f ) );
	BOOST_CHECK( BotSFJ_SlowedOut( 420.0f, 180.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_SlowedOut( 420.0f, 260.0f, 250.0f ) );
	/* Never got going: not a slow-out, the take-off logic handles it. */
	BOOST_CHECK( !BotSFJ_SlowedOut( 240.0f, 200.0f, 250.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_track_circle_jump_start )
{
	BOOST_CHECK( BotSFJ_TrackCircleJumpStart( 1, 127, -127, 180.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( BotSFJ_TrackCircleJumpStart( 1, 127, 127, -120.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 0, 127, 127, 180.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 1, 0, 127, 180.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 1, 127, 0, 180.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 1, 127, 127, 30.0f, 300.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 1, 127, 127, 180.0f, 250.0f, 250.0f, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackCircleJumpStart( 1, 127, 127, 180.0f, 300.0f, 250.0f, 0 ) );
}

BOOST_AUTO_TEST_CASE( sfj_track_landing_and_end )
{
	int slow;

	BOOST_CHECK( BotSFJ_TrackLanded( 0, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackLanded( 1, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackLanded( 0, 0 ) );

	/* Slow only counts on the ground at or below ground speed. */
	BOOST_CHECK_EQUAL( BotSFJ_TrackSlowSince( 0, 1000, 0, 100.0f, 250.0f ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackSlowSince( 0, 1000, 1, 400.0f, 250.0f ), 0 );
	slow = BotSFJ_TrackSlowSince( 0, 1000, 1, 250.0f, 250.0f );
	BOOST_CHECK_EQUAL( slow, 1000 );
	slow = BotSFJ_TrackSlowSince( slow, 1100, 1, 200.0f, 250.0f );
	BOOST_CHECK_EQUAL( slow, 1000 );
	BOOST_CHECK( !BotSFJ_TrackShouldEnd( slow, 1100 ) );
	BOOST_CHECK( BotSFJ_TrackShouldEnd( slow, 1000 + BOT_SFJ_TRACK_END_MS ) );
	/* A landing hop that is still fast resets the timer. */
	BOOST_CHECK_EQUAL( BotSFJ_TrackSlowSince( slow, 1120, 1, 380.0f, 250.0f ), 0 );
	BOOST_CHECK( !BotSFJ_TrackShouldEnd( 0, 5000 ) );
}

BOOST_AUTO_TEST_CASE( sfj_track_keep_rules )
{
	BOOST_CHECK( BotSFJ_TrackKeep( 2, 300.0f, 0 ) );
	BOOST_CHECK( !BotSFJ_TrackKeep( 1, 900.0f, 0 ) );
	BOOST_CHECK( !BotSFJ_TrackKeep( 4, 100.0f, 0 ) );
	BOOST_CHECK( !BotSFJ_TrackKeep( 4, 900.0f, 1 ) );
}

BOOST_AUTO_TEST_CASE( sfj_continuity_settings_and_drop_limits_are_bounded )
{
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( 80.0f, 16.0f, 256.0f ), 80.0f );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( -1.0f, 16.0f, 256.0f ), 16.0f );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( 999.0f, 0.0f, 200.0f ), 200.0f );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( NAN, 16.0f, 256.0f ), 16.0f );
	BOOST_CHECK_EQUAL( BotSFJ_ClampSetting( INFINITY, 0.0f, 200.0f ), 0.0f );
	BOOST_CHECK( BotSFJ_HeightAllows( -200.0f, 32.0f, 200.0f ) );
	BOOST_CHECK( BotSFJ_HeightAllows( 32.0f, 32.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( -201.0f, 32.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( 33.0f, 32.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( -1.0f, 32.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_HeightAllows( NAN, 32.0f, 200.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_drop_advancement_keeps_legal_link_and_direction_gates )
{
	BOOST_CHECK( BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -200.0f, -200.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -201.0f, -200.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -200.0f, -201.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 33.0f, 0.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, 0.0f, 65.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 1, 1, 1, -100.0f, -100.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 0, 1, -100.0f, -100.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 0, -100.0f, -100.0f, 1.0f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -100.0f, -100.0f, 0.8f, 8.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_AdvanceDropLinkAllows( 0, 1, 1, -100.0f, -100.0f, 1.0f, -1.0f, 200.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_recorded_nodes_are_progress_markers_not_exact_landings )
{
	const float from[3] = { 0.0f, 0.0f, 0.0f };
	const float node[3] = { 300.0f, 0.0f, 0.0f };
	const float passed[3] = { 500.0f, 80.0f, 200.0f };
	const float outside[3] = { 500.0f, 81.0f, 0.0f };
	const float wrongFloor[3] = { 500.0f, 0.0f, 65.0f };
	const float near[3] = { 220.0f, 0.0f, 0.0f };
	const float behind[3] = { 219.0f, 0.0f, 0.0f };

	BOOST_CHECK( BotSFJ_TrackNodePassed( passed, from, node, 0, 80.0f, 64.0f ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( outside, from, node, 0, 80.0f, 64.0f ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( wrongFloor, from, node, 1, 80.0f, 64.0f ) );
	BOOST_CHECK( BotSFJ_TrackNodePassed( wrongFloor, from, node, 0, 80.0f, 64.0f ) );
	BOOST_CHECK( BotSFJ_TrackNodePassed( near, from, node, 1, 80.0f, 64.0f ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( behind, from, node, 1, 80.0f, 64.0f ) );
	BOOST_CHECK( !BotSFJ_TrackNodePassed( passed, node, node, 0, 80.0f, 64.0f ) );
}

BOOST_AUTO_TEST_CASE( sfj_continuity_ignores_small_speed_losses_and_smooths_wraparound )
{
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420.0f, 250.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420.0f, 200.0f, 250.0f ) );
	BOOST_CHECK( BotSFJ_ContinuitySlowedOut( 420.0f, 180.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 260.0f, 180.0f, 250.0f ) );
	BOOST_CHECK( !BotSFJ_ContinuitySlowedOut( 420.0f, 0.0f, 0.0f ) );
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 350.0f, 10.0f, 12.0f ), 362.0f );
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 10.0f, 350.0f, 12.0f ), -2.0f );
	BOOST_CHECK_EQUAL( BotSFJ_SteerYaw( 350.0f, 355.0f, 12.0f ), 355.0f );
}

BOOST_AUTO_TEST_CASE( nav_recent_waypoint_ring )
{
	int ring[BOT_NAV_RECENT_WAYPOINTS] = { 0 };
	int head = 0;
	int i;

	BOOST_CHECK( !BotNav_RecentContains( ring, BOT_NAV_RECENT_WAYPOINTS, 0 ) );
	head = BotNav_RecentPush( ring, BOT_NAV_RECENT_WAYPOINTS, head, 0 );
	BOOST_CHECK( BotNav_RecentContains( ring, BOT_NAV_RECENT_WAYPOINTS, 0 ) );
	/* Duplicates do not advance the ring. */
	BOOST_CHECK_EQUAL( BotNav_RecentPush( ring, BOT_NAV_RECENT_WAYPOINTS, head, 0 ), head );
	for (i = 1; i <= BOT_NAV_RECENT_WAYPOINTS; i++)
		head = BotNav_RecentPush( ring, BOT_NAV_RECENT_WAYPOINTS, head, i );
	/* Oldest entry (0) has been overwritten. */
	BOOST_CHECK( !BotNav_RecentContains( ring, BOT_NAV_RECENT_WAYPOINTS, 0 ) );
	BOOST_CHECK( BotNav_RecentContains( ring, BOT_NAV_RECENT_WAYPOINTS, BOT_NAV_RECENT_WAYPOINTS ) );
	BOOST_CHECK( !BotNav_RecentContains( ring, BOT_NAV_RECENT_WAYPOINTS, -1 ) );
}

BOOST_AUTO_TEST_CASE( nav_progress_stall )
{
	BOOST_CHECK( BotNav_ProgressImproved( 0.0f, 900.0f ) );
	BOOST_CHECK( BotNav_ProgressImproved( 900.0f, 800.0f ) );
	BOOST_CHECK( !BotNav_ProgressImproved( 900.0f, 890.0f ) );
	BOOST_CHECK( !BotNav_ProgressStalled( 0, 10000 ) );
	BOOST_CHECK( !BotNav_ProgressStalled( 1000, 1000 + BOT_NAV_PROGRESS_STALL_MS - 1 ) );
	BOOST_CHECK( BotNav_ProgressStalled( 1000, 1000 + BOT_NAV_PROGRESS_STALL_MS ) );
}


BOOST_AUTO_TEST_CASE( saber_good_grade_keeps_a_chain_that_is_landing_hits )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );

	context.chainLength = 3;
	context.landedHit = 1;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_GRADE_GOOD ),
		NEWBOTAI_SABER_TACTIC_CHAIN );
	context.landedHit = 0;
	BOOST_CHECK_EQUAL( NewBotAI_ApplySaberChoiceGrade( context, NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_GRADE_GOOD ),
		NEWBOTAI_SABER_TACTIC_REPOSITION );
}

BOOST_AUTO_TEST_CASE( saber_attack_tactics_never_freeze_while_the_swing_window_is_closed )
{
	newbotai_saber_tactic_context_t context = MakeSaberDuelContext( 80.0f );
	const newbotai_saber_tactic_t attacking[] = { NEWBOTAI_SABER_TACTIC_ATTACK,
		NEWBOTAI_SABER_TACTIC_CHAIN, NEWBOTAI_SABER_TACTIC_COUNTER };

	for (const auto tactic : attacking)
	{
		// attackLegal = 0: own swing still recovering.
		const newbotai_saber_command_t waiting = NewBotAI_PlanSaberCommand( context, tactic,
			NEWBOTAI_SABER_HORIZONTAL, 0, -1, 1, 1, 0, 0 );
		BOOST_CHECK_EQUAL( waiting.attack, 0 );
		BOOST_CHECK_EQUAL( waiting.forward, 1 );
		BOOST_CHECK_EQUAL( waiting.right, -1 );
	}
	// Inside the swing-peak range: circle, do not walk through the opponent.
	context.enemyDistance = 40.0f;
	const newbotai_saber_command_t close = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_HORIZONTAL, 0, 1, 1, 1, 0, 0 );
	BOOST_CHECK_EQUAL( close.forward, 0 );
	BOOST_CHECK_EQUAL( close.right, 1 );
	// STAND_SWING is the mediocre "swing in place" grade and keeps its standstill.
	const newbotai_saber_command_t stand = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_STAND_SWING, NEWBOTAI_SABER_HORIZONTAL, 0, 1, 1, 1, 0, 0 );
	BOOST_CHECK_EQUAL( stand.forward, 0 );
	BOOST_CHECK_EQUAL( stand.right, 0 );
	// Airborne footwork is unchanged.
	const newbotai_saber_command_t air = NewBotAI_PlanSaberCommand( context,
		NEWBOTAI_SABER_TACTIC_ATTACK, NEWBOTAI_SABER_HORIZONTAL, 0, 1, 0, 1, 0, 0 );
	BOOST_CHECK_EQUAL( air.forward, -1 );
}

BOOST_AUTO_TEST_CASE( committed_swings_drop_backpedal_above_low_skill )
{
	BOOST_CHECK( NewBotAI_SaberCommittedSwingDropsBackpedal( 7, 1, 0 ) );
	BOOST_CHECK( NewBotAI_SaberCommittedSwingDropsBackpedal( 10, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedSwingDropsBackpedal( 6, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedSwingDropsBackpedal( 10, 0, 0 ) );
	BOOST_CHECK( !NewBotAI_SaberCommittedSwingDropsBackpedal( 10, 1, 1 ) );

	// Back-left diagonal while the enemy is ahead on +x: only the lateral part survives.
	float x = -0.7071f, y = 0.7071f;
	BOOST_CHECK( NewBotAI_StripBackwardComponent( &x, &y, 1.0f, 0.0f ) );
	BOOST_CHECK_SMALL( x, 0.0001f );
	BOOST_CHECK_CLOSE( y, 0.7071f, 0.01f );
	// Forward or lateral input is untouched.
	x = 0.5f; y = 0.5f;
	BOOST_CHECK( !NewBotAI_StripBackwardComponent( &x, &y, 1.0f, 0.0f ) );
	BOOST_CHECK_EQUAL( x, 0.5f );
	// Pure backpedal becomes no movement.
	x = -1.0f; y = 0.0f;
	BOOST_CHECK( NewBotAI_StripBackwardComponent( &x, &y, 1.0f, 0.0f ) );
	BOOST_CHECK_SMALL( x * x + y * y, 0.0001f );
}

BOOST_AUTO_TEST_CASE( strafe_chain_rejumps_accept_safe_landings_off_corridor )
{
	// Strict rule still accepts everything it accepted before.
	BOOST_CHECK( BotSFJ_ChainLandingAllows( 0, 1, 1.0f, 1.0f ) );
	// First takeoff outside the corridor is still rejected.
	BOOST_CHECK( !BotSFJ_ChainLandingAllows( 0, 0, 1.0f, 0.9f ) );
	// Running chain: off-corridor but safe and heading along the route.
	BOOST_CHECK( BotSFJ_ChainLandingAllows( 1, 0, 1.0f, 0.6f ) );
	// Never steep floors or landings that turn back.
	BOOST_CHECK( !BotSFJ_ChainLandingAllows( 1, 0, 0.6f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_ChainLandingAllows( 1, 0, 1.0f, 0.4f ) );
}

BOOST_AUTO_TEST_CASE( strafe_chain_survives_brief_wall_scrapes )
{
	BOOST_CHECK( !BotSFJ_SlowedOutShouldRelease( 0, 1000, 5000 ) );
	BOOST_CHECK( !BotSFJ_SlowedOutShouldRelease( 1, 0, 5000 ) );
	BOOST_CHECK( !BotSFJ_SlowedOutShouldRelease( 1, 1000, 1000 + BOT_SFJ_WALL_SCRAPE_GRACE_MS - 1 ) );
	BOOST_CHECK( BotSFJ_SlowedOutShouldRelease( 1, 1000, 1000 + BOT_SFJ_WALL_SCRAPE_GRACE_MS ) );
	BOOST_CHECK( !BotSFJ_SlowedOutShouldRelease( 1, 2805, 2805 + 999 ) );
	/* A recovered speed resets the slowdown condition even at the grace boundary. */
	BOOST_CHECK( !BotSFJ_SlowedOutShouldRelease( 0, 1782, 1782 + 1000 ) );
	BOOST_CHECK_GE( BOT_SFJ_WALL_SCRAPE_GRACE_MS, 1000 );
	BOOST_CHECK( BotSFJ_WallSlowdownLandingAllows( 1, 0.1f ) );
	BOOST_CHECK( !BotSFJ_WallSlowdownLandingAllows( 0, 0.1f ) );
	BOOST_CHECK( BotSFJ_WallSlowdownLandingAllows( 0, 0.4f ) );
}

BOOST_AUTO_TEST_CASE( no_grounded_crouch_roll_at_strafe_speed )
{
	BOOST_CHECK( !BotSFJ_GroundCrouchAllowed( 1, 450.0f, 250.0f, 0 ) );
	BOOST_CHECK( BotSFJ_GroundCrouchAllowed( 1, 250.0f, 250.0f, 0 ) );
	BOOST_CHECK( BotSFJ_GroundCrouchAllowed( 0, 600.0f, 250.0f, 0 ) );	// air crouch is fine
	BOOST_CHECK( BotSFJ_GroundCrouchAllowed( 1, 450.0f, 250.0f, 1 ) );	// deliberate roll
	BOOST_CHECK( BotSFJ_GroundCrouchAllowed( 1, 450.0f, 0.0f, 0 ) );
}

BOOST_AUTO_TEST_CASE( escape_strafes_start_outside_saber_range )
{
	BOOST_CHECK_EQUAL( BotSFJ_EscapeStartDistance( 320.0f ), BOT_SFJ_ESCAPE_MIN_DISTANCE );
	BOOST_CHECK_EQUAL( BotSFJ_EscapeStartDistance( 64.0f ), 64.0f );
	BOOST_CHECK_EQUAL( BotSFJ_EscapeStartDistance( 0.0f ), 0.0f );
	BOOST_CHECK_EQUAL( BotSFJ_EscapeStartDistance( -5.0f ), 0.0f );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE( recorded_route_tolerance )
{
	const float nodes[] = {
		400, 0, 0,
		800, 0, 0,
		1200, 0, 0,
	};
	const float nearSecond[] = { 760, 30, 0 };
	const float nearFirst[] = { 300, 0, 0 };

	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTolerance( 80.0f, 416.0f, 0 ), 80.0f );
	BOOST_CHECK_CLOSE( BotSFJ_TrackNodeTolerance( 80.0f, 416.0f, 1 ), 124.8f, 0.01 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTolerance( 80.0f, 100.0f, 1 ), 80.0f );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTolerance( 80.0f, 2000.0f, 1 ), BOT_SFJ_TRACK_LOOSE_MAX_TOLERANCE );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTolerance( 16.0f, 50.0f, 1 ), BOT_SFJ_TRACK_LOOSE_MIN_TOLERANCE );

	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 957, 0 ), 3000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 957, 1 ), 3414 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 100, 1 ), 3000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 20000, 1 ), 15000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 8811, 1 ), 15000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackNodeTimeout( 3000, 2147483647, 1 ), 15000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackAirTimeout( 1, 3000, 8811, 1 ), 15000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackAirTimeout( 1, 3000, 3500, 1 ), 8500 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackAirTimeout( 1, 3000, 8811, 0 ), 1500 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackAirTimeout( 0, 3000, 8811, 1 ), 1500 );

	BOOST_CHECK_EQUAL( BotSFJ_TrackSkipAhead( nearSecond, nodes, 0, 3, 2 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackSkipAhead( nearFirst, nodes, 0, 3, 2 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackSkipAhead( nearSecond, nodes, 2, 3, 2 ), 0 );

	// a 124-unit tolerance passes a landing 110 units wide of a 416-unit hop
	{
		const float from[] = { 0, 0, 0 };
		const float node[] = { 416, 0, 0 };
		const float landed[] = { 430, 110, 0 };
		BOOST_CHECK( !BotSFJ_TrackNodePassed( landed, from, node, 1, 80.0f, 64.0f ) );
		BOOST_CHECK( BotSFJ_TrackNodePassed( landed, from, node, 1,
			BotSFJ_TrackNodeTolerance( 80.0f, 416.0f, 1 ), BOT_SFJ_TRACK_LOOSE_HEIGHT ) );
	}
}

BOOST_AUTO_TEST_CASE( strafetrack_export_coverage )
{
	// fastest first: three routes from the same spot, then two elsewhere
	const float starts[] = {
		0, 0, 0,
		10, 0, 0,
		20, 0, 0,
		1000, 0, 0,
		0, 1000, 0,
	};
	unsigned char picked[5];

	BOOST_CHECK_EQUAL( BotSFJ_TrackSelectCoverage( starts, 5, 3, 192.0f, picked ), 3 );
	BOOST_CHECK( picked[0] );
	BOOST_CHECK( !picked[1] );
	BOOST_CHECK( !picked[2] );
	BOOST_CHECK( picked[3] );
	BOOST_CHECK( picked[4] );

	// remaining slots fill in speed order
	BOOST_CHECK_EQUAL( BotSFJ_TrackSelectCoverage( starts, 5, 4, 192.0f, picked ), 4 );
	BOOST_CHECK( picked[1] );
	BOOST_CHECK( !picked[2] );

	BOOST_CHECK_EQUAL( BotSFJ_TrackSelectCoverage( starts, 5, 10, 192.0f, picked ), 5 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackSelectCoverage( starts, 0, 10, 192.0f, picked ), 0 );
}

BOOST_AUTO_TEST_CASE( saber_pursuit_escape_stall )
{
	int still = 0;

	BOOST_CHECK_EQUAL( NewBotAI_PursuitJumpChance( 1, 0 ), NEWBOTAI_PURSUIT_SABER_CHANCE );
	BOOST_CHECK_EQUAL( NewBotAI_PursuitJumpChance( 0, 0 ), NEWBOTAI_PURSUIT_FORCE_CHANCE );
	BOOST_CHECK_LE( NewBotAI_PursuitJumpChance( 1, 1000 ), 35 );
	BOOST_CHECK_GE( NewBotAI_PursuitJumpChance( 0, -1000 ), 0 );

	BOOST_CHECK( NewBotAI_ShouldPursuitJump( -150.0f, 180.0f, 1, 0, 0, 1, 20, 10 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 180.0f, 1, 0, 0, 1, 20, 30 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -50.0f, 180.0f, 1, 0, 0, 1, 20, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 60.0f, 1, 0, 0, 1, 20, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 400.0f, 1, 0, 0, 1, 20, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 180.0f, 0, 0, 0, 1, 20, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 180.0f, 1, 1, 0, 1, 20, 1 ) );
	BOOST_CHECK( !NewBotAI_ShouldPursuitJump( -150.0f, 180.0f, 1, 0, 0, 0, 20, 1 ) );

	BOOST_CHECK( NewBotAI_PursuitLandingSwing( 1500, 1000, 1, 80.0f, 100.0f ) );
	BOOST_CHECK( !NewBotAI_PursuitLandingSwing( 2000, 1000, 1, 80.0f, 100.0f ) );
	BOOST_CHECK( !NewBotAI_PursuitLandingSwing( 1500, 1000, 0, 80.0f, 100.0f ) );
	BOOST_CHECK( !NewBotAI_PursuitLandingSwing( 1500, 1000, 1, 150.0f, 100.0f ) );

	BOOST_CHECK_EQUAL( NewBotAI_SaberBurstLimit( 3, 0 ), 2 );
	BOOST_CHECK_EQUAL( NewBotAI_SaberBurstLimit( 8, 10 ), 4 );
	BOOST_CHECK( NewBotAI_SaberBurstCompleteScaled( NEWBOTAI_SABER_BURST, 2, 1, 0, 4 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstCompleteScaled( NEWBOTAI_SABER_BURST, 2, 0, 1, 4 ) );
	BOOST_CHECK( NewBotAI_SaberBurstCompleteScaled( NEWBOTAI_SABER_BURST, 4, 0, 1, 4 ) );
	BOOST_CHECK( !NewBotAI_SaberBurstCompleteScaled( NEWBOTAI_SABER_BASIC, 4, 1, 1, 2 ) );

	// healthy bots keep the planner's back-pedal
	BOOST_CHECK_EQUAL( NewBotAI_ChooseEscapeStyle( 80, 1, 1, 0, 0 ), NEWBOTAI_ESCAPE_BACKPEDAL );
	{
		int counts[3] = { 0, 0, 0 };
		int r;
		for ( r = 0; r < 100; r++ )
			counts[NewBotAI_ChooseEscapeStyle( 30, 1, 1, 0, r )]++;
		BOOST_CHECK_LE( counts[NEWBOTAI_ESCAPE_BACKPEDAL], 30 );
		BOOST_CHECK_GT( counts[NEWBOTAI_ESCAPE_DIAGONAL], 0 );
		BOOST_CHECK_GT( counts[NEWBOTAI_ESCAPE_JUMP], 0 );
	}
	BOOST_CHECK_EQUAL( NewBotAI_ChooseEscapeStyle( 30, 0, 0, 0, 7 ), NEWBOTAI_ESCAPE_BACKPEDAL );

	BOOST_CHECK( !NewBotAI_StallBreakDue( 1000, &still, 10.0f, 100.0f, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_StallBreakDue( 1400, &still, 10.0f, 100.0f, 1, 0 ) );
	BOOST_CHECK( NewBotAI_StallBreakDue( 1700, &still, 10.0f, 100.0f, 1, 0 ) );
	BOOST_CHECK_EQUAL( still, 0 );
	BOOST_CHECK( !NewBotAI_StallBreakDue( 1800, &still, 10.0f, 100.0f, 1, 1 ) );
	BOOST_CHECK( !NewBotAI_StallBreakDue( 2600, &still, 10.0f, 100.0f, 1, 0 ) );
	BOOST_CHECK( !NewBotAI_StallBreakDue( 3400, &still, 200.0f, 100.0f, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( footing_smoother )
{
	int sign = 0, since = 0, active = 0, rev = 0;
	int held = 0, released = 0;

	BOOST_CHECK_EQUAL( BotFooting_HoldMs( 1 ), BOT_FOOTING_HOLD_MAX_MS );
	BOOST_CHECK_EQUAL( BotFooting_HoldMs( 5 ), BOT_FOOTING_HOLD_MIN_MS );
	BOOST_CHECK_EQUAL( BotFooting_HoldMs( 99 ), BOT_FOOTING_HOLD_MIN_MS );

	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( 127, &sign, &since, &active, 1000, 200, &rev ), 127 );
	BOOST_CHECK( !rev );
	// quick reversal becomes a stop
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( -127, &sign, &since, &active, 1050, 200, &rev ), 0 );
	BOOST_CHECK( rev );
	// reversal through a zero frame is still deferred
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( 0, &sign, &since, &active, 1080, 200, &rev ), 0 );
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( -127, &sign, &since, &active, 1100, 200, &rev ), 0 );
	// same direction keeps going
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( 100, &sign, &since, &active, 1120, 200, &rev ), 100 );
	// after the hold, reversal is allowed
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( -127, &sign, &since, &active, 1210, 200, &rev ), -127 );
	BOOST_CHECK( !rev );
	// long idle forgets the direction
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( 0, &sign, &since, &active, 1500, 200, &rev ), 0 );
	BOOST_CHECK_EQUAL( BotFooting_FilterAxis( 127, &sign, &since, &active, 1510, 200, &rev ), 127 );

	BOOST_CHECK( BotFooting_FilterButton( 1, &held, &released, 0, 200 ) );
	BOOST_CHECK( !BotFooting_FilterButton( 0, &held, &released, 50, 200 ) );
	BOOST_CHECK( !BotFooting_FilterButton( 1, &held, &released, 100, 200 ) );
	BOOST_CHECK( BotFooting_FilterButton( 1, &held, &released, 260, 200 ) );
}

BOOST_AUTO_TEST_CASE( strafe_loosen_limits )
{
	BOOST_CHECK_EQUAL( BotSFJ_CorridorLinkRise( 0 ), 32.0f );
	BOOST_CHECK_EQUAL( BotSFJ_CorridorLinkRise( 1 ), 64.0f );
	BOOST_CHECK_EQUAL( BotSFJ_CorridorBaseRise( 0 ), 64.0f );
	BOOST_CHECK_EQUAL( BotSFJ_CorridorBaseRise( 1 ), 96.0f );
	BOOST_CHECK_EQUAL( BotSFJ_CorridorMinLength( 0, 1 ), 160.0f );
	BOOST_CHECK_LT( BotSFJ_CorridorMinLength( 1, 1 ), 160.0f );
	BOOST_CHECK_EQUAL( BotSFJ_CorridorMinLength( 1, 0 ), BotSFJ_CorridorMinLength( 0, 0 ) );

	// world is always acceptable
	BOOST_CHECK( BotSFJ_ArcEntityAllows( 1, 0, 1, 0 ) );
	// legacy: any non-world contact fails
	BOOST_CHECK( !BotSFJ_ArcEntityAllows( 0, 0, 1, 0 ) );
	BOOST_CHECK( !BotSFJ_ArcEntityAllows( 0, 1, 0, 0 ) );
	// loosened: brush entities can be landed on, players only bumped
	BOOST_CHECK( BotSFJ_ArcEntityAllows( 0, 0, 1, 1 ) );
	BOOST_CHECK( !BotSFJ_ArcEntityAllows( 0, 1, 1, 1 ) );
	BOOST_CHECK( BotSFJ_ArcEntityAllows( 0, 1, 0, 1 ) );

	// endpoint stays strict even when loosened
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 100, 0, 130, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( waypoint_skip_goal_helpers )
{
	BOOST_CHECK_EQUAL( BotNav_WaypointSkipCap( -3 ), 0 );
	BOOST_CHECK_EQUAL( BotNav_WaypointSkipCap( 100 ), 100 );
	BOOST_CHECK_EQUAL( BotNav_WaypointSkipCap( 9999 ), BOT_NAV_MAX_WAYPOINT_SKIP );

	BOOST_CHECK_EQUAL( BotNav_StepsToGoal( 10, 110, 1 ), 100 );
	BOOST_CHECK_EQUAL( BotNav_StepsToGoal( 110, 10, -1 ), 100 );
	BOOST_CHECK_EQUAL( BotNav_StepsToGoal( 10, 5, 1 ), -1 );
	BOOST_CHECK_EQUAL( BotNav_StepsToGoal( 7, 7, 1 ), 0 );

	/* Recently left points, sideways points and points with no real gain are refused. */
	BOOST_CHECK( BotNav_GoalNeighborScore( 500.0f, 400.0f, 0.9f, 0 ) > 0.0f );
	BOOST_CHECK( BotNav_GoalNeighborScore( 500.0f, 400.0f, 0.9f, 1 ) < 0.0f );
	BOOST_CHECK( BotNav_GoalNeighborScore( 500.0f, 400.0f, 0.2f, 0 ) < 0.0f );
	BOOST_CHECK( BotNav_GoalNeighborScore( 500.0f, 495.0f, 1.0f, 0 ) < 0.0f );
	BOOST_CHECK( BotNav_GoalNeighborScore( 500.0f, 300.0f, 0.6f, 0 ) >
		BotNav_GoalNeighborScore( 500.0f, 450.0f, 1.0f, 0 ) );

	/* A single failed step does not reverse the trail direction. */
	BOOST_CHECK( !BotNav_SkipShouldFlip( 0, 5000 ) );
	BOOST_CHECK( !BotNav_SkipShouldFlip( 5000, 5000 + BOT_NAV_SKIP_FLIP_DELAY_MS - 1 ) );
	BOOST_CHECK( BotNav_SkipShouldFlip( 5000, 5000 + BOT_NAV_SKIP_FLIP_DELAY_MS ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_preference_and_backward_strafe )
{
	/* Longer and faster routes outrank short or slow ones. */
	BOOST_CHECK( BotSFJ_TrackRouteScore( 3000.0f, 600.0f, 0, 0, 0.0f, 192.0f ) >
		BotSFJ_TrackRouteScore( 800.0f, 600.0f, 0, 0, 0.0f, 192.0f ) );
	BOOST_CHECK( BotSFJ_TrackRouteScore( 2000.0f, 800.0f, 0, 0, 0.0f, 192.0f ) >
		BotSFJ_TrackRouteScore( 2000.0f, 400.0f, 0, 0, 0.0f, 192.0f ) );
	/* Bot successes count; failures sink a route below an untried one of equal shape. */
	BOOST_CHECK( BotSFJ_TrackRouteScore( 2000.0f, 600.0f, 10, 9, 0.0f, 192.0f ) >
		BotSFJ_TrackRouteScore( 2000.0f, 600.0f, 0, 0, 0.0f, 192.0f ) );
	BOOST_CHECK( BotSFJ_TrackRouteScore( 2000.0f, 600.0f, 10, 0, 0.0f, 192.0f ) <
		BotSFJ_TrackRouteScore( 2000.0f, 600.0f, 0, 0, 0.0f, 192.0f ) );
	/* A proven long route still beats a nearer short one. */
	BOOST_CHECK( BotSFJ_TrackRouteScore( 3000.0f, 600.0f, 4, 3, 180.0f, 192.0f ) >
		BotSFJ_TrackRouteScore( 600.0f, 600.0f, 0, 0, 0.0f, 192.0f ) );
	BOOST_CHECK_EQUAL( BotSFJ_TrackRouteScore( 0.0f, 600.0f, 0, 0, 0.0f, 192.0f ), 0.0f );
	BOOST_CHECK_CLOSE( BotSFJ_TrackRouteSuccess( 0, 0 ), 0.5f, 0.001f );
	BOOST_CHECK_CLOSE( BotSFJ_TrackRouteSuccess( 2, 5 ), 0.75f, 0.001f );

	/* Backward strafing only on full-force retreats, at the configured rate. */
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 0, 0, 100, 0 ) );
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 1, 1, 100, 0 ) );
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 1, 0, 0, 0 ) );
	BOOST_CHECK( BotSFJ_UseBackwardStrafe( 1, 0, 100, 99 ) );
	BOOST_CHECK( BotSFJ_UseBackwardStrafe( 1, 0, 50, 49 ) );
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 1, 0, 50, 50 ) );

	/* Mirrored keys with the view turned around keep the same wish direction. */
	for ( int side = -1; side <= 1; side += 2 )
	{
		for ( int strafeOnly = 0; strafeOnly <= 1; strafeOnly++ )
		{
			int forward, right, backForward, backRight;
			BotSFJ_HalfBeatAxes( side, strafeOnly, &forward, &right );
			backForward = forward;
			backRight = right;
			BotSFJ_BackwardAxes( &backForward, &backRight );
			const float wishYaw = 30.0f;
			const float yaw = BotSFJ_ViewYawForAxes( wishYaw, forward, right ) * 0.017453292f;
			const float backYaw = BotSFJ_ViewYawForAxes( wishYaw, backForward, backRight ) * 0.017453292f;
			/* forward axis (cos, sin), right axis (sin, -cos) in Quake yaw */
			const float wx = cosf( yaw ) * forward + sinf( yaw ) * right;
			const float wy = sinf( yaw ) * forward - cosf( yaw ) * right;
			const float bx = cosf( backYaw ) * backForward + sinf( backYaw ) * backRight;
			const float by = sinf( backYaw ) * backForward - cosf( backYaw ) * backRight;
			BOOST_CHECK_SMALL( wx - bx, 0.01f );
			BOOST_CHECK_SMALL( wy - by, 0.01f );
			BOOST_CHECK( backForward <= 0 );
		}
	}
}

BOOST_AUTO_TEST_CASE( recorded_circle_start_has_bounded_reachable_approach )
{
	/* A nearer waypoint or opposite input does not constrain circle-start eligibility. */
	BOOST_CHECK( BotSFJ_TrackEntryAllows( 600.0f, 32.0f, 1, 96.0f ) );
	BOOST_CHECK( BotSFJ_TrackEntryAllows( 768.0f, 64.0f, 1, 96.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( 769.0f, 0.0f, 1, 96.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( 100.0f, 65.0f, 1, 96.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( 100.0f, -65.0f, 1, 96.0f ) );
	/* Mid-route joins retain their smaller pickup bound. */
	BOOST_CHECK( BotSFJ_TrackEntryAllows( 192.0f, 96.0f, 0, 192.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( 193.0f, 0.0f, 0, 192.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( -1.0f, 0.0f, 1, 96.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( INFINITY, 0.0f, 1, 96.0f ) );
	BOOST_CHECK( !BotSFJ_TrackEntryAllows( 10.0f, NAN, 1, 96.0f ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_owns_incidental_navigation_not_required_interactions )
{
	BOOST_CHECK( BotSFJ_TrackIgnoresNavigation( 1, 0 ) );
	BOOST_CHECK( !BotSFJ_TrackIgnoresNavigation( 0, 0 ) );
	BOOST_CHECK( !BotSFJ_TrackIgnoresNavigation( 1, 1 ) );
	/* Ownership never suppresses hard engine/use conflicts. */
	BOOST_CHECK( BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_HARD, BOT_SFJ_PHASE_PREPARE ) );
	BOOST_CHECK( BotSFJ_ConflictBlocks( BOT_SFJ_CONFLICT_HARD, BOT_SFJ_PHASE_AIR ) );
	BOOST_CHECK( BotSFJ_UseIsConflict( 1, 0, 0 ) );
	BOOST_CHECK( BotSFJ_UseIsConflict( 0, 1, 0 ) );
}

BOOST_AUTO_TEST_CASE( loosened_roll_pauses_only_running_chain_without_input_ownership )
{
	BOOST_CHECK( BotSFJ_RollPauseAllows( 1, 1, 1, 0, 500 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 0, 1, 1, 0, 500 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 1, 0, 1, 0, 500 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 1, 1, 0, 0, 500 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 1, 1, 1, 1, 500 ) );
	BOOST_CHECK( BotSFJ_RollPauseAllows( 1, 1, 1, 0, 1500 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 1, 1, 1, 0, 1501 ) );
	BOOST_CHECK( !BotSFJ_RollPauseAllows( 1, 1, 1, 0, -1 ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_wall_recovery_and_retry_are_bounded )
{
	int retryUntil[BOT_SFJ_MAX_TRACK_ROUTES] = { 0 };

	BOOST_CHECK( BotSFJ_TrackRecoveryAllows( 1, 2, 1, 200, 3000 ) );
	BOOST_CHECK( !BotSFJ_TrackRecoveryAllows( 0, 2, 1, 200, 3000 ) );
	BOOST_CHECK( !BotSFJ_TrackRecoveryAllows( 1, 0, 1, 200, 3000 ) );
	BOOST_CHECK( !BotSFJ_TrackRecoveryAllows( 1, 2, 0, 200, 3000 ) );
	BOOST_CHECK( !BotSFJ_TrackRecoveryAllows( 1, 2, 1, 3000, 3000 ) );
	BOOST_CHECK( !BotSFJ_TrackRecoveryAllows( 1, 2, 1, -1, 3000 ) );
	BotSFJ_DelayTrackRetry( 1, retryUntil, 1000 );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( 1, retryUntil, 5999 ) );
	BOOST_CHECK( BotSFJ_TrackRetryAllows( 1, retryUntil, 6000 ) );
	BOOST_CHECK( BotSFJ_TrackRetryAllows( 2, retryUntil, 5999 ) );
	/* Loosened landing recovery does not relax terminal endpoint or landing slope. */
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 100, 0, 250, 0, 96 ) );
	BOOST_CHECK( !BotSFJ_ChainLandingAllows( 1, 0, 0.6f, 1.0f ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_cooldowns_survive_alternating_failures_and_reset_on_reload )
{
	int retryUntil[BOT_SFJ_MAX_TRACK_ROUTES] = { 0 };
	BotSFJ_DelayTrackRetry( 0, retryUntil, 1000 );
	BotSFJ_DelayTrackRetry( 1, retryUntil, 1500 );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( 0, retryUntil, 1600 ) );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( 1, retryUntil, 1600 ) );
	BOOST_CHECK( BotSFJ_TrackRetryAllows( 2, retryUntil, 1600 ) );
	BOOST_CHECK( BotSFJ_TrackRetryAllows( 0, retryUntil, 6000 ) );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( 1, retryUntil, 6000 ) );
	BOOST_CHECK( BotSFJ_TrackRetryAllows( 1, retryUntil, 6500 ) );
	BotSFJ_DelayTrackRetry( BOT_SFJ_MAX_TRACK_ROUTES - 1, retryUntil, 2000 );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( BOT_SFJ_MAX_TRACK_ROUTES - 1, retryUntil, 2001 ) );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( -1, retryUntil, 2001 ) );
	BOOST_CHECK( !BotSFJ_TrackRetryAllows( BOT_SFJ_MAX_TRACK_ROUTES, retryUntil, 2001 ) );
	BotSFJ_ClearTrackRetries( retryUntil );
	for ( int i = 0; i < BOT_SFJ_MAX_TRACK_ROUTES; i++ )
		BOOST_CHECK( BotSFJ_TrackRetryAllows( i, retryUntil, 2001 ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_stats_skip_header_and_inter_record_newlines )
{
	const char *text = "botroute_stats 2\n"
		"1 2 3 4 5 6 10 7 2 1 3000.0 5000 3000000.0 3500000.0\n"
		"\r\n \t7 8 9 10 11 12 4 2 1 1 1200.0 3000 1200000.0 1500000.0\n";
	const char header[] = "botroute_stats 2";
	text += sizeof( header ) - 1;
	for ( int row = 0; row < 2; row++ )
	{
		text = BotSFJ_SkipStatsRecordWhitespace( text );
		double values[14];
		for ( int column = 0; column < 14; column++ )
		{
			/* Model the engine parser's refusal to cross a newline inside a row. */
			while ( *text == ' ' || *text == '\t' )
				text++;
			BOOST_REQUIRE( *text && *text != '\n' && *text != '\r' );
			char *end;
			values[column] = std::strtod( text, &end );
			BOOST_REQUIRE( end != text );
			text = end;
		}
		BOOST_CHECK_EQUAL( values[0], row ? 7 : 1 );
		BOOST_CHECK_EQUAL( values[6], row ? 4 : 10 );
		BOOST_CHECK_EQUAL( values[10], row ? 1200 : 3000 );
	}
	BOOST_CHECK_EQUAL( *BotSFJ_SkipStatsRecordWhitespace( text ), '\0' );
	const char *legacy = BotSFJ_SkipStatsRecordWhitespace( "\n\r\t 1 2 3 4 5 6 10 7\n" );
	BOOST_CHECK_EQUAL( *legacy, '1' );
	BOOST_CHECK( !BotSFJ_SkipStatsRecordWhitespace( NULL ) );
}

BOOST_AUTO_TEST_CASE( recorded_route_actual_performance_beats_recorded_speed_alone )
{
	const float fast = BotSFJ_TrackPerformance( 3000.0f, 5000.0f, 3000000.0f, 3000000.0f );
	const float stalled = BotSFJ_TrackPerformance( 300.0f, 5000.0f, 300000.0f, 3000000.0f );
	const float lostSpeed = BotSFJ_TrackPerformance( 3000.0f, 5000.0f, 1000000.0f, 3000000.0f );
	BOOST_CHECK( fast > stalled );
	BOOST_CHECK( fast > lostSpeed );
	BOOST_CHECK_EQUAL( BotSFJ_TrackPerformance( 0, 0, 0, 0 ), 1.0f );
	BOOST_CHECK_EQUAL( BotSFJ_TrackPerformance( NAN, 100, 0, 0 ), 1.0f );
	BOOST_CHECK_EQUAL( BotSFJ_TrackPerformance( 100, INFINITY, 0, 0 ), 1.0f );
	BOOST_CHECK( BotSFJ_TrackSampleAllows( 100, 60, 600 ) );
	BOOST_CHECK( !BotSFJ_TrackSampleAllows( 100, 2000, 600 ) );
	BOOST_CHECK( !BotSFJ_TrackSampleAllows( 1001, 60, 600 ) );
	BOOST_CHECK( !BotSFJ_TrackSampleAllows( 0, 0, 600 ) );
	BOOST_CHECK( !BotSFJ_TrackSampleAllows( 100, NAN, 600 ) );
	BOOST_CHECK( !BotSFJ_TrackSampleAllows( 100, 60, INFINITY ) );
	BOOST_CHECK_EQUAL( BotSFJ_TrackMeasuredDuration( 1, 1000, 5000 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackMeasuredDuration( 0, 5000, 6000 ), 1000 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackMeasuredDuration( 0, 6000, 6000 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackMeasuredDuration( 0, 6000, 5000 ), 0 );
	/* A rejected, trace-blocked takeoff teaches a failure before real contact. */
	BOOST_CHECK( BotSFJ_TrackBlockedFailure( 0, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackBlockedFailure( 1, 1 ) );
	BOOST_CHECK( !BotSFJ_TrackBlockedFailure( 0, 0 ) );
	/* Three interrupted runs leave one completion and one actual failed attempt. */
	BOOST_CHECK_EQUAL( BotSFJ_TrackResolvedAttempts( 5, 1, 3 ), 2 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackResolvedAttempts( 5, 1, 0 ), 5 );
	BOOST_CHECK_EQUAL( BotSFJ_TrackResolvedAttempts( 5, 1, 8 ), 1 );
	/* Untried routes retain a positive score, instead of being permanently excluded. */
	BOOST_CHECK( BotSFJ_TrackRouteScore( 1000, 600, 0, 0, 600, 768 ) > 0.0f );
}

BOOST_AUTO_TEST_CASE( backward_boolean_preserves_legacy_probability_when_enabled )
{
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 0, 100 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 1, 50 ), 50 );
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 1, 0 ), 0 );
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 1, 100 ), 100 );
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 1, 150 ), 100 );
	BOOST_CHECK_EQUAL( BotSFJ_BackwardPercent( 1, -1 ), 0 );
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 1, 0, BotSFJ_BackwardPercent( 0, 100 ), 0 ) );
	BOOST_CHECK( !BotSFJ_UseBackwardStrafe( 1, 1, BotSFJ_BackwardPercent( 1, 100 ), 0 ) );
	BOOST_CHECK( BotSFJ_UseBackwardStrafe( 1, 0, BotSFJ_BackwardPercent( 1, 100 ), 99 ) );
}

BOOST_AUTO_TEST_CASE( saber_only_sweeps_and_low_health )
{
	/* Yellow 2-4 and staff 3-6 planned sweeps; other stances keep the link roll. */
	BOOST_CHECK_EQUAL( NewBotAI_SweepChainTarget( NEWBOTAI_STANCE_MEDIUM, 1 ), 2 );
	BOOST_CHECK_EQUAL( NewBotAI_SweepChainTarget( NEWBOTAI_STANCE_MEDIUM, 100 ), 4 );
	BOOST_CHECK_EQUAL( NewBotAI_SweepChainTarget( NEWBOTAI_STANCE_STAFF, 1 ), 3 );
	BOOST_CHECK_EQUAL( NewBotAI_SweepChainTarget( NEWBOTAI_STANCE_STAFF, 100 ), 6 );
	BOOST_CHECK_EQUAL( NewBotAI_SweepChainTarget( 3, 50 ), 0 );
	BOOST_CHECK( NewBotAI_SweepChainForceLink( 1, 3, 90.0f ) );
	BOOST_CHECK( !NewBotAI_SweepChainForceLink( 3, 3, 90.0f ) );
	BOOST_CHECK( !NewBotAI_SweepChainForceLink( 1, 3, 130.0f ) );
	BOOST_CHECK( !NewBotAI_SweepChainForceLink( 1, 0, 90.0f ) );
	BOOST_CHECK_CLOSE( NewBotAI_SweepStanceFanBias( 50.0f, NEWBOTAI_STANCE_STAFF ), 70.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SweepStanceFanBias( 90.0f, NEWBOTAI_STANCE_MEDIUM ), 100.0f, 0.001f );
	BOOST_CHECK_CLOSE( NewBotAI_SweepStanceFanBias( 50.0f, 3 ), 50.0f, 0.001f );
	BOOST_CHECK_EQUAL( NewBotAI_SweepStanceFanBias( 0.0f, NEWBOTAI_STANCE_STAFF ), 0.0f );

	/* Back off only from 90-120u; closer, step in or hold; never retreat beyond reach. */
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 130.0f, 0, 0, 1 ), NEWBOTAI_SWING_DODGE_NONE );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 100.0f, 0, 0, 1 ), NEWBOTAI_SWING_DODGE_LATERAL );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 75.0f, 0, 0, 1 ), NEWBOTAI_SWING_DODGE_ADVANCE );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 75.0f, 0, 0, 100 ), NEWBOTAI_SWING_DODGE_NONE );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 40.0f, 0, 0, 70 ), NEWBOTAI_SWING_DODGE_ADVANCE );
	BOOST_CHECK_EQUAL( NewBotAI_GetSaberOnlySweepResponse( 40.0f, 20, 5, 1 ), NEWBOTAI_SWING_DODGE_BACKPEDAL );
	for ( int roll = 1; roll <= 100; roll++ )
		BOOST_CHECK( NewBotAI_GetSaberOnlySweepResponse( 80.0f, 0, 0, roll ) != NEWBOTAI_SWING_DODGE_BACKPEDAL );

	/* Punish in the gap after the swing, once. */
	BOOST_CHECK( NewBotAI_SweepPunishReady( 100, 90.0f, 0 ) );
	BOOST_CHECK( !NewBotAI_SweepPunishReady( 100, 90.0f, 1 ) );
	BOOST_CHECK( !NewBotAI_SweepPunishReady( 400, 90.0f, 0 ) );
	BOOST_CHECK( !NewBotAI_SweepPunishReady( 100, 140.0f, 0 ) );

	/* Enemy chain tracking and holding off a sweeper just outside reach. */
	BOOST_CHECK_EQUAL( NewBotAI_EnemySweepChainCount( 1, 100 ), 2 );
	BOOST_CHECK_EQUAL( NewBotAI_EnemySweepChainCount( 3, 900 ), 1 );
	BOOST_CHECK_EQUAL( NewBotAI_EnemySweepChainCount( 3, -1 ), 1 );
	BOOST_CHECK( NewBotAI_ShouldHoldOffSweeper( 2, 1, 150.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldHoldOffSweeper( 1, 1, 150.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldHoldOffSweeper( 2, 1, 100.0f ) );
	BOOST_CHECK( !NewBotAI_ShouldHoldOffSweeper( 2, 0, 150.0f ) );

	/* Low health changes escape style only outside saber-only play. */
	BOOST_CHECK_EQUAL( NewBotAI_ChooseEscapeStyle( NewBotAI_EscapeHealthFor( 20, 1 ), 1, 1, 0, 10 ),
		NEWBOTAI_ESCAPE_BACKPEDAL );
	BOOST_CHECK_EQUAL( NewBotAI_EscapeHealthFor( 20, 0 ), 20 );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( bot_strafejump )

BOOST_AUTO_TEST_CASE( minimum_pmove_slice_still_has_full_prediction_horizon )
{
	BOOST_CHECK_GE( BOT_SFJ_MAX_ARC_STEPS, 2000 );
	BOOST_CHECK_EQUAL( BotSFJ_PmoveSliceMsec( 1, 1, 1, 0 ), 1 );
	float height = 0.0f, velocity = 225.0f;
	int steps = 0;
	do
	{
		const float nextVelocity = velocity - 800.0f * 0.001f;
		height += (velocity + nextVelocity) * 0.5f * 0.001f;
		velocity = nextVelocity;
		++steps;
	} while (height > 0.0f && steps < BOT_SFJ_MAX_ARC_STEPS);
	BOOST_CHECK_LT( steps, BOT_SFJ_MAX_ARC_STEPS );
	BOOST_CHECK_GT( steps, 256 );
}

BOOST_AUTO_TEST_CASE( release_requires_consumed_usercmd_and_cleared_engine_flag )
{
	BOOST_CHECK( !BotSFJ_JumpReleaseConfirmed( 0, 1000, 0 ) );
	BOOST_CHECK( !BotSFJ_JumpReleaseConfirmed( 1000, 999, 0 ) );
	BOOST_CHECK( !BotSFJ_JumpReleaseConfirmed( 1000, 1000, 1 ) );
	BOOST_CHECK( BotSFJ_JumpReleaseConfirmed( 1000, 1000, 0 ) );
	BOOST_CHECK( BotSFJ_JumpReleaseConfirmed( 1000, 1500, 0 ) );
}

BOOST_AUTO_TEST_CASE( confirmed_air_release_rejumps_on_first_grounded_command )
{
	bot_sfj_phase_t phase = BOT_SFJ_PHASE_TAKEOFF;
	for (int hop = 0; hop < 8; ++hop)
	{
		BOOST_CHECK( BotSFJ_JumpPressed( phase, 1 ) );
		phase = BotSFJ_ReleasedNextPhase( phase, 0, 25, 0 );
		BOOST_CHECK_EQUAL( phase, BOT_SFJ_PHASE_AIR );
		BOOST_CHECK( !BotSFJ_JumpPressed( phase, 0 ) );
		phase = BotSFJ_ReleasedNextPhase( phase, 0, 300, 1 );
		BOOST_CHECK_EQUAL( phase, BOT_SFJ_PHASE_AIR );
		phase = BotSFJ_ReleasedNextPhase( phase, 1, 525, 1 );
		BOOST_CHECK_EQUAL( phase, BOT_SFJ_PHASE_REJUMP );
	}
}

BOOST_AUTO_TEST_CASE( missing_release_keeps_releasing_until_engine_confirms )
{
	bot_sfj_phase_t phase = BotSFJ_ReleasedNextPhase( BOT_SFJ_PHASE_AIR, 1, 500, 0 );
	BOOST_CHECK_EQUAL( phase, BOT_SFJ_PHASE_LANDING );
	BOOST_CHECK( !BotSFJ_JumpPressed( phase, 1 ) );
	BOOST_CHECK_EQUAL( BotSFJ_ReleasedNextPhase( phase, 1, 100, 0 ), phase );
	BOOST_CHECK_EQUAL( BotSFJ_ReleasedNextPhase( phase, 1, 0, 1 ), BOT_SFJ_PHASE_REJUMP );
}

BOOST_AUTO_TEST_CASE( halfbeat_strafe_key_matches_turn_side_for_both_styles )
{
	for (int side = -1; side <= 1; side += 2)
	{
		for (int only = 0; only < 2; ++only)
		{
			int forward, right;
			BotSFJ_HalfBeatAxes( side, only, &forward, &right );
			BOOST_CHECK_EQUAL( forward, only ? 0 : 127 );
			/* Left turn (side > 0) holds A, right turn holds D, like humans. */
			BOOST_CHECK_EQUAL( right, side > 0 ? -127 : 127 );
		}
	}
}

BOOST_AUTO_TEST_CASE( strafe_view_stays_near_velocity_like_humans )
{
	/* Humans look within 10-25 degrees of their velocity; the old key mapping put
	 * the bot's view 105-150 degrees off it (the "random backwards strafe"). */
	for (int side = -1; side <= 1; side += 2)
	{
		for (int only = 0; only < 2; ++only)
		{
			for (int speed = only ? 400 : 300; speed <= 700; speed += 50)
			{
				int forward, right;
				const float wishYaw = BotSFJ_CommandYaw( 0.0f, (float)speed, 0.0f,
					250.0f, 1.0f, 0.008f, side );
				BotSFJ_HalfBeatAxes( side, only, &forward, &right );
				const float viewYaw = BotSFJ_ViewYawForAxes( wishYaw, forward, right );
				BOOST_CHECK_LT( fabsf( BotSFJ_AngleDelta( viewYaw, 0.0f ) ), 40.0f );
				/* A left turn (side > 0) puts the wish left of the velocity. */
				BOOST_CHECK( side > 0 ? BotSFJ_AngleDelta( wishYaw, 0.0f ) > 0.0f :
					BotSFJ_AngleDelta( wishYaw, 0.0f ) < 0.0f );
			}
		}
	}
}

BOOST_AUTO_TEST_CASE( backward_strafe_view_faces_back_along_route )
{
	for (int side = -1; side <= 1; side += 2)
	{
		int forward, right;
		const float wishYaw = BotSFJ_CommandYaw( 0.0f, 450.0f, 0.0f, 250.0f, 1.0f, 0.008f, side );
		BotSFJ_HalfBeatAxes( side, 0, &forward, &right );
		BotSFJ_BackwardAxes( &forward, &right );
		const float viewYaw = BotSFJ_ViewYawForAxes( wishYaw, forward, right );
		BOOST_CHECK_GT( fabsf( BotSFJ_AngleDelta( viewYaw, 0.0f ) ), 140.0f );
	}
}

BOOST_AUTO_TEST_CASE( human_wish_offset_is_wider_than_optimum_and_capped )
{
	BOOST_CHECK_CLOSE( BotSFJ_HumanWishOffset( 400.0f ), 63.5f, 0.1f );
	BOOST_CHECK_CLOSE( BotSFJ_HumanWishOffset( 5000.0f ), 72.0f, 0.1f );
	BOOST_CHECK( BotSFJ_HumanWishOffset( 300.0f ) < BotSFJ_HumanWishOffset( 500.0f ) );
	for (int speed = 300; speed <= 700; speed += 100)
	{
		const float optimum = BotSFJ_OptimalWishOffset( (float)speed, 250.0f, 1.0f, 0.033f );
		const float yaw = BotSFJ_CommandYaw( 0.0f, (float)speed, 0.0f, 250.0f, 1.0f, 0.033f, 1 );
		BOOST_CHECK_GE( yaw, optimum - 0.01f );
		BOOST_CHECK_LE( yaw, BOT_SFJ_WISH_OFFSET_MAX + 0.01f );
	}
	/* Always inside the controller's 0.25 input/intent alignment gate. */
	BOOST_CHECK_GT( cosf( BOT_SFJ_ROUTE_CLAMP * 0.017453292519943295f ), 0.25f );
}

BOOST_AUTO_TEST_CASE( rate_limited_view_picks_keys_closest_to_wish )
{
	int forward = 0, right = 0;
	/* View already at the target: the hop's own style is chosen. */
	const float wishYaw = 60.0f;
	int f0, r0;
	BotSFJ_HalfBeatAxes( 1, 0, &f0, &r0 );
	const float target = BotSFJ_ViewYawForAxes( wishYaw, f0, r0 );
	BOOST_CHECK_SMALL( BotSFJ_PickAxesForView( target, wishYaw, 1, 0, 0, &forward, &right ), 0.01f );
	BOOST_CHECK_EQUAL( forward, f0 );
	BOOST_CHECK_EQUAL( right, r0 );
	/* View lagging toward the wish: forward only (the human W-only frame). */
	BotSFJ_PickAxesForView( wishYaw, wishYaw, 1, 0, 0, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 127 );
	BOOST_CHECK_EQUAL( right, 0 );
	/* View lagging the other way: side-only on the same side. */
	BotSFJ_PickAxesForView( wishYaw - 90.0f, wishYaw, 1, 0, 0, &forward, &right );
	BOOST_CHECK_EQUAL( forward, 0 );
	BOOST_CHECK_EQUAL( right, -127 );
	/* Backward mirrors every candidate. */
	BotSFJ_PickAxesForView( wishYaw + 180.0f, wishYaw, 1, 0, 1, &forward, &right );
	BOOST_CHECK_EQUAL( forward, -127 );
	BOOST_CHECK_EQUAL( right, 0 );
}

BOOST_AUTO_TEST_CASE( strafe_view_yaw_rate_follows_human_profile_and_skill )
{
	BOOST_CHECK_EQUAL( BotSFJ_ViewYawRate( 3.0f, 1, 0, 0.0f, 0.0f ), 0.0f );
	BOOST_CHECK_EQUAL( BotSFJ_ViewYawRate( 10.0f, 0, 50, 0.0f, 0.0f ), 0.0f );
	BOOST_CHECK( BotSFJ_ViewYawRate( 2.0f, 0, 50, 0.0f, 0.0f ) <
		BotSFJ_ViewYawRate( 8.0f, 0, 50, 0.0f, 0.0f ) );
	BOOST_CHECK( BotSFJ_ViewYawRate( 5.0f, 0, 400, 0.0f, 0.0f ) <
		BotSFJ_ViewYawRate( 5.0f, 0, 50, 0.0f, 0.0f ) );
	BOOST_CHECK_CLOSE( BotSFJ_ViewYawRate( 5.0f, 0, 400, 0.0f, 0.0f ), 90.0f, 0.1f );
	BOOST_CHECK_CLOSE( BotSFJ_ViewYawRate( 5.0f, 0, 400, 0.0f, 100.0f ), 200.0f, 0.1f );
	BOOST_CHECK_CLOSE( BotSFJ_ViewYawRate( 5.0f, 0, 400, 0.0f, 900.0f ), 240.0f, 0.1f );
	BOOST_CHECK_CLOSE( BotSFJ_ViewYawRate( 5.0f, 0, 50, 2000.0f, 0.0f ), 720.0f, 0.1f );
	BOOST_CHECK( BotSFJ_ViewYawRate( std::numeric_limits<float>::quiet_NaN(), 0, 400, 0.0f,
		std::numeric_limits<float>::quiet_NaN() ) > 0.0f );
}

BOOST_AUTO_TEST_CASE( hop_style_keeps_side_and_uses_full_beat_when_slow )
{
	BOOST_CHECK( !BotSFJ_HopStrafeOnly( 350.0f, 0, -1 ) );
	BOOST_CHECK( BotSFJ_HopStrafeOnly( 500.0f, 0, -1 ) );
	BOOST_CHECK( !BotSFJ_HopStrafeOnly( 500.0f, 1, -1 ) );
	BOOST_CHECK( BotSFJ_HopStrafeOnly( 450.0f, 1, 80 ) );
	BOOST_CHECK( !BotSFJ_HopStrafeOnly( 450.0f, 0, 10 ) );
	BOOST_CHECK( !BotSFJ_HopStrafeOnly( 250.0f, 0, 80 ) );
	/* Route left of the velocity: keep turning left across the landing. */
	BOOST_CHECK_EQUAL( BotSFJ_HopSide( 40.0f, 500.0f, 0.0f, 1, 0 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_HopSide( 40.0f, 500.0f, 0.0f, 1, 1 ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_HopSide( -40.0f, 500.0f, 0.0f, 1, 0 ), -1 );
	/* Near the route the human's key side decides: D (+1) turns right (-1). */
	BOOST_CHECK_EQUAL( BotSFJ_HopSide( 5.0f, 500.0f, 0.0f, 1, 1 ), -1 );
	BOOST_CHECK_EQUAL( BotSFJ_HopSide( 5.0f, 500.0f, 0.0f, -1, -1 ), 1 );
}

BOOST_AUTO_TEST_CASE( air_side_switch_follows_recorded_fraction_once )
{
	int switched = 0;
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( 1, 0.0f, 500.0f, 0.0f, 200, 1000, 45, &switched ), 1 );
	BOOST_CHECK_EQUAL( switched, 0 );
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( 1, 0.0f, 500.0f, 0.0f, 460, 1000, 45, &switched ), -1 );
	BOOST_CHECK_EQUAL( switched, 1 );
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( -1, 0.0f, 500.0f, 0.0f, 600, 1000, 45, &switched ), -1 );
	switched = 0;
	/* The route bending the other way switches regardless of the recording. */
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( 1, -20.0f, 500.0f, 0.0f, 100, 0, 0, &switched ), -1 );
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( -1, 20.0f, 500.0f, 0.0f, 100, 0, 0, &switched ), 1 );
	BOOST_CHECK_EQUAL( BotSFJ_AirSideSwitch( 1, -5.0f, 500.0f, 0.0f, 100, 0, 0, &switched ), 1 );
}

BOOST_AUTO_TEST_CASE( escape_never_turns_back_on_close_visible_enemy )
{
	BOOST_CHECK( !BotSFJ_EscapeFacingAllows( 0, 1, 300.0f, 0.0f ) );
	BOOST_CHECK( BotSFJ_EscapeFacingAllows( 1, 1, 300.0f, 0.0f ) );
	BOOST_CHECK( BotSFJ_EscapeFacingAllows( 0, 0, 300.0f, 0.0f ) );
	BOOST_CHECK( BotSFJ_EscapeFacingAllows( 0, 1, 400.0f, 0.0f ) );
	BOOST_CHECK( !BotSFJ_EscapeFacingAllows( 0, 1, 400.0f, 448.0f ) );
}

BOOST_AUTO_TEST_CASE( style_accumulator_measures_human_hop )
{
	bot_sfj_style_accum_t acc;
	bot_sfj_hop_style_t style;
	BotSFJ_StyleAccumReset( &acc );
	/* 1000 ms hop: WD flick at takeoff, D only, then W, then WA from 500 ms. */
	for (int t = 0; t < 1000; t += 8)
	{
		const int fwd = (t >= 200 && t < 480) ? 0 : 127;
		const int right = t < 480 ? 127 : (t < 500 ? 0 : -127);
		BotSFJ_StyleAccumAdd( &acc, 8, fwd, right, t < 16 ? 2.0f : 0.4f );
	}
	BotSFJ_StyleAccumFinish( &acc, &style );
	BOOST_CHECK_EQUAL( style.firstSide, 1 );
	BOOST_CHECK_EQUAL( style.switchPct, 50 );
	BOOST_CHECK( style.sideOnlyPct > 20 && style.sideOnlyPct < 35 );
	BOOST_CHECK_CLOSE( style.takeoffRate, 250.0f, 0.1f );
	BOOST_CHECK_CLOSE( style.airRate, 50.0f, 0.5f );
	BotSFJ_StyleAccumReset( &acc );
	BotSFJ_StyleAccumFinish( &acc, &style );
	BOOST_CHECK_EQUAL( style.sideOnlyPct, -1 );
	BOOST_CHECK_EQUAL( style.firstSide, 0 );
}

BOOST_AUTO_TEST_CASE( recorded_force_jump_is_measured_and_copied )
{
	bot_sfj_style_accum_t acc;
	bot_sfj_hop_style_t style;
	BotSFJ_StyleAccumReset( &acc );
	for (int t = 0; t < 800; t += 8)
	{
		BotSFJ_StyleAccumAdd( &acc, 8, 127, 127, 0.3f );
		BotSFJ_StyleAccumForce( &acc, t > 100 && t < 300, t < 400 ? t * 0.4f : 160.0f - (t - 400) * 0.4f );
	}
	BotSFJ_StyleAccumFinish( &acc, &style );
	BOOST_CHECK_EQUAL( style.forceJump, 1 );
	BOOST_CHECK_CLOSE( style.forceHeight, 160.0f, 0.5f );
	BotSFJ_StyleAccumReset( &acc );
	BotSFJ_StyleAccumAdd( &acc, 8, 127, 127, 0.3f );
	BotSFJ_StyleAccumForce( &acc, 0, 40.0f );
	BotSFJ_StyleAccumFinish( &acc, &style );
	BOOST_CHECK_EQUAL( style.forceJump, 0 );
	BOOST_CHECK_EQUAL( style.forceHeight, 0.0f );

	BOOST_CHECK( BotSFJ_ForceJumpHold( 1, 160.0f, 1, 0, 50.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 1, 160.0f, 1, 0, 160.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 1, 160.0f, 1, 0, 50.0f, -10.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 1, 160.0f, 0, 0, 50.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 0, 160.0f, 1, 0, 50.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 1, 160.0f, 1, 1, 50.0f, 200.0f ) );
	BOOST_CHECK( !BotSFJ_ForceJumpHold( 1, 160.0f, 1, 0, std::numeric_limits<float>::quiet_NaN(), 200.0f ) );
}

BOOST_AUTO_TEST_CASE( failing_routes_decay_and_lose_export_rank )
{
	BOOST_CHECK_EQUAL( BotSFJ_TrackFailureDecay( 3, 0 ), 1.0f );
	BOOST_CHECK_EQUAL( BotSFJ_TrackFailureDecay( 20, 1 ), 1.0f );
	BOOST_CHECK( BotSFJ_TrackFailureDecay( 8, 0 ) < BotSFJ_TrackFailureDecay( 5, 0 ) );
	BOOST_CHECK_GE( BotSFJ_TrackFailureDecay( 1000, 0 ), 0.05f );
	BOOST_CHECK_EQUAL( BotSFJ_TrackExportFactor( 2, 0, 1.0f ), 1.0f );
	BOOST_CHECK( BotSFJ_TrackExportFactor( 10, 9, 1.0f ) > BotSFJ_TrackExportFactor( 10, 2, 1.0f ) );
	BOOST_CHECK( BotSFJ_TrackExportFactor( 10, 0, 1.0f ) < BotSFJ_TrackExportFactor( 10, 1, 1.0f ) );
	BOOST_CHECK_GE( BotSFJ_TrackExportFactor( 500, 0, 0.1f ), 0.05f );
	BOOST_CHECK_LE( BotSFJ_TrackExportFactor( 500, 500, 99.0f ), 3.0f );
	/* A fast human route the bots never finish loses its pick score to a finished one. */
	BOOST_CHECK( BotSFJ_TrackRouteScore( 3000.0f, 800.0f, 12, 0, 0.0f, 256.0f ) <
		BotSFJ_TrackRouteScore( 1500.0f, 500.0f, 12, 10, 0.0f, 256.0f ) );
}

BOOST_AUTO_TEST_CASE( emitted_halfbeat_axes_match_predictor_world_wish_direction )
{
	for (int side = -1; side <= 1; side += 2)
	{
		for (int only = 0; only < 2; ++only)
		{
			for (int yaw = -180; yaw <= 180; yaw += 30)
			{
				int forward, right;
				float x, y;
				const float wishYaw = BotSFJ_CommandYaw( (float)yaw, 550.0f, 125.0f,
					250.0f, 1.0f, 0.008f, side );
				BotSFJ_HalfBeatAxes( side, only, &forward, &right );
				const float viewYaw = BotSFJ_ViewYawForAxes( wishYaw, forward, right );
				BotSFJ_EffectiveMovement( 0, 0, 400, viewYaw, 1, forward, 1, right, &x, &y );
				BOOST_CHECK_SMALL( x - cosf( wishYaw * 0.017453292519943295f ), 0.00001f );
				BOOST_CHECK_SMALL( y - sinf( wishYaw * 0.017453292519943295f ), 0.00001f );
			}
		}
	}
}

BOOST_AUTO_TEST_CASE( curved_lookahead_accepts_gentle_bends_not_hairpins )
{
	BOOST_CHECK( BotSFJ_CurveSegmentAllows( 0.95f, 0.85f ) );
	BOOST_CHECK( BotSFJ_CurveSegmentAllows( 0.8f, 0.6f ) );
	BOOST_CHECK( !BotSFJ_CurveSegmentAllows( 0.7f, 0.9f ) );
	BOOST_CHECK( !BotSFJ_CurveSegmentAllows( 1.0f, 0.49f ) );
	BOOST_CHECK( !BotSFJ_CurveSegmentAllows( std::numeric_limits<float>::quiet_NaN(), 1 ) );
}

BOOST_AUTO_TEST_CASE( curved_advancement_still_requires_links_heights_and_endpoint )
{
	BOOST_CHECK( BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, -60, -80, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 1, 1, 1, 0, 0, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 0, 1, 0, 0, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 0, 0, 0, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, -101, 0, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, 0, -101, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, 33, 0, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, 0, 65, 0.75f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, 0, 0, 0.49f, 10, 100 ) );
	BOOST_CHECK( !BotSFJ_AdvanceCurveLinkAllows( 0, 1, 1, 0, 0, 0.75f, -1, 100 ) );
}

BOOST_AUTO_TEST_CASE( chain_or_wall_slide_cannot_bypass_endpoint_bound )
{
	BOOST_CHECK( BotSFJ_EndpointAllows( 0, 0, 200, 0, 250, 80, 96 ) );
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 200, 0, 297, 0, 96 ) );
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 200, 0, -1, 0, 96 ) );
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 0, 0, 0, 0, 96 ) );
	BOOST_CHECK( !BotSFJ_EndpointAllows( 0, 0, 200, 0,
		std::numeric_limits<float>::quiet_NaN(), 0, 96 ) );
}

BOOST_AUTO_TEST_CASE( required_curve_endpoint_uses_terminal_plane_without_slack )
{
	BOOST_CHECK( BotSFJ_EndpointPlaneAllows( 100, 0, 200, 100, 50, 0, 0 ) );
	BOOST_CHECK( BotSFJ_EndpointPlaneAllows( 100, 0, 200, 100, 200, 100, 0 ) );
	BOOST_CHECK( !BotSFJ_EndpointPlaneAllows( 100, 0, 200, 100, 200, 101, 0 ) );
	BOOST_CHECK( BotSFJ_EndpointPlaneAllows( 100, 0, 200, 100, 200, 101, 96 ) );
	BOOST_CHECK( !BotSFJ_EndpointPlaneAllows( 100, 0, 100, 0, 100, 0, 0 ) );
}

BOOST_AUTO_TEST_CASE( curved_endpoint_counterexample_cannot_escape_terminal_segment )
{
	const float points[3][3] = {{0, 0, 0}, {200, 0, 0}, {300, 100, 0}};
	BOOST_CHECK( BotSFJ_EndpointAllows( 0, 0, 300, 100, 225, 375, 96 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, 3, 225, 375, 96 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, 3, 225, 375, 0 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, 3, 301, 100, 0 ) );
	BOOST_CHECK( BotSFJ_PolylineEndpointAllows( points, 3, 250, 50, 0 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, 1, 250, 50, 0 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, BOT_SFJ_MAX_CORRIDOR_POINTS + 1, 250, 50, 0 ) );
	BOOST_CHECK( BotSFJ_EndpointAllows( 0, 0, 300, 100, 225, 250, 0 ) );
	BOOST_CHECK( !BotSFJ_PolylineEndpointAllows( points, 3, 225, 250, 0 ) );
}

BOOST_AUTO_TEST_SUITE_END()
