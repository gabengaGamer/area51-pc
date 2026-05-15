-- dynamic_light_spawner.lua
--
-- F4              - spawn / respawn one controllable dynamic light
-- Arrow keys      - move the light on world X/Z
-- NUM7 / NUM9     - intensity -/+
-- NUM4 / NUM6     - range -/+
-- NUM1 / NUM3     - falloff -/+
-- NUM5            - toggle shadow casting

local LIGHT_TYPE    = "Dynamic Light"
local RAY_DIST      = 110000
local SPAWN_OFFSET  = 60
local MOVE_SPEED    = 700

local INTENSITY_STEP = 1.5
local RANGE_STEP     = 900
local FALLOFF_STEP   = 0.8

local MIN_INTENSITY = 0.0
local MAX_INTENSITY = 5.0
local MIN_RANGE     = 50.0
local MAX_RANGE     = 5000.0

local KEY_SPAWN        = "INPUT_KBD_F4"
local KEY_MOVE_LEFT    = "INPUT_KBD_LEFT"
local KEY_MOVE_RIGHT   = "INPUT_KBD_RIGHT"
local KEY_MOVE_FORWARD = "INPUT_KBD_UP"
local KEY_MOVE_BACK    = "INPUT_KBD_DOWN"

local KEY_INTENSITY_DOWN = "INPUT_KBD_NUMPAD7"
local KEY_INTENSITY_UP   = "INPUT_KBD_NUMPAD9"
local KEY_RANGE_DOWN     = "INPUT_KBD_NUMPAD4"
local KEY_RANGE_UP       = "INPUT_KBD_NUMPAD6"
local KEY_FALLOFF_DOWN   = "INPUT_KBD_NUMPAD1"
local KEY_FALLOFF_UP     = "INPUT_KBD_NUMPAD3"
local KEY_TOGGLE_SHADOWS = "INPUT_KBD_NUMPAD5"

local light_guid = 0

local light_state =
{
    intensity    = 1.5,
    range        = 900.0,
    falloff      = 0.40,
    cast_shadows = true,
}

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function light_exists()
    return light_guid ~= 0 and obj_isalive(light_guid)
end

local function destroy_light()
    if light_exists() then
        obj_destroy(light_guid)
    end
    light_guid = 0
end

local function apply_static_setup()
    obj_prop_set(light_guid, "Light\\EmitterType", "OMNI")
    obj_prop_set(light_guid, "Light\\Behavior", "CONSTANT")
    obj_prop_set(light_guid, "Light\\StartActive", true)
    obj_prop_set(light_guid, "Light\\LightColor", 255, 245, 220, 255)
end

local function apply_runtime_settings()
    if not light_exists() then return end

    obj_prop_set(light_guid, "Light\\Range",       light_state.range)
    obj_prop_set(light_guid, "Light\\Intensity",   light_state.intensity)
    obj_prop_set(light_guid, "Light\\Falloff",     light_state.falloff)
    obj_prop_set(light_guid, "Light\\CastShadows", light_state.cast_shadows)

    if light_state.cast_shadows then
        obj_prop_set(light_guid, "Light\\ShadowMapResolution", "2048")
        obj_prop_set(light_guid, "Light\\ShadowPriority",      "HIGH")
    end
end

local function spawn_light()
    local hx, hy, hz, nx, ny, nz = raycast_from_cam_pos(RAY_DIST)
    if not hx then
        log("[dynamic_light] no surface hit")
        return
    end

    destroy_light()

    light_guid = obj_create(LIGHT_TYPE)
    if light_guid == 0 then
        log("[dynamic_light] ERROR: obj_create failed")
        return
    end

    obj_setpos(light_guid,
        hx + nx * SPAWN_OFFSET,
        hy + ny * SPAWN_OFFSET,
        hz + nz * SPAWN_OFFSET)

    apply_static_setup()
    apply_runtime_settings()
    obj_activate(light_guid, true)

    log(string.format(
        "[dynamic_light] spawned guid=%d pos=(%.0f, %.0f, %.0f) intensity=%.2f range=%.0f falloff=%.2f shadows=%s",
        light_guid,
        hx + nx * SPAWN_OFFSET,
        hy + ny * SPAWN_OFFSET,
        hz + nz * SPAWN_OFFSET,
        light_state.intensity,
        light_state.range,
        light_state.falloff,
        light_state.cast_shadows and "ON" or "OFF"))
end

local function movement_tick(dt)
    if not light_exists() then return end

    local delta = MOVE_SPEED * dt
    local dx = 0.0
    local dz = 0.0

    if input_ispressed(KEY_MOVE_LEFT)  then dx = dx - delta end
    if input_ispressed(KEY_MOVE_RIGHT) then dx = dx + delta end
    if input_ispressed(KEY_MOVE_FORWARD) then dz = dz - delta end
    if input_ispressed(KEY_MOVE_BACK)    then dz = dz + delta end

    if dx ~= 0.0 or dz ~= 0.0 then
        obj_moverel(light_guid, dx, 0.0, dz)
    end
end

local function tuning_tick(dt)
    if input_waspressed(KEY_TOGGLE_SHADOWS) then
        light_state.cast_shadows = not light_state.cast_shadows
        apply_runtime_settings()
        log(string.format("[dynamic_light] shadows %s", light_state.cast_shadows and "ON" or "OFF"))
    end

    if not light_exists() then return end

    local changed = false

    if input_ispressed(KEY_INTENSITY_DOWN) then
        light_state.intensity = clamp(light_state.intensity - INTENSITY_STEP * dt, MIN_INTENSITY, MAX_INTENSITY)
        changed = true
    end
    if input_ispressed(KEY_INTENSITY_UP) then
        light_state.intensity = clamp(light_state.intensity + INTENSITY_STEP * dt, MIN_INTENSITY, MAX_INTENSITY)
        changed = true
    end
    if input_ispressed(KEY_RANGE_DOWN) then
        light_state.range = clamp(light_state.range - RANGE_STEP * dt, MIN_RANGE, MAX_RANGE)
        changed = true
    end
    if input_ispressed(KEY_RANGE_UP) then
        light_state.range = clamp(light_state.range + RANGE_STEP * dt, MIN_RANGE, MAX_RANGE)
        changed = true
    end
    if input_ispressed(KEY_FALLOFF_DOWN) then
        light_state.falloff = clamp(light_state.falloff - FALLOFF_STEP * dt, 0.0, 1.0)
        changed = true
    end
    if input_ispressed(KEY_FALLOFF_UP) then
        light_state.falloff = clamp(light_state.falloff + FALLOFF_STEP * dt, 0.0, 1.0)
        changed = true
    end

    if changed then
        apply_runtime_settings()
    end
end

register_update(function(dt)
    if input_waspressed(KEY_SPAWN) then
        spawn_light()
    end

    movement_tick(dt)
    tuning_tick(dt)
end)

register_level_unload(function()
    destroy_light()
end)

