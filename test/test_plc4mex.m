function varargout = test_plc4mex(scenario)

if nargin == 0 
    scenario = 'all';
end

switch (scenario)
    case 'all'
        build();
        connect();
        [write_req, read_req] = formRequests();
        write(write_req);
        varargout{1} = read(read_req);
    case 'make'
        build();
    case 'connect'
        connect();
    case 'update'
        build();
        connect();
    case 'read'
        [~, read_req] = formRequests();
        varargout{1} = read(read_req);
    case 'write'
        [write_req, ~] = formRequests();
        write(write_req);
    otherwise 
        warning('switch case not reconginsed')
end

end

%% formRequests
function [write_req, read_req] = formRequests()
    
    reqType = struct('name',[],'address',[],'value',[]);
    
    write_req(2) = reqType;
    write_req(1).name = 'IP1';
    write_req(1).address = '%DB2:0.0:REAL[66]';
    write_req(1).value = [single(1.1) single(2.2)];
    write_req(2).name = 'OP2';
    write_req(2).address = '%DB2:996.0:REAL';
    write_req(2).value = single(4.4);

    read_req(2) = reqType;
    read_req(1).name = 'OP1';
    read_req(1).address = '%DB2:0.0:REAL[66]';
    read_req(2).name = 'OP2';
    read_req(2).address = '%DB2:996.0:REAL';

    %read_req(2)= [];
end

%% build
function build()
    plc4mex('release'); 
    make_plc4mex; 
end

%% connect
function connect()
    plc4mex('connect');
end

%% write
function write(write_req)
    plc4mex('write', write_req);
end

%% read
function read_resp = read(read_req)
    read_resp = plc4mex('read', read_req);
end