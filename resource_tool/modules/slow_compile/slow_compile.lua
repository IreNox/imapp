local module = Module:new();

module:add_files( "include/*.h" );
module:add_files( "src/*.cpp" );
module:add_files( "*.lua" );

module:add_include_dir( "include" );

module:add_external( "https://github.com/samhocevar/portable-file-dialogs.git" )
