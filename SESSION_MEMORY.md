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

### Session 3: JoM Cycleloadout Respawn Fix & Asset PK3 Packaging - [SOLVED]
- **Issue**:
  1. Profile button still missing in Arcade, JvM, JoM on clients using base game pk3s without a dedicated mod pk3 override.
  2. In JoM (`g_gametype 12`), `jom_cycleloadout 1` broke joining after dying, locking the player into spectator permanently. The glitch persisted even after turning it off or switching gametypes.
- **Root Cause**:
  1. The build workflow was copying loose files to `japro/` without creating a `.pk3` archive. Pure servers and standard clients read `ui/jamp/ingame.menu` from `base/assets1.pk3` (which hides Profile for gametypes != 0-6,8). Additionally, `uix86.dll` was only placed in `taystjk/` and not `japro/`.
  2. When cycling loadout on death in JoM, `ClientSpawn` ran with `forceDoInit = 1`. In `WP_InitForcePowers`, non-Arcade gametypes check `warnClient || !sess.setForce`. Because `warnClient` evaluated to true for non-Jedi loadouts (or initial uninitialized `setForce = 0`), the server kicked the player to spectator and sent `spc`.
  3. In `g_cmds.c` `Cmd_Team_f`, JoM did not recognize `"free"`, `"f"`, `"join"`, `"j"`, etc. for spectators attempting to rejoin, and applied a 5-second `switchTeamTime` block.
  4. Once kicked to spectator, `session%i` stored `sessionTeam = 3`, persisting across gametype switches, and invalid forcepower userinfo continued to fail legalization on every respawn attempt.
- **Solution**:
  - `codemp/game/w_force.c`:
    - In `JVM_IsMode(level.gametype)`, never kick players to spectator or trigger profile popup; set `sess.setForce = qtrue`.
    - Automatically update userinfo with the legalized `forcePowers` string when `warnClient` occurs so legalization never fails in an infinite loop.
    - Only offer initial force profile popup if `!sess.setForce` on first connect; never kick an already active player to spectator during gametype transitions.
  - `codemp/game/g_cmds.c`:
    - Allow JoM spectators to join the free team using `"jedi"`, `"merc"`, `"tank"`, `"free"`, `"f"`, `"join"`, `"j"`, `"enter"`, `"auto"`.
    - Bypass `switchTeamTime` penalty for spectators entering the game.
    - Add server console command `/profile` (`Cmd_Profile_f`) sending `spc` to client so the profile menu can always be opened on any client.
  - `codemp/game/g_client.c`:
    - Avoid re-triggering `forceDoInit = 1` during JoM loadout cycling on death; keep `sess.setForce = qtrue`.
  - `codemp/game/g_session.c`:
    - Initialize `sess.setForce = qtrue` for `JVM_IsMode` and `GT_ARCADE` in `G_InitSessionData` and `G_ReadSessionData`.
  - `.github/workflows/build.yml`:
    - Generate `z_japro_assets.pk3` and `z_taystjk_assets.pk3` from `assets/japro/` and ensure UI libraries are available in both `japro/` and `taystjk/` distribution directories.
