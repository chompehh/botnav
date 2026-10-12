# TaystJK Mod (botnav) - Session Memory Bank

**Repository:** https://github.com/chompehh/botnav  
**Game Engine:** Jedi Academy (Quake 3 / OpenJK / TaystJK / JAPro)  

---

## 1. Gametypes Reference
- **GT 0**: `GT_FFA` (Free for All)
- **GT 1**: `GT_HOLOCRON` (Holocron FFA)
- **GT 2**: `GT_JEDIMASTER` (Jedi Master)
- **GT 3**: `GT_DUEL` (1v1 Tournament)
- **GT 4**: `GT_POWERDUEL` (Power Duel 2v1)
- **GT 5**: `GT_SINGLE_PLAYER` (SP FFA)
- **GT 6**: `GT_TEAM` (Team FFA)
- **GT 7**: `GT_SIEGE` (Siege - Objectives button replaces Profile)
- **GT 8**: `GT_CTF` (Capture The Flag)
- **GT 9**: `GT_CTY` (Capture The Ysalimiri)
- **GT 10**: `GT_ARCADE` (Arcade Survival mode)
- **GT 11**: `GT_JVM` (Jedi vs Merc - Team mode, Merc loadouts, Tank regen)
- **GT 12**: `GT_JOM` (Jedi or Merc - Deathmatch mode with class selection)

---

## 2. Session History

### Session 1: Ingame Menu Profile Submenu Fix (Arcade, JvM, JoM) - [SOLVED]
- **Issue**: Profile submenu option at `rect 145 0 70 32` did not render when running Arcade (10), JvM (11), or JoM (12).
- **Diagnosis**: 
  1. `PC_Script_Parse` in `ui_shared.c` wraps tokens with length > 1 in quotes, turning multi-digit gametypes `"10"`, `"11"`, `"12"` into `""10""`, causing `Item_EnableShowViaCvar` comparisons to fail.
  2. The Profile button is meant to display for all gametypes except Siege (`GT_SIEGE = 7`), which uses Objectives instead.
  3. In `codemp/ui/ui_main.c` `GameType_Parse`, custom gametypes loop omitted `GT_ARCADE` (10).
- **Files Modified**:
  - `assets/japro/ui/jamp/ingame.menu`: Changed `showcvar` allowlist to `hideCvar { "7" }`.
  - `codemp/ui/ui_main.c`: Expanded loop `for (gt = GT_ARCADE; gt <= GT_JOM; gt++)`.

### Session 2: Arcade & JvM/JoM In-Game Workflow & Bot Navigation Tuning - [IN PROGRESS]
- Verifying player setup submenu (`ingame_player.menu`) team join buttons and class abilities.
- Tracking bot navigation, strafe route validation, and arcade lifecycle cleanup.
