#include "g_duel_tutorial_lessons.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cctype>
#include <set>
#include <string>

BOOST_AUTO_TEST_SUITE(duel_tutorial_curriculum)

BOOST_AUTO_TEST_CASE(catalog_is_complete_unique_and_fits_chat)
{
	std::set<std::string> messages;
	duel_curriculum_inputs_t inputs = {};
	char rendered[150];
	int tiers[2] = {};
	int armorBypass = 0;
	bool ptk = false, drainlock = false, antiDrain = false, rageEscape = false;
	inputs.validMinimums = (1u << DUEL_CURRICULUM_POWER_COUNT) - 1;
	inputs.validActionCosts = inputs.validMinimums;
	inputs.validValues = (1u << DUEL_CURRICULUM_VALUE_COUNT) - 1;
	for (int i = 0; i < DUEL_CURRICULUM_POWER_COUNT; ++i)
		inputs.minimumFP[i] = inputs.actionCost[i] = 999;
	for (int i = 0; i < DUEL_CURRICULUM_VALUE_COUNT; ++i)
		inputs.values[i] = 999;
	BOOST_REQUIRE_EQUAL(DUEL_CURRICULUM_COUNT, 300u);
	for (size_t i = 0; i < DUEL_CURRICULUM_COUNT; ++i)
	{
		const duel_curriculum_lesson_t &lesson = g_duelCurriculumLessons[i];
		BOOST_REQUIRE(lesson.advanced == 0 || lesson.advanced == 1);
		++tiers[lesson.advanced];
		BOOST_CHECK(messages.insert(lesson.text).second);
		BOOST_REQUIRE(G_DuelCurriculumRender(&lesson, &inputs, rendered, sizeof(rendered)));
		BOOST_CHECK_LT(std::string(rendered).size(), 150u);
		BOOST_CHECK(std::string(rendered).find("when safe") == std::string::npos);
		BOOST_CHECK(std::string(rendered).find("when possible") == std::string::npos);
		std::string text(rendered);
		std::transform(text.begin(), text.end(), text.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		for (const char *phrase : {"server", "setting", "rules", "push then pull",
				"pull then push", "speed and push", "speed plus push", "safe jump",
				"face your opponent", "aim at the opponent", "adjust your aim",
				"adjust aim", "level 2 saber throw", "level 2 throw",
				"throw angle", "move a target into a flying saber", "current"})
			BOOST_CHECK_MESSAGE(text.find(phrase) == std::string::npos, text);
		if (text.find("bypass") != std::string::npos)
			++armorBypass;
		ptk |= text.find("ptk") != std::string::npos;
		drainlock |= text.find("drainlock") != std::string::npos;
		antiDrain |= text.find("anti-drain") != std::string::npos;
		rageEscape |= text.find("rage") != std::string::npos && text.find("escape") != std::string::npos;
		BOOST_CHECK_EQUAL(lesson.level2Powers & ~lesson.requiredPowers, 0u);
		BOOST_CHECK_EQUAL(lesson.level3Powers & ~lesson.level2Powers, 0u);
	}
	BOOST_CHECK_EQUAL(tiers[0], 150);
	BOOST_CHECK_EQUAL(tiers[1], 150);
	BOOST_CHECK_EQUAL(armorBypass, 1);
	BOOST_CHECK(ptk && drainlock && antiDrain && rageEscape);
	BOOST_CHECK(std::string(g_duelCurriculumLessons[1].text).find("level 3 Push, Pull and Saber Throw") != std::string::npos);
	BOOST_CHECK(std::string(g_duelCurriculumLessons[2].text).find("stun") != std::string::npos);
	BOOST_CHECK(std::string(g_duelCurriculumLessons[3].text).find("stun") != std::string::npos);
	BOOST_CHECK(std::string(g_duelCurriculumLessons[4].text).find("longer") != std::string::npos);
	BOOST_CHECK(std::string(g_duelCurriculumLessons[5].text).find("Below 20 FP") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(combo_examples_require_live_components_and_show_base_math)
{
	duel_curriculum_inputs_t inputs = {};
	char message[150];
	const duel_curriculum_lesson_t *pair = NULL, *ptk = NULL;
	for (size_t i = 0; i < DUEL_CURRICULUM_COUNT; ++i)
	{
		if (g_duelCurriculumLessons[i].format == DUEL_CURRICULUM_PAIR_THROW_DAMAGE)
			pair = &g_duelCurriculumLessons[i];
		if (g_duelCurriculumLessons[i].format == DUEL_CURRICULUM_PTK_EXAMPLE)
			ptk = &g_duelCurriculumLessons[i];
	}
	BOOST_REQUIRE(pair && ptk);
	inputs.validActionCosts = (1u << DUEL_CURRICULUM_PULL) | (1u << DUEL_CURRICULUM_THROW);
	inputs.actionCost[DUEL_CURRICULUM_PULL] = 20;
	inputs.actionCost[DUEL_CURRICULUM_THROW] = 20;
	BOOST_CHECK(!G_DuelCurriculumRender(pair, &inputs, message, sizeof(message)));
	inputs.validValues = 1u << DUEL_VALUE_THROW_BASE_DAMAGE;
	inputs.values[DUEL_VALUE_THROW_BASE_DAMAGE] = 30;
	BOOST_REQUIRE(G_DuelCurriculumRender(pair, &inputs, message, sizeof(message)));
	BOOST_CHECK(std::string(message).find("40 FP") != std::string::npos);
	BOOST_CHECK(std::string(message).find("30 base") != std::string::npos);
	BOOST_CHECK(!G_DuelCurriculumRender(ptk, &inputs, message, sizeof(message)));
	inputs.validValues |= 1u << DUEL_VALUE_FLIPKICK_BASE_DAMAGE;
	inputs.values[DUEL_VALUE_FLIPKICK_BASE_DAMAGE] = 20;
	BOOST_REQUIRE(G_DuelCurriculumRender(ptk, &inputs, message, sizeof(message)));
	BOOST_CHECK(std::string(message).find("80 damage") != std::string::npos);
	inputs.values[DUEL_VALUE_FLIPKICK_BASE_DAMAGE] = -1;
	BOOST_CHECK(!G_DuelCurriculumRender(ptk, &inputs, message, sizeof(message)));
}

BOOST_AUTO_TEST_CASE(all_required_powers_and_levels_are_checked)
{
	duel_curriculum_lesson_t lesson = {
		"test", 1, 1, (1u << DUEL_CURRICULUM_PUSH) | (1u << DUEL_CURRICULUM_THROW),
		1u << DUEL_CURRICULUM_THROW, 0, DUEL_CURRICULUM_LITERAL, -1, -1
	};
	int levels[DUEL_CURRICULUM_POWER_COUNT] = {};
	unsigned int both = lesson.requiredPowers;
	levels[DUEL_CURRICULUM_PUSH] = 1;
	levels[DUEL_CURRICULUM_THROW] = 2;
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 0, both, levels));
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 1, 1u << DUEL_CURRICULUM_PUSH, levels));
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 1, both, NULL));
	levels[DUEL_CURRICULUM_THROW] = 1;
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 1, both, levels));
	levels[DUEL_CURRICULUM_THROW] = 2;
	BOOST_CHECK(G_DuelCurriculumAllowed(&lesson, 1, both, levels));
	lesson.level3Powers = 1u << DUEL_CURRICULUM_THROW;
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 1, both, levels));
	levels[DUEL_CURRICULUM_THROW] = 3;
	BOOST_CHECK(G_DuelCurriculumAllowed(&lesson, 1, both, levels));
	lesson.requiredPowers = 0;
	BOOST_CHECK(!G_DuelCurriculumAllowed(&lesson, 1, both, NULL));
}

BOOST_AUTO_TEST_CASE(live_minimum_rendering_requires_valid_input_and_buffer)
{
	const duel_curriculum_lesson_t minimum = {
		"Jump threshold %d FP.", 0, 1, 1u << DUEL_CURRICULUM_JUMP, 0, 0,
		DUEL_CURRICULUM_MINIMUM_FP, DUEL_CURRICULUM_JUMP, -1
	};
	const duel_curriculum_lesson_t *lesson = &minimum;
	duel_curriculum_inputs_t inputs = {};
	char message[150], small[10];
	BOOST_REQUIRE(lesson);
	BOOST_CHECK(!G_DuelCurriculumRender(lesson, NULL, message, sizeof(message)));
	BOOST_CHECK(!G_DuelCurriculumRender(lesson, &inputs, message, sizeof(message)));
	inputs.validMinimums = 1u << DUEL_CURRICULUM_JUMP;
	inputs.minimumFP[DUEL_CURRICULUM_JUMP] = 25;
	BOOST_REQUIRE(G_DuelCurriculumRender(lesson, &inputs, message, sizeof(message)));
	BOOST_CHECK(std::string(message).find("25 FP") != std::string::npos);
	BOOST_CHECK(!G_DuelCurriculumRender(lesson, &inputs, small, sizeof(small)));
	BOOST_CHECK_EQUAL(small[0], '\0');
	inputs.minimumFP[DUEL_CURRICULUM_JUMP] = -1;
	BOOST_CHECK(!G_DuelCurriculumRender(lesson, &inputs, message, sizeof(message)));
	inputs.minimumFP[DUEL_CURRICULUM_JUMP] = 1000000;
	BOOST_CHECK(!G_DuelCurriculumRender(lesson, &inputs, message, sizeof(message)));
}

BOOST_AUTO_TEST_CASE(costs_and_values_are_typed_and_independently_gated)
{
	duel_curriculum_inputs_t inputs = {};
	char message[150];
	duel_curriculum_lesson_t start = {
		"Need %d FP, spend %d FP.", 0, 1, 1u << DUEL_CURRICULUM_HEAL, 0, 0,
		DUEL_CURRICULUM_THRESHOLD_AND_COST, DUEL_CURRICULUM_HEAL, -1
	};
	duel_curriculum_lesson_t pair = {
		"Spend %d plus %d = %d.", 1, 1,
		(1u << DUEL_CURRICULUM_PUSH) | (1u << DUEL_CURRICULUM_PULL), 0, 0,
		DUEL_CURRICULUM_TWO_COSTS_SUM, DUEL_CURRICULUM_PUSH, DUEL_CURRICULUM_PULL
	};
	duel_curriculum_lesson_t heal = {
		"Spend %d FP for up to %d HP.", 0, 1, 1u << DUEL_CURRICULUM_HEAL, 0, 0,
		DUEL_CURRICULUM_COST_AND_VALUE, DUEL_CURRICULUM_HEAL, DUEL_VALUE_HEAL_GAIN
	};
	inputs.validMinimums = 1u << DUEL_CURRICULUM_HEAL;
	inputs.minimumFP[DUEL_CURRICULUM_HEAL] = 60;
	BOOST_CHECK(!G_DuelCurriculumRender(&start, &inputs, message, sizeof(message)));
	inputs.validActionCosts = 1u << DUEL_CURRICULUM_HEAL;
	inputs.actionCost[DUEL_CURRICULUM_HEAL] = 50;
	BOOST_REQUIRE(G_DuelCurriculumRender(&start, &inputs, message, sizeof(message)));
	BOOST_CHECK_EQUAL(std::string(message), "Need 60 FP, spend 50 FP.");
	BOOST_CHECK(!G_DuelCurriculumRender(&heal, &inputs, message, sizeof(message)));
	inputs.validValues = 1u << DUEL_VALUE_HEAL_GAIN;
	inputs.values[DUEL_VALUE_HEAL_GAIN] = 25;
	BOOST_REQUIRE(G_DuelCurriculumRender(&heal, &inputs, message, sizeof(message)));
	BOOST_CHECK_EQUAL(std::string(message), "Spend 50 FP for up to 25 HP.");
	inputs.validActionCosts |= 1u << DUEL_CURRICULUM_PUSH;
	inputs.actionCost[DUEL_CURRICULUM_PUSH] = 20;
	inputs.actionCost[DUEL_CURRICULUM_PULL] = 60;
	BOOST_CHECK(!G_DuelCurriculumRender(&pair, &inputs, message, sizeof(message)));
	inputs.validActionCosts |= 1u << DUEL_CURRICULUM_PULL;
	BOOST_REQUIRE(G_DuelCurriculumRender(&pair, &inputs, message, sizeof(message)));
	BOOST_CHECK_EQUAL(std::string(message), "Spend 20 plus 60 = 80.");
	inputs.values[DUEL_VALUE_HEAL_GAIN] = -1;
	BOOST_CHECK(!G_DuelCurriculumRender(&heal, &inputs, message, sizeof(message)));
}

BOOST_AUTO_TEST_CASE(selection_introduces_then_mixes_eligible_renderable_tiers)
{
	int levels[DUEL_CURRICULUM_POWER_COUNT];
	duel_curriculum_inputs_t inputs = {};
	char output[150];
	for (int i = 0; i < DUEL_CURRICULUM_POWER_COUNT; ++i)
		levels[i] = 3;
	inputs.validMinimums = inputs.validActionCosts = (1u << DUEL_CURRICULUM_POWER_COUNT) - 1;
	inputs.validValues = (1u << DUEL_CURRICULUM_VALUE_COUNT) - 1;
	for (int i = 0; i < DUEL_CURRICULUM_POWER_COUNT; ++i)
		inputs.minimumFP[i] = inputs.actionCost[i] = 20;
	for (int i = 0; i < DUEL_CURRICULUM_VALUE_COUNT; ++i)
		inputs.values[i] = 4;
	inputs.values[DUEL_VALUE_DRAIN_SHOT_COST] = 5;
	for (int rotation = 0; rotation < 40; ++rotation)
	{
		int selected = G_DuelCurriculumSelect(rotation, 1, ~0u, levels, &inputs, output, sizeof(output));
		BOOST_REQUIRE_GE(selected, 0);
		BOOST_CHECK_EQUAL(g_duelCurriculumLessons[selected].advanced,
			rotation < DUEL_CURRICULUM_INTRO_COUNT ? 0 : (rotation - DUEL_CURRICULUM_INTRO_COUNT) % 2);
		if (rotation < DUEL_CURRICULUM_INTRO_COUNT)
			BOOST_CHECK_EQUAL(selected, rotation);
		BOOST_CHECK(output[0]);
	}
	BOOST_REQUIRE_EQUAL(G_DuelCurriculumSelect(22, 1, ~0u, levels, &inputs, output, sizeof(output)), 22);
	BOOST_CHECK(std::string(output).find("costs 5 FP") != std::string::npos);
	BOOST_CHECK(std::string(output).find("heal up to 4 HP") != std::string::npos);
	BOOST_CHECK(std::string(output).find("remove up to 4 enemy FP") != std::string::npos);
	inputs.validValues &= ~(1u << DUEL_VALUE_DRAIN_TARGET_LOSS);
	BOOST_CHECK(!G_DuelCurriculumRender(&g_duelCurriculumLessons[22], &inputs, output, sizeof(output)));
	BOOST_CHECK_EQUAL(G_DuelCurriculumSelect(22, 1, ~0u, levels, &inputs, output, sizeof(output)), 23);
	BOOST_CHECK_EQUAL(G_DuelCurriculumSelect(3, 1, 0, levels, &inputs, output, sizeof(output)), 11);
	char shortOutput[2];
	BOOST_CHECK_EQUAL(G_DuelCurriculumSelect(0, 0, 0, NULL, NULL, shortOutput, sizeof(shortOutput)), -1);
	BOOST_CHECK_EQUAL(shortOutput[0], '\0');
	BOOST_CHECK_EQUAL(G_DuelCurriculumSelect(-1, 1, ~0u, levels, &inputs, output, sizeof(output)), -1);
}

BOOST_AUTO_TEST_SUITE_END()
