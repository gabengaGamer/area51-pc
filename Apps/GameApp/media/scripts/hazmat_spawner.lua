-- hazmat_spawner.lua
--
--  F5  — cast ray from camera, spawn one Hazmat at the hit point
--  F6  — cast ray from camera, spawn ring of 5 Hazmats around the hit point
--  F7  — destroy all Hazmats spawned this session
--
--  Rotation cheat (last spawned / selected bot):
--  NUM4 / NUM6  — Yaw  -/+ 15°
--  NUM8 / NUM2  — Pitch -/+ 15°
--  NUM7 / NUM9  — Roll  -/+ 15°
--  NUM5         — reset rotation to 0 / 0 / 0
--
-- Resource paths are copied from the first Hazmat already present in the level.
-- If none exists, falls back to HAZMAT_SKIN / HAZMAT_ANIM constants below.

local HAZMAT_TYPE  = "NPC - Hazmat"
local RAY_DIST     = 110000
local WAVE_RADIUS  = 400
local WAVE_COUNT   = 100
local SPAWN_MIN_FLOOR_NY = 0.5

local KEY_SPAWN_ONE  = "INPUT_KBD_F5"
local KEY_SPAWN_WAVE = "INPUT_KBD_F6"
local KEY_KILL_ALL   = "INPUT_KBD_F7"

-- Rotation cheat keys (numpad)
local KEY_YAW_L  = "INPUT_KBD_NUMPAD4"
local KEY_YAW_R  = "INPUT_KBD_NUMPAD6"
local KEY_PITCH_U = "INPUT_KBD_NUMPAD8"
local KEY_PITCH_D = "INPUT_KBD_NUMPAD2"
local KEY_ROLL_L  = "INPUT_KBD_NUMPAD7"
local KEY_ROLL_R  = "INPUT_KBD_NUMPAD9"
local KEY_ROT_RESET = "INPUT_KBD_NUMPAD5"

local ROT_STEP     = 90   -- degrees per second (hold key)

-- Rainbow cheat
local KEY_RAINBOW    = "INPUT_KBD_F1"   -- toggle on/off
local RAINBOW_SPEED  = 90               -- degrees per second along hue wheel
local rainbow_on     = false
local rainbow_hue    = 65                -- 0..360

-- Fallback resource paths (fill in from your build if needed)
local HAZMAT_SKIN            = "NPC_MIL_SPEC4_01_LEVELBLUE_SL1-2.skingeom"
local HAZMAT_ANIM            = "NPC_Military_SMP.anim"
local HAZMAT_ANIM_WEAPONLESS = "NPC_Military_SMP.anim"

local spawned_guids = {}
local selected_guid = 0   -- guid of the bot we're rotating

-- Current rotation state for the selected bot (degrees)
local rot_pitch = 0
local rot_yaw   = 0
local rot_roll  = 0

-- ============================================================
--  Resource template: read paths off the first level Hazmat
-- ============================================================

local tmpl_skin            = nil
local tmpl_anim            = nil
local tmpl_anim_weaponless = nil

local function cache_template()
    if tmpl_skin then return true end

    local guid = obj_getfirst(HAZMAT_TYPE)
    if not guid or guid == 0 then
        log("[hazmat_spawner] WARNING: no Hazmat in level, using fallback resource paths")
        tmpl_skin            = HAZMAT_SKIN
        tmpl_anim            = HAZMAT_ANIM
        tmpl_anim_weaponless = HAZMAT_ANIM_WEAPONLESS
        return false
    end

    tmpl_skin            = obj_prop_get(guid, "RenderInst\\File")
    tmpl_anim            = obj_prop_get(guid, "RenderInst\\Anim")
    tmpl_anim_weaponless = obj_prop_get(guid, "RenderInst\\Weaponless Anim")

    tmpl_skin            = tmpl_skin            or HAZMAT_SKIN
    tmpl_anim            = tmpl_anim            or HAZMAT_ANIM
    tmpl_anim_weaponless = tmpl_anim_weaponless or HAZMAT_ANIM_WEAPONLESS

    log(string.format("[hazmat_spawner] template: skin='%s'  anim='%s'", tmpl_skin, tmpl_anim))
    return true
end

-- ============================================================
--  Setup
-- ============================================================

local function setup_hazmat(guid)
    obj_prop_set(guid, "RenderInst\\File",            tmpl_skin)
    obj_prop_set(guid, "RenderInst\\Anim",            tmpl_anim)
    obj_prop_set(guid, "RenderInst\\Weaponless Anim", tmpl_anim_weaponless)

    obj_prop_set(guid, "Character\\Logical Name",        "Hazmat")
    obj_prop_set(guid, "Character\\Health",              1.0)
    obj_prop_set(guid, "Character\\Start Active",        true)
    obj_prop_set(guid, "Character\\Flags\\Combat Ready", true)
    obj_prop_set(guid, "Character\\Flags\\Run Logic",    true)
    obj_prop_set(guid, "Character\\Pathing\\Use Navigation Map",    true)	
    obj_prop_set(guid, "Character\\Pathing\\Use Small Paths",    true)		
    obj_prop_set(guid, "Character\\Weapon",              "Weapon SMP")

    obj_prop_set(guid, "FollowState\\ResponseList\\Invincible", false)
end

local function spawn_one(x, y, z)
    cache_template()

    local guid = obj_create(HAZMAT_TYPE)
    if guid == 0 then
        log("[hazmat_spawner] ERROR: obj_create failed")
        return 0
    end

    obj_setpos(guid, x, y, z)
    setup_hazmat(guid)
    spawned_guids[#spawned_guids + 1] = guid

    -- Auto-select the freshly spawned bot for rotation cheat
    selected_guid = guid
    rot_pitch = 0
    rot_yaw   = 0
    rot_roll  = 0

    log(string.format("[hazmat_spawner] spawned guid=%d  pos=(%.0f, %.0f, %.0f)", guid, x, y, z))
    return guid
end

-- ============================================================
--  Public spawners
-- ============================================================

function hazmat_spawn_one()
    local hx, hy, hz, nx, ny, nz = raycast_from_cam_pos(RAY_DIST)
    if not hx then
        log("[hazmat_spawner] no surface hit")
        return
    end
    if ny < SPAWN_MIN_FLOOR_NY then
        log(string.format("[hazmat_spawner] surface too steep (ny=%.2f)", ny))
        return
    end
    spawn_one(hx, hy, hz)
end

function hazmat_spawn_wave()
    local hx, hy, hz, nx, ny, nz = raycast_from_cam_pos(RAY_DIST)
    if not hx then
        log("[hazmat_spawner] no surface hit")
        return
    end
    if ny < SPAWN_MIN_FLOOR_NY then
        log(string.format("[hazmat_spawner] surface too steep (ny=%.2f)", ny))
        return
    end
    for i = 1, WAVE_COUNT do
        local angle = (2 * math.pi / WAVE_COUNT) * (i - 1)
        spawn_one(hx + math.cos(angle) * WAVE_RADIUS,
                  hy,
                  hz + math.sin(angle) * WAVE_RADIUS)
    end
    log(string.format("[hazmat_spawner] wave of %d at (%.0f, %.0f, %.0f)", WAVE_COUNT, hx, hy, hz))
end

function hazmat_kill_all()
    local count = 0
    for _, guid in ipairs(spawned_guids) do
        if guid ~= 0 and obj_isalive(guid) then
            obj_destroy(guid)
            count = count + 1
        end
    end
    spawned_guids = {}
    selected_guid = 0
    log(string.format("[hazmat_spawner] destroyed %d Hazmat(s)", count))
end

-- ============================================================
--  Rotation cheat helpers
-- ============================================================

local function apply_rotation()
    if selected_guid == 0 or not obj_isalive(selected_guid) then
        log("[hazmat_spawner] rot: no valid bot selected")
        return
    end
    -- bind_obj_setrotation expects (guid, pitch, yaw, roll) in degrees
    obj_setrotation(selected_guid, rot_pitch, rot_yaw, rot_roll)
    log(string.format("[hazmat_spawner] rot guid=%d  pitch=%.0f  yaw=%.0f  roll=%.0f",
        selected_guid, rot_pitch, rot_yaw, rot_roll))
end

-- HSV->RGB (h=0..360, s=0..1, v=0..1) -> r,g,b 0..255
local function hsv_to_rgb(h, s, v)
    local i = math.floor(h / 60) % 6
    local f = (h / 60) - math.floor(h / 60)
    local p = v * (1 - s)
    local q = v * (1 - f * s)
    local t = v * (1 - (1 - f) * s)
    local r, g, b
    if     i == 0 then r,g,b = v,t,p
    elseif i == 1 then r,g,b = q,v,p
    elseif i == 2 then r,g,b = p,v,t
    elseif i == 3 then r,g,b = p,q,v
    elseif i == 4 then r,g,b = t,p,v
    else               r,g,b = v,p,q
    end
    return math.floor(r*255), math.floor(g*255), math.floor(b*255)
end

local function color_tick(dt)
    if input_waspressed(KEY_RAINBOW) then
        rainbow_on = not rainbow_on
        log(string.format("[hazmat_spawner] rainbow %s", rainbow_on and "ON" or "OFF"))
        if not rainbow_on and selected_guid ~= 0 and obj_isalive(selected_guid) then
            -- сброс при выключении
            obj_prop_set(selected_guid, "RenderInst\\MinAmbient", 0, 0, 0, 255)
        end
    end

    if not rainbow_on then return end
    if selected_guid == 0 or not obj_isalive(selected_guid) then return end

    rainbow_hue = (rainbow_hue + RAINBOW_SPEED * dt) % 360
    local r, g, b = hsv_to_rgb(rainbow_hue, 1.0, 1.0)
    obj_prop_set(selected_guid, "RenderInst\\MinAmbient", r, g, b, 255)
end

local function rot_tick(dt)
    if selected_guid == 0 then return end

    local delta = ROT_STEP * dt
    local changed = false

    if input_ispressed(KEY_YAW_L) then
        rot_yaw = rot_yaw - delta
        changed = true
    end
    if input_ispressed(KEY_YAW_R) then
        rot_yaw = rot_yaw + delta
        changed = true
    end
    if input_ispressed(KEY_PITCH_U) then
        rot_pitch = rot_pitch + delta
        changed = true
    end
    if input_ispressed(KEY_PITCH_D) then
        rot_pitch = rot_pitch - delta
        changed = true
    end
    if input_ispressed(KEY_ROLL_L) then
        rot_roll = rot_roll - delta
        changed = true
    end
    if input_ispressed(KEY_ROLL_R) then
        rot_roll = rot_roll + delta
        changed = true
    end
    -- сброс оставляем на waspressed — одиночное нажатие
    if input_waspressed(KEY_ROT_RESET) then
        rot_pitch, rot_yaw, rot_roll = 0, 0, 0
        changed = true
    end

    if changed then
        apply_rotation()
    end
end

-- ============================================================
--  Update
-- ============================================================

register_update(function(dt)
    if input_waspressed(KEY_SPAWN_ONE)  then hazmat_spawn_one()  end
    if input_waspressed(KEY_SPAWN_WAVE) then hazmat_spawn_wave() end
    if input_waspressed(KEY_KILL_ALL)   then hazmat_kill_all()   end

    rot_tick(dt)
    color_tick(dt)
end)