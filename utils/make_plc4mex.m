function make_plc4mex(recipe)
% psudo makefile

plc4c_root = '/home/thomas/MEGA/plc4x/sandbox/plc4c/';

srcName = 'plc4mex';
srcDir = 'src';
outDir = 'bin';
projectRoot = fullfile(strrep(mfilename('fullpath'),mfilename,''),'..');

default_goal = 'build';
if nargin == 0 
    recipe = default_goal;
end

%% Options, probably no need to change

args.root = projectRoot;
args.srcName = srcName;
args.srcDir = srcDir;
args.outDir = outDir;
args.libs = {'-ldl'};

args.objects = {
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'write_buffer.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'subscribe.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'types.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'read.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'connection.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'write.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'system.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'read_buffer.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'data.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'utils', 'queue.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'utils', 'list.c.o'),...
    fullfile(plc4c_root, 'spi', 'CMakeFiles', 'plc4c-spi.dir', 'src', 'evaluation_helper.c.o'),...
    }; 

args.incs = {
    ['-I' fullfile(plc4c_root, 'api', 'include')], ...
    ['-I' fullfile(plc4c_root, 'drivers', 's7','include')],...
    ['-I' fullfile(plc4c_root, 'transports', 'tcp','include')],...
    ['-I' fullfile(plc4c_root, 'spi', 'include')] ...
    };

args.archives = {
    fullfile(plc4c_root, 'drivers', 's7', 'libplc4c-driver-s7.a'),...
    fullfile(plc4c_root, 'transports', 'tcp', 'libplc4c-transport-tcp.a')...
    };
    
args.flags = { '-g'};

%% Recipes

switch recipe
    case 'build'
        build(args);
    case 'clean' 
        clean(args);
end

end

function build(args)
    fileName = fullfile(args.root, args.srcDir, [args.srcName '.cpp']);
    mex(args.flags{:}, args.incs{:}, fileName, args.objects{:}, ...
        args.archives{:},'-outdir', args.outDir,'-output', args.srcName,... 
        args.libs{:} );
end

function clean(args)
    fileName = fullfile(args.root, args.outDir,[args.srcName '.mexa64'] );
    delete(fileName);
end
