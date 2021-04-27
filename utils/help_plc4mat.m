function varargout = help_plc4mat(scenario)

if nargin == 0 
    scenario = '';
end

helpDir = 'docs';
mexHelp = 'readme';
simHelp = 'readme';
matHelp = 'readme';
prjRoot = fullfile(strrep(mfilename('fullpath'),mfilename,''),'..');

switch scenario 
    case 'sfun'
        varargout{1} = fullfile(prjRoot, helpDir, [simHelp '.html#plc4sim']); 
    case 'sim'
        web(fullfile(prjRoot, helpDir, [simHelp '.html']),'-browser');
    case 'mex'
        web(fullfile(prjRoot, helpDir, [mexHelp '.html']),'-browser');
    case 'mat'
        web(fullfile(prjRoot, helpDir, [matHelp '.html']),'-browser');
    otherwise
        disp('NA')
end

end

