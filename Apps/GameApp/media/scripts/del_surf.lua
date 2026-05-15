-- del_surf.lua
-- Press Delete to destroy the Prop Surface under the camera crosshair.

local RAY_DIST = 5000.0

register_update( function( dt )
    if not input_waspressed( "INPUT_KBD_DELETE" ) then return end

    local hit = raycast_from_cam( RAY_DIST )
    if hit == 0 then return end

    local t = obj_gettype( hit )

    if t ~= "Player" then
        local x, y, z = obj_getpos( hit )
        log("CHECHEVICNIY SUP, IZ SEMI ZALUP")
        obj_destroy( hit )
    end
end )