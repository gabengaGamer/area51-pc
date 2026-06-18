-- boss_fight.lua  -  F8 spawns Theta + a Military squad and starts the fight; F9 clears it.

local TANK_TYPE    = "NPC - Mutant_Tank"
local SOLDIER_TYPE = "NPC - BlackOpps"

-- NPC types swept by make_everyone_fight(); the boss is skipped.
local FRIENDLY_NPC_TYPES = { "NPC - BlackOpps", "NPC - Hazmat" }

local RAY_DIST     = 110000
local SQUAD_COUNT  = 6
local SQUAD_RADIUS = 1100
local FLOOR_NY_MIN = 0.5

local KEY_SPAWN = "INPUT_KBD_F8"
local KEY_CLEAR = "INPUT_KBD_F9"

-- Theta assets when no live tank exists to copy from (SL4-6 skin + ASCEND3 combat anim).
local TANK_ASSET_DIR = "LEVELS\\CAMPAIGN\\ASCEND3\\RESOURCE"
local TANK_SKIN      = "NPC_TANK_ALIENBASE_SL4-6.skingeom"
local TANK_ANIM      = "NPC_STRAIN_TANK.anim"

-- Soldier fallbacks (resident on Dreamland and most military levels).
local SOLDIER_SKIN = "NPC_MIL_SPEC4_01_LEVELBLUE_SL0-1.skingeom"
local SOLDIER_ANIM = "NPC_BLACKOPS_SMP.anim"

local ALL_FACTIONS =
{
    "PlayerNormal", "PlayerStrain1", "PlayerStrain2", "PlayerStrain3",
    "Neutral", "BlackOps", "Military", "MutantsLesser", "MutantsGreater",
    "Gray", "Theta", "Workers", "Team 1", "Team 2", "Deathmatch",
}

local spawned     = {}
local mounted_dir = nil

-- ============================================================

local function template_for(type_name, fb_skin, fb_anim)
    local g = obj_getfirst(type_name)
    if g and g ~= 0 then
        local skin = obj_prop_get(g, "RenderInst\\File")
        local anim = obj_prop_get(g, "RenderInst\\Anim")
        local wls  = obj_prop_get(g, "RenderInst\\Weaponless Anim")
        return skin or fb_skin, anim or fb_anim, wls or anim or fb_anim, true
    end
    return fb_skin, fb_anim, fb_anim, false
end

local function set_faction(guid, faction, friends)
    obj_prop_set(guid, "Factions\\Faction", faction)
    for _, f in ipairs(ALL_FACTIONS) do
        obj_prop_set(guid, "Factions\\FriendlyFactions\\" .. f, false)
    end
    for _, f in ipairs(friends) do
        obj_prop_set(guid, "Factions\\FriendlyFactions\\" .. f, true)
    end
end

local SPAWN_WALL_CLEARANCE = 80
local SPAWN_PROBE_HEIGHT   = 40

-- Stop short of any wall between center and the point, then snap to the nav mesh.
local function place_on_nav(cx, cy, cz, x, y, z)
    local hx, _, hz = raycast_world(cx, cy + SPAWN_PROBE_HEIGHT, cz, x, y + SPAWN_PROBE_HEIGHT, z)
    if hx then
        local dx, dz = hx - cx, hz - cz
        local len = math.sqrt(dx * dx + dz * dz)
        if len > SPAWN_WALL_CLEARANCE then
            local s = (len - SPAWN_WALL_CLEARANCE) / len
            x, z = cx + dx * s, cz + dz * s
        else
            x, z = cx, cz
        end
    end
    return nav_nearest_point(x, y, z)
end

-- Common character setup; must run inside an obj_load_start/obj_load_end bracket.
local function base_setup(guid, x, y, z, skin, anim, wls, zone1, zone2)
    obj_prop_set(guid, "RenderInst\\File",            skin)
    obj_prop_set(guid, "RenderInst\\Anim",            anim)
    obj_prop_set(guid, "RenderInst\\Weaponless Anim", wls)
    obj_prop_set(guid, "Base\\Position",              x, y, z)
    obj_prop_set(guid, "Character\\Start Active",        true)
    obj_prop_set(guid, "Character\\Flags\\Combat Ready", true)
    obj_prop_set(guid, "Character\\Flags\\Run Logic",    true)
    obj_prop_set(guid, "Character\\Flags\\Root to position and idle", false)
    obj_prop_set(guid, "Character\\Pathing\\Use Navigation Map", true)
    obj_prop_set(guid, "Character\\Pathing\\Use Small Paths",    true)
    obj_setzones(guid, zone1, zone2)
end

local function spawn_soldier(x, y, z, skin, anim, wls, zone1, zone2)
    local g = obj_create(SOLDIER_TYPE)
    if g == 0 then return 0 end
    obj_load_start(g)
    base_setup(g, x, y, z, skin, anim, wls, zone1, zone2)
    obj_prop_set(g, "Character\\Logical Name", "BlackOps")
    obj_prop_set(g, "Character\\Weapon",       "Weapon SMP")
    set_faction(g, "Military", { "Military", "PlayerNormal", "Neutral", "Workers" })
    obj_load_end(g)
    obj_activate(g, true)
    spawned[#spawned + 1] = g
    return g
end

local function spawn_theta(x, y, z, skin, anim, wls, zone1, zone2)
    local g = obj_create(TANK_TYPE)
    if g == 0 then return 0 end
    obj_load_start(g)
    base_setup(g, x, y, z, skin, anim, wls, zone1, zone2)
    obj_prop_set(g, "Character\\Logical Name", "Theta")
    -- Off the nav map she heads straight for the target in any environment.
    obj_prop_set(g, "Character\\Pathing\\Use Navigation Map", false)
    set_faction(g, "Theta", { "Theta" })   -- hostile to everyone, including the player
    obj_load_end(g)
    obj_activate(g, true)
    spawned[#spawned + 1] = g
    return g
end

-- ============================================================

local function ensure_tank_assets()
    local g = obj_getfirst(TANK_TYPE)
    if g and g ~= 0 then return true end

    if not asset_is_loaded(TANK_SKIN) then
        if asset_mount(TANK_ASSET_DIR) then
            mounted_dir = TANK_ASSET_DIR
        else
            log("[boss_fight] could not mount '" .. TANK_ASSET_DIR .. "'")
        end
        asset_load(TANK_SKIN)
        asset_load(TANK_ANIM)
    end

    if not asset_is_loaded(TANK_SKIN) then
        log("[boss_fight] WARNING: Theta skin '" .. TANK_SKIN ..
            "' not available; she will be invisible. Point TANK_ASSET_DIR/TANK_SKIN at a build that has the tank.")
        return false
    end
    return true
end

-- Self-contained type-A combat profile (ASCEND3 values) so she works anywhere.
local function define_theta_combat()
    tweak_set("THETA_MeleeRange",         300)
    tweak_set("THETA_MeleeMinAngle",      45)
    tweak_set("THETA_MeleeReachDistance", 300)
    tweak_set("THETA_MeleeSphereRadius",  150)
    tweak_set("THETA_BubbleHealth",       600)
    tweak_set("THETA_ProjectileSpeed",    3500)

    -- Big health pool stands in for armor; fast and aggressive.
    tweak_set("THETA_A_Health",                               3000)
    tweak_set("THETA_A_StageCount",                           1)
    tweak_set("THETA_A_Stage0_HealthPercent",                 100)
    tweak_set("THETA_A_Stage0_MoveSpeed",                     1.1)
    tweak_set("THETA_A_Stage0_ChaseMinTime",                 0)
    tweak_set("THETA_A_Stage0_EvadeInterval",                2)
    tweak_set("THETA_A_Stage0_MeleeInterval",                0.5)

    -- Arm-cannon ranged attack (fires beyond melee range).
    tweak_set("THETA_A_Stage0_RangedInterval",               1.5)
    tweak_set("THETA_A_Stage0_RangedMinDist",                600)
    tweak_set("THETA_A_Stage0_RangedMaxDist",                5000)

    -- Arena-only mechanics off.
    tweak_set("THETA_A_Stage0_ChargeInterval",               -1)
    tweak_set("THETA_A_Stage0_LeapInterval",                 -1)
    tweak_set("THETA_A_Stage0_BubbleHealthPercentInterval",   -1)
    tweak_set("THETA_A_Stage0_CanisterHealthPercentInterval", -1)
    tweak_set("THETA_A_Stage0_ParasiteShieldRegenInterval",   -1)
    tweak_set("THETA_A_Stage0_ContagionHealthPercentInterval",-1)
    tweak_set("THETA_A_Stage0_JumpToPerch",                  -1)
    tweak_set("THETA_A_Stage0_JumpToGrate",                  -1)
end

-- Point friendlies at the boss and the boss at the player; the AI takes it from there.
function make_everyone_fight(boss_guid)
    if not boss_guid or boss_guid == 0 then return end

    local count = 0
    for _, type_name in ipairs(FRIENDLY_NPC_TYPES) do
        local g = obj_getfirst(type_name)
        while g and g ~= 0 do
            if g ~= boss_guid and obj_isalive(g) then
                obj_set_target(g, boss_guid)
                count = count + 1
            end
            g = obj_getnext(g)
        end
    end

    obj_set_target(boss_guid, obj_get_player())
    log(string.format("[boss_fight] %d friendlies now hunting the boss", count))
end

local function spawn_encounter()
    local hx, hy, hz, nx, ny, nz = raycast_from_cam_pos(RAY_DIST)
    if not hx then log("[boss_fight] no surface hit") return end
    if ny < FLOOR_NY_MIN then log("[boss_fight] aim at a floor") return end

    ensure_tank_assets()
    define_theta_combat()

    local player = obj_get_player()
    local z1, z2 = obj_getzone(player)

    local t_skin, t_anim, t_wls = template_for(TANK_TYPE,    TANK_SKIN,    TANK_ANIM)
    local s_skin, s_anim, s_wls = template_for(SOLDIER_TYPE, SOLDIER_SKIN, SOLDIER_ANIM)

    local bx, by, bz = nav_nearest_point(hx, hy, hz)
    local theta = spawn_theta(bx, by, bz, t_skin, t_anim, t_wls, z1, z2)

    for i = 1, SQUAD_COUNT do
        local a = (2 * math.pi / SQUAD_COUNT) * (i - 1)
        local px, py, pz = place_on_nav(hx, hy, hz,
                                        hx + math.cos(a) * SQUAD_RADIUS,
                                        hy,
                                        hz + math.sin(a) * SQUAD_RADIUS)
        spawn_soldier(px, py, pz, s_skin, s_anim, s_wls, z1, z2)
    end

    make_everyone_fight(theta)

    log(string.format("[boss_fight] Theta(guid=%d) vs %d soldiers at (%.0f, %.0f, %.0f) zone=%d",
        theta, SQUAD_COUNT, hx, hy, hz, z1))
end

local function clear_encounter()
    local n = 0
    for _, g in ipairs(spawned) do
        if g ~= 0 and obj_isalive(g) then
            obj_destroy(g)
            n = n + 1
        end
    end
    spawned = {}
    if mounted_dir then
        asset_unmount(mounted_dir)
        mounted_dir = nil
    end
    log(string.format("[boss_fight] cleared %d", n))
end

-- ============================================================

register_update(function(dt)
    if input_waspressed(KEY_SPAWN) then spawn_encounter() end
    if input_waspressed(KEY_CLEAR) then clear_encounter() end
end)

register_level_unload(function()
    clear_encounter()
end)
