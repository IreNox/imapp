-- https://github.com/freetype/freetype.git

local ft_module = module
local ft_project = nil
if tiki.use_lib then
	ft_project = Project:new( "msdfgen", ProjectTypes.StaticLibrary )
	ft_module = ft_project.module
end

ft_module.module_type = ModuleTypes.FilesModule

ft_module:add_include_dir( "include" )

ft_module:add_files( "include/**.h" )
ft_module:add_files( "src/autofit/autofit.c" )
ft_module:add_files( "src/base/ftbase.c" )
ft_module:add_files( "src/base/ftbitmap.c" )
ft_module:add_files( "src/base/ftdebug.c" )
ft_module:add_files( "src/base/ftglyph.c" )
ft_module:add_files( "src/base/ftinit.c" )
ft_module:add_files( "src/base/ftmm.c" )
ft_module:add_files( "src/base/ftsystem.c" )
ft_module:add_files( "src/bdf/bdf.c" )
ft_module:add_files( "src/bzip2/*.c" )
ft_module:add_files( "src/cache/ftcache.c" )
ft_module:add_files( "src/cff/cff.c" )
ft_module:add_files( "src/cid/type1cid.c" )
ft_module:add_files( "src/dlg/*.c" )
ft_module:add_files( "src/gxvalid/gxvalid.c" )
ft_module:add_files( "src/gzip/ftgzip.c" )
ft_module:add_files( "src/lzw/ftlzw.c" )
ft_module:add_files( "src/otvalid/otvalid.c" )
ft_module:add_files( "src/pcf/pcf.c" )
ft_module:add_files( "src/pfr/pfr.c" )
ft_module:add_files( "src/psaux/psaux.c" )
ft_module:add_files( "src/pshinter/pshinter.c" )
ft_module:add_files( "src/psnames/psnames.c" )
ft_module:add_files( "src/raster/raster.c" )
ft_module:add_files( "src/sdf/sdf.c" )
ft_module:add_files( "src/sfnt/sfnt.c" )
ft_module:add_files( "src/smooth/smooth.c" )
ft_module:add_files( "src/svg/svg.c" )
ft_module:add_files( "src/truetype/truetype.c" )
ft_module:add_files( "src/type1/type1.c" )
ft_module:add_files( "src/type42/type42.c" )
ft_module:add_files( "src/winfonts/*.c" )

ft_module:add_define( "FT2_BUILD_LIBRARY" )

if tiki.use_lib then
	ft_module.import_func = function( project, solution )
		project:add_project_dependency( ft_project )
		solution:add_project( ft_project )
	end
end
