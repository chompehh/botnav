#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

namespace {
std::string MenuSource(const char *path)
{
	std::ifstream file(std::string(BOTNAV_SOURCE_DIR) + "/assets/japro/" + path);
	BOOST_REQUIRE_MESSAGE(file.good(), "Missing packaged menu: " << path);
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string ProfileButton(const std::string &menu, const char *name)
{
	std::smatch match;
	const std::regex pattern(std::string("itemDef\\s*\\{\\s*name\\s+\"?") +
		name + "\"?\\s+([\\s\\S]*?)\\baction\\s*\\{([\\s\\S]*?)\\}");
	BOOST_REQUIRE_MESSAGE(std::regex_search(menu, match, pattern), "Missing Profile button: " << name);
	return match[0].str();
}

bool ButtonVisible(const std::string &button, int gametype)
{
	std::smatch match;
	if (!std::regex_search(button, match, std::regex("cvarTest\\s+\"ui_about_gametype\"",
		std::regex::icase)))
		return true;
	const std::string mode = "\"" + std::to_string(gametype) + "\"";
	if (std::regex_search(button, match, std::regex("hideCvar\\s*\\{([^}]*)\\}",
		std::regex::icase)))
		return match[1].str().find(mode) == std::string::npos;
	if (std::regex_search(button, match, std::regex("showCvar\\s*\\{([^}]*)\\}",
		std::regex::icase)))
		return match[1].str().find(mode) != std::string::npos;
	return true;
}
}

BOOST_AUTO_TEST_SUITE(ui_profile)

BOOST_AUTO_TEST_CASE(custom_modes_keep_both_profile_entry_points)
{
	const std::string menu = MenuSource("ui/jamp/ingame.menu");
	for (const char *name : {"player", "account"}) {
		const std::string button = ProfileButton(menu, name);
		for (int mode : {10, 11, 12})
			BOOST_CHECK_MESSAGE(ButtonVisible(button, mode), name << " hidden in mode " << mode);
		BOOST_CHECK(std::regex_search(button, std::regex("visible\\s+1\\b")));
		BOOST_CHECK(std::regex_search(button, std::regex("uiScript\\s+getcharcvars")));
		BOOST_CHECK(std::regex_search(button, std::regex("open\\s+ingame_player\\b")));
	}
	BOOST_CHECK(!ButtonVisible(ProfileButton(menu, "player"), 7));
}

BOOST_AUTO_TEST_CASE(profile_and_force_menus_are_loaded_and_keep_apply_join_actions)
{
	const std::string manifest = MenuSource("ui/jampingame.txt");
	for (const char *path : {"ui/jamp/ingame.menu", "ui/jamp/ingame_player.menu",
		"ui/jamp/ingame_playerforce.menu"}) {
		BOOST_CHECK_NE(manifest.find(path), std::string::npos);
		BOOST_CHECK(!MenuSource(path).empty());
	}
	const std::string profile = MenuSource("ui/jamp/ingame_player.menu");
	for (const char *name : {"applycurrent", "applyjoin", "applyred", "applyblue"}) {
		const std::string button = ProfileButton(profile, name);
		BOOST_CHECK(std::regex_search(button, std::regex("uiScript\\s+setForce")));
		if (std::string(name) == "applyred" || std::string(name) == "applyblue") {
			BOOST_CHECK(ButtonVisible(button, 11));
			BOOST_CHECK(!ButtonVisible(button, 10));
			BOOST_CHECK(!ButtonVisible(button, 12));
		} else {
			for (int mode : {10, 11, 12})
				BOOST_CHECK(ButtonVisible(button, mode));
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
