-- morph.lua
-- Random virtual-mesh and skin changer for NPC - BlackOpps.
--
-- Every MORPH_INTERVAL seconds each NPC gets a randomly-chosen vmesh mask,
-- making the engine show/hide arbitrary submesh groups mid-game.
--
-- Every SKIN_INTERVAL seconds one random NPC is given a different .skingeom,
-- cycling through the known pool.

local MORPH_INTERVAL = 1.5   -- seconds between vmesh randomisations
local SKIN_INTERVAL  = 8.0   -- seconds between skin swaps
local NPC_TYPE       = "NPC - BlackOpps"

-- Pool of .skingeom files known to exist in the shipped data.
-- Add or remove entries freely.
local SKIN_POOL = {
    "MP_AVATAR_BIND.skingeom",       -- standard BlackOps body (default)
    "FP_PLR_Human_BIND.skingeom",    -- first-person player arms rig
    "WPN_MUT_Spear.skingeom",        -- mutant tendril/spear mesh
}

-- ============================================================
--  State
-- ============================================================

local t_morph = 0.0
local t_skin  = 0.0

-- Per-NPC vmesh metadata cached on level load.
-- npcs[guid] = { n_vmesh = <int>, full_mask = <int> }
local npcs = {}

-- ============================================================
--  Helpers
-- ============================================================

local function collect_npcs()
    npcs = {}
    local g = obj_getfirst( NPC_TYPE )
    while g and g ~= 0 do
        local nv = geom_getnumvmeshes( g )

        -- Dump mesh info once so you can see what the geom contains.
        local nm = geom_getnummeshes( g )
        log( xfs( "morph: NPC guid=%d  meshes=%d  vmeshes=%d", g, nm, nv ) )
        for i = 0, nm - 1 do
            local name = geom_getmeshname( g, i )
            log( xfs( "  mesh[%d] = %s", i, name or "(nil)" ) )
        end
        for i = 0, nv - 1 do
            local name = geom_getvmeshname( g, i )
            log( xfs( "  vmesh[%d] = %s", i, name or "(nil)" ) )
        end

        -- full_mask has all vmesh bits set; 0 means no vmeshes → skip masking.
        local full = 0
        if nv > 0 then
            full = (1 << nv) - 1
        end

        npcs[g] = { n_vmesh = nv, full_mask = full }

        g = obj_getnext( g )
    end
    log( xfs( "morph: cached %d NPCs", #npcs ) )
end

local function random_nonzero_mask( full_mask )
    -- Pick any non-zero subset so at least one vmesh is always visible.
    local mask = math.random( 1, full_mask )
    return mask
end

-- ============================================================
--  Hooks
-- ============================================================

register_level_load( function()
    math.randomseed( os.time() )
    collect_npcs()
    t_morph = 0.0
    t_skin  = 0.0
end )

register_update( function( dt )

    -- ---- vmesh randomisation ----
    t_morph = t_morph + dt
    if t_morph >= MORPH_INTERVAL then
        t_morph = 0.0

        for g, info in pairs( npcs ) do
            if info.full_mask ~= 0 then
                local mask = random_nonzero_mask( info.full_mask )
                geom_setvmeshmask( g, mask )
            end
        end
    end

    -- ---- skin swap ----
    t_skin = t_skin + dt
    if t_skin >= SKIN_INTERVAL then
        t_skin = 0.0

        -- Collect live guids (some NPCs may have died since level load).
        local live = {}
        local g = obj_getfirst( NPC_TYPE )
        while g and g ~= 0 do
            live[#live + 1] = g
            g = obj_getnext( g )
        end

        if #live > 0 then
            -- Pick one random NPC and one random skin.
            local target   = live[ math.random( #live ) ]
            local new_skin = SKIN_POOL[ math.random( #SKIN_POOL ) ]
            log( xfs( "morph: swapping guid=%d to %s", target, new_skin ) )
            geom_setskin( target, new_skin )
        end
    end

end )
