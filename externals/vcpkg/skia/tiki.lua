-- vcpkg://skia

--module:set_define( "MINIZ_NO_STDIO" );

local skia_module = module
local skia_external = tiki.external

skia_module.external = tiki.external

skia_module:add_library_file( "skia.lib" )

skia_module.import_func = function( project, solution )
	cppdialect( "C++17" )
	staticruntime( "On" )
	
	for _, platform in pairs( solution.platforms ) do
		local triplet = skia_external:get_vcpkg_triplet( platform );
	
		skia_module:add_include_dir( triplet .. "/include/skia", nil, platform )
	end
end
