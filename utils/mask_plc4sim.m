function mask_plc4sim(fcn, blk, varargin)

    hMask = Simulink.Mask.get(blk);
    switch fcn
        case 'portCallback'
            portCallback(blk, hMask, varargin{:})
        case 'numportsCallback'
            numportsCallback(blk, hMask, varargin{:})
        case 'transportCallback'
            transportCallback(blk, hMask, varargin{:});
        
        otherwise
    end

end

function numportsCallback(blk, hMask, side)

    switch side
        case 'ip'
            paramName = 'nInputs';
        case 'op'
            paramName = 'nOutputs';
        otherwise 
            error('bad callback args in inputsCallback')
    end
    
    param = getParameter(hMask,paramName);
    value =  get_param(blk, paramName);
    setIfNeeded(blk, param, value, false);
    formIOString(blk, hMask,side);
    
end

function portCallback(blk, hMask, postfix, side, portNo)

    name = [side num2str(portNo)  '_' postfix];
    param = getParameter(hMask,name);
    value =  get_param(blk, name);
    setIfNeeded(blk, param, value, false);
    formPortString(blk, hMask, param, value);
    
end

function transportCallback(blk, hMask)

    name = 'transport';
    param = getParameter(hMask,name);
    value =  get_param(blk, name);
    setIfNeeded(blk, param, value, false);
    formConnString(blk, hMask);
    
end

function formConnString(blk, hMask)

    transport = get_param(blk,'transport');
    protocol = get_param(blk,'protocol');
    ip = '0.0.0.0';
    port = '102';
    connValue = [lower(protocol) ':' lower(transport) '://' ip ':' port];
    connParam = getParameter(hMask,'connStr');
    setIfNeeded(blk, connParam, connValue, false);
    
end

function formPortString(blk, hMask, setParam, setValue)
    
    switch regexprep(setParam.Name, '^.+_','')
        case 'Type'
            dimsName = regexprep(setParam.Name,'_.+$','_Dims');
            posName = regexprep(setParam.Name,'_.+$','_Pos');
            dimsParam = get_param(blk,dimsName);
            posParam =  get_param(blk,posName);
            newString = [posParam ':' setValue '[' dimsParam ']'];
        case 'Dims'
            typeName = regexprep(setParam.Name,'_.+$','_Type');
            posName = regexprep(setParam.Name,'_.+$','_Pos');
            typeParam = get_param(blk,typeName);
            posParam =  get_param(blk,posName);
            newString = [posParam ':' typeParam '[' setValue ']'];
        case 'Pos'
            typeName = regexprep(setParam.Name,'_.+$','_Type');
            dimsName = regexprep(setParam.Name,'_.+$','_Dims');
            typeParam = get_param(blk,typeName);
            dimsParam = get_param(blk,dimsName);
            newString = [setValue ':' typeParam '[' dimsParam ']'];
        otherwise 
            error('Bad callback args')
    end
    
    
    stringName = regexprep(setParam.Name,'_.+$','_String');
    stringParam = hMask.getDialogControl(stringName);
    
    setIfNeededTxt(blk, stringParam, newString, true);
    
     
    switch regexprep(setParam.Name, '\d.+$','')
        case 'ip'
            formIOString(blk, hMask,'ip')
        case 'op'
            formIOString(blk, hMask, 'op')
        otherwise
             error('Bad callback args')
    end
end

function formIOString(blk, hMask, side)
    
    switch side 
        case 'ip'
            param = getParameter(hMask,'writeStr');
            n = get_param(blk,'nInputs');
            reqParamsNames = arrayfun(@(x) ['ip' num2str(x) '_String'],...
                1:str2double(n), 'uni',0);
        case 'op'
            param = getParameter(hMask,'readStr');
            n = get_param(blk,'nOutputs');
            reqParamsNames = arrayfun(@(x) ['op' num2str(x) '_String'],...
                1:str2double(n), 'uni',0);
        otherwise
            error('Bad callback args formIOString');
    end
    
    reqParams = cellfun(@(x) hMask.getDialogControl(x) ,reqParamsNames ,'uni',0);
    reqValues= cellfun(@(x) x.Prompt ,reqParams ,'uni',0);
    setIfNeeded(blk, param, strjoin(reqValues,';'), true);
end


function setIfNeeded(blk, param, new, force)
    current = param.Value ;
    if strcmp(current, new) == false
        set_param(blk, param.Name, new)
        if (force == true)
            param.set('Value', new);
        end
    end
end

function setIfNeededTxt(~, text, new, ~)
    current = text.Prompt ;
    if strcmp(current, new) == false
        text.Prompt=new;
    end
end      