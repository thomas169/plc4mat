function make_plc4c(recipe)
% psudo makefile

plc4c_root = '/home/thomas/MEGA/plc4x/sandbox/plc4c/';

name = 'plc4c';
srcDir = 'src';
outDir = 'bin';
plc4c_mex_root = fullfile(strrep(mfilename('fullpath'),mfilename,''),'..');

default_goal = 'build';
if nargin == 0 
    recipe = default_goal;
end

%% Options, probably no need to change

args.target = fullfile(plc4c_mex_root,srcDir,[name '.mexa64']);
args.srcs = {fullfile(plc4c_mex_root,srcDir,[name '.cpp'])};
args.libs = {'-ldl'};
args.outdir = outDir;
args.output = name;

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
    mex(args.flags{:}, args.incs{:}, args.srcs{:}, args.objects{:}, ...
        args.archives{:},'-outdir', args.outdir,'-output', args.output,... 
        args.libs{:} );
end

function clean(args)
    fileName = fullfile(args.outdir,args.target);
    delete(fileName);
end