function mask_maker(blk)

%blk  = 'untitled/Subsystem';

     
typeSelType = @(n) ['unidt({a=' num2str(n) '|||}{i=Inherit: auto}{b='...
    'double|single|int8|uint8|int16|uint16|int32|uint32|boolean}{u=bus})'];

transportOpts = {'TCP','UDP','Serial','SocketCAN','Raw Socket','PCAP Replay'};

protocolOpts = {'AB-ETH','ADS/AMS','BACnet/IP','CANopen','DeltaV','DF1',...
    'EtherNet/IP','Firmata','KNXnet/IP','Modbus','OPC UA','S7','Simulated'};

% Get the mask
maskObj = Simulink.Mask.get(blk);
if ~isempty(maskObj)
    maskObj.delete;
end

maskObj = Simulink.Mask.create(blk);

% Adding a descriton automatically adds some header content too
maskObj.Description = ['Use plc4sim block to commuincated with PLCs '...
    'via PLC4x. Setup the connection within the PLC4x tab.'];
maskObj.SelfModifiable = 'on';
maskObj.RunInitForIconRedraw = 'on';
maskObj.Help = 'web(help_plc4mat(''sfun''),''-browser'')';
maskObj.Type = 'plc4sim';
% Create a tab container
maskObj.addDialogControl('tabcontainer','allTabs');
tabs = maskObj.getDialogControl('allTabs');

maskObj.Initialization = [...
    "mask_plc4sim(''transportCallback'',gcb)"





%% Block Tab
blockTab = tabs.addDialogControl('tab','blockTab');
blockTab.Prompt = 'Block';

maskObj.addParameter('Name', 'ts','Prompt', 'Sample Time (s)',...
    'Type','edit', 'Value', '1',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('ts');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(blockTab);

maskObj.addParameter('Name', 'nInputs','Prompt', 'Inputs',...
    'Type','edit', 'Value', '1','Tunable', 'off',...
    'Callback', 'mask_plc4sim(''numportsCallback'',gcb, ''ip'');');
paramDialog = maskObj.getDialogControl('nInputs');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(blockTab);

maskObj.addParameter('Name', 'nOutputs','Prompt', 'Outputs',...
    'Type','edit', 'Value', '1','Tunable', 'off',  ...
    'Callback',  'mask_plc4sim(''numportsCallback'',gcb, ''op'');');
paramDialog = maskObj.getDialogControl('nOutputs');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(blockTab);

csvPanel = maskObj.addDialogControl('collapsiblepanel','csvPanel');
csvPanel.Prompt = 'Block CSV Paramtmers';
csvPanel.moveTo(blockTab);

maskObj.addParameter('Name', 'connStr','Prompt', 'Connection',...
    'Type','edit', 'Value', 's7:tcp://0.0.0.0:102','Enabled', 'off',...
    'Evaluate','off',  'Tunable', 'off');
paramDialog = maskObj.getDialogControl('connStr');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(csvPanel);

maskObj.addParameter('Name', 'writeStr','Prompt', 'Inputs',...
    'Type','edit', 'Value', '%DB1:0.0:single[1,1]','Enabled', 'off',...
    'Evaluate','off', 'Tunable', 'off');
paramDialog = maskObj.getDialogControl('writeStr');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(csvPanel);

maskObj.addParameter('Name', 'readStr','Prompt', 'Outputs',...
    'Type','edit', 'Value', '%DB1:0.0:single[1,1]','Enabled', 'off',...
    'Evaluate','off', 'Tunable', 'off');
paramDialog = maskObj.getDialogControl('readStr');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(csvPanel);

%% PLC4x tab
plc4xTab = tabs.addDialogControl('tab','plc4xTab');
plc4xTab.Prompt = 'PLC4x';

maskObj.addParameter('Name', 'transport','Prompt', 'Transport',...
    'Type','popup', 'TypeOptions',transportOpts, 'Value', 'TCP', ...
    'Tunable', 'off',  'Callback', 'mask_plc4sim(''transportCallback'',gcb);',...
    'Evaluate','off');
paramDialog = maskObj.getDialogControl('transport');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

maskObj.addParameter('Name', 'protocol','Prompt', 'Protocol',...
    'Type','popup', 'TypeOptions',protocolOpts, 'Value', 'S7', ...
    'Tunable', 'off',  'Callback',  'mask_plc4sim(''transportCallback'',gcb);',...
    'Evaluate','off');
paramDialog = maskObj.getDialogControl('protocol');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

maskObj.addParameter('Name', 'connItem1','Prompt', 'Conn Item 1',...
    'Type','edit', 'Value', '','Enabled', 'on', 'Evaluate','off',...
    'Tunable', 'off',  'Callback', '');
paramDialog = maskObj.getDialogControl('connItem1');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

maskObj.addParameter('Name', 'connItem2','Prompt', 'Conn Item 2',...
    'Type','edit', 'Value', '','Enabled', 'on', 'Evaluate','off',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('connItem2');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

maskObj.addParameter('Name', 'connItem3','Prompt', 'Conn Item 3',...
    'Type','edit', 'Value', '','Enabled', 'on', 'Evaluate','off',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('connItem3');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

maskObj.addParameter('Name', 'connItem4','Prompt', 'Conn Item 4',...
    'Type','edit', 'Value', '','Enabled', 'on', 'Evaluate','off',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('connItem4');
paramDialog.PromptLocation = 'left';
paramDialog.moveTo(plc4xTab);

%% Inputs tab, add type paramter and sub tab container
inTab = tabs.addDialogControl('tab','inTab');
inTab.Prompt = 'Inputs';

tidx = 13;

maskObj.addParameter('Name', 'ipTypeSelector','Prompt', 'Type',...
    'Type',typeSelType(tidx), 'Prompt', 'Type', 'Value', 'Inherit: auto',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('ipTypeSelector');
paramDialog.moveTo(inTab);

ipTabCon = maskObj.addDialogControl('tabcontainer','ip_Tab');
ipTabCon.moveTo(inTab);

addport(maskObj, ipTabCon, 'ip');

%% Outputs tab, add type paramter and sub tab container
outTab = tabs.addDialogControl('tab','outTab');
outTab.Prompt = 'Outputs';

nIp = 3 ;
nP = 32;
tidx = tidx + 1 + nIp * nP;

maskObj.addParameter('Name', 'opTypeSelector','Prompt', 'Type',...
    'Type',typeSelType(tidx), 'Prompt', 'Type', 'Value', 'Inherit: auto',...
    'Tunable', 'off');
paramDialog = maskObj.getDialogControl('opTypeSelector');
paramDialog.moveTo(outTab);

opTabCon = maskObj.addDialogControl('tabcontainer','op_Tab');
opTabCon.moveTo(outTab);

addport(maskObj, opTabCon, 'op');

%% Add the display 
maskObj.Display = sprintf('%s\n',[...
"fprintf('plc4sim\n\n%s\n%s\n\n%s',transport, protocol, connStr)"...
""...
"for n = 1 : nInputs"...
"    idx = str2num(get_param(gcb, ['ip' num2str(n) '_dims']));"...
"    idx = prod(idx(2:end),2);"...
"    posStr = regexprep(get_param(gcb,['ip' num2str(n) '_Pos']), '^%', '');"...
"    if idx > 1"...
"        port_label('input', n, [posStr '[' num2str(idx) ']'])"...
"    else"...
"        port_label('input', n, posStr)"...
"    end"...
"end"...
"for n = 1 : nOutputs"...
"    idx = str2num(get_param(gcb, ['op' num2str(n) '_dims']));"...
"    idx = prod(idx(2:end),2);"...
"    posStr = regexprep(get_param(gcb,['op' num2str(n) '_Pos']), '^%', '');"...
"    if idx > 1"...
"        port_label('output', n, [posStr '[' num2str(idx) ']'])"...
"    else"...
"        port_label('output', n, posStr)"...
"    end"...
"end"]);

end


function addport(maskObj, container, side)

    genPortCbk = @(type, side, idx) ['mask_plc4sim(''portCallback'', '...
                'gcb, ''' type ''', ''' side ''', ' num2str(idx) ')'];

    % Add a paramtmer to input output tabs
    for idx = 1 : 32

        portTab = container.addDialogControl('tab',[side num2str(idx) '_Tab']);
        portTab.Prompt = [upper(side) num2str(idx)];

        maskObj.addParameter('Name', [side num2str(idx) '_Type'],...
            'Type', 'edit', 'Prompt', 'Type',...
            'Value', 'double',   'Evaluate', 'off', 'Tunable', 'off',...
            'Callback', genPortCbk('Type', side, idx));

        maskObj.addParameter('Name', [side num2str(idx) '_Dims'], ...
            'Type', 'edit', 'Prompt', 'Dims','Value', '1,1', 'Evaluate', 'off', ...
            'Tunable', 'off', 'Callback', genPortCbk('Dims', side, idx));
        
        maskObj.addParameter('Name',[side num2str(idx) '_Pos'], ...
            'Type', 'edit', 'Prompt', 'Pos',...
            'Value', '%DB1:0.0', 'Evaluate', 'off', 'Tunable', 'off',...
            'Callback', genPortCbk('Pos', side, idx));

        stringPrompt = maskObj.addDialogControl('Type', 'text', 'Name',...
            [side num2str(idx) '_StringPrompt'],...
            'Prompt', ' Item');
        
        stringValue = maskObj.addDialogControl('Type', 'text', 'Name', ...
            [side num2str(idx) '_String'],...
            'Prompt', '%DB1:0.0:single');
        
        %string =  maskObj.addParameter('Name', [side num2str(idx) '_String'],...
        %    'Type', 'edit', 'Prompt', 'String',...
        %    'Value', '%DB1:0.0:single', 'Evaluate', 'off', 'Tunable', 'off',...
        %    'Enabled', 'off', 'Callback', 'disp(''TextField'')');
        
        paramDialog = maskObj.getDialogControl([side num2str(idx) '_Type']);
        paramDialog.PromptLocation = 'left';
        paramDialog.moveTo(portTab);

        paramDialog = maskObj.getDialogControl([side num2str(idx) '_Dims']);
        paramDialog.PromptLocation = 'left';
        paramDialog.Tooltip = [...
            'Specifcy -1 to inherit size via Simulink else set an array with' newline...
            '1st value being number of dims and next values being the' newline...
            'size of each dim. eg. 2,4,3 is 4x3 matrix, 1,1 is scalar.'];
        paramDialog.moveTo(portTab);

        paramDialog = maskObj.getDialogControl([side num2str(idx) '_Pos']);
        paramDialog.PromptLocation = 'left';
        paramDialog.Tooltip = 'Protocol dependant item string position on PLC';
        paramDialog.moveTo(portTab);
        
        stringPrompt.moveTo(portTab);
        stringValue.moveTo(portTab);
        stringValue.Row = 'current';

    end
end
