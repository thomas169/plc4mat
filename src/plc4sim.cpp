/**************************************************************************
* File:             plc4sim.cpp
*
* Description:      Synchronous IO operations in Simulink using PLC4c
*
* Notes:            ...
*
* Revsions:         1.00 21/04/21 (tf) first release, no read arrays 
*
* See also:         make_plc4sim.m, test_plc4sim.m
*
* SPDX-License-Identifier: Apache-2.0
**************************************************************************/

// Preamble: defines & includes
#define S_FUNCTION_NAME plc4sim
#define S_FUNCTION_LEVEL 2

#include <iostream>
#include <unistd.h>
#include <cstddef>

#include <plc4c/driver_s7.h>
#include <plc4c/plc4c.h>
#include <plc4c/transport_tcp.h>
#include <plc4c/spi/types_private.h>

#include "simstruc.h"

#define PARAM_PTR(PIDX) (ssGetSFcnParam(S, PIDX))
#define PARAM_NUMEL(PIDX) (mxGetN(PARAM_PTR(PIDX))*mxGetM(PARAM_PTR(PIDX)))
#define PARAM_STRLEN(PIDX) ((PARAM_NUMEL(PIDX) + 1) * sizeof(char))
#define PARAM_VAL(PIDX) *mxGetPr(PARAM_PTR(PIDX))

char _INFO_[128];
char _ERROR_[128];


#define SET_INFO(...) snprintf(_INFO_,128,__VA_ARGS__)
#define SET_ERROR(...) snprintf(_ERROR_,128,__VA_ARGS__)
#define GET_ERROR() (_ERROR_)
#define GET_INFO() (_INFO_)
#define INFO(...) do {SET_INFO(__VA_ARGS__); ssPrintf(_INFO_);} while(0)
#define ERROR(...) do {SET_ERROR(__VA_ARGS__); ssSetErrorStatus(S,_ERROR_);return;} while(0)
#define WARNING(...) do {SET_INFO(__VA_ARGS__); ssWarning(S,_INFO_);} while(0)
#define ASSERT(chk, ...) do { if ((chk) == false) { ERROR(__VA_ARGS__); } } while (0)

#define N_PARAMS 6
#define P_TS 0
#define P_N_IN 1
#define P_N_OUT 2
#define P_CONNECTION 3
#define P_WRITES 4
#define P_READS 5

#define N_DWORK 4
#define DW_SYSTEM 0
#define DW_CONNECTION 1
#define DW_WRITES 2
#define DW_READS 3

#ifndef SS_STDIO_AVAILABLE
    #define SS_STDIO_AVAILABLE
#endif

// Add and remove optional s-functions
#ifdef MATLAB_MEX_FILE
    #define MDL_CHECK_PARAMETERS
    #define MDL_SET_INPUT_PORT_DIMENSION_INFO
    #define MDL_SET_INPUT_PORT_DATA_TYPE
    #define MDL_SET_OUTPUT_PORT_DIMENSION_INFO
    #define MDL_SET_OUTPUT_PORT_DATA_TYPE
    #define MDL_SET_DEFAULT_PORT_DATA_TYPES
    #define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
    #define MDL_SET_WORK_WIDTHS
    #define MDL_START 
    #define MDL_RTW
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define ssGetInputPortBytes(S,i) ssGetDataTypeSize(S, \
    ssGetInputPortDataType(S,i)) * ssGetInputPortWidth(S,i)
#define ssGetOutputPortBytes(S,i) ssGetDataTypeSize(S, \
    ssGetOutputPortDataType(S,i)) * ssGetOutputPortWidth(S,i)

// Function: typeNameIsBuiltIn ============================================
// Abstract: Check if the argument is a Simulink builtin type
int parsePortString(const char* portStr, DimsInfo_T* dimsInfo, char * const typeStr) {

    char dimsStr[strlen(portStr)];
    char *curPos, *token;
    curPos = strcpy(dimsStr, portStr);
    int idx, actDim, totalDim = 1;
    dimsInfo->nextSigDims = NULL;

    if (!curPos)
        return -1;

    // Skip over the DB
    strtok_r(curPos,":", &curPos);
    if ((!curPos) || (strlen(curPos) == 0))
        return -1;

    // Skip over the position
    strtok_r(curPos,":", &curPos);
    if ((!curPos) || (strlen(curPos) == 0))
        return -1;

    // Get the type
    token = strtok_r(curPos,"[", &curPos);
    if (!token)
        return -1;
    strncpy(typeStr, token, 64);
    
    // No brackes specified
    if (strlen(curPos) == 0) {
        dimsInfo->width = -1;
        dimsInfo->numDims = -1;
        dimsInfo->dims = NULL;
        return 0;
    }

    // Brackets specifed, get comma seperated tokens
    token = strtok_r(curPos, ",]", &curPos);
    dimsInfo->numDims = atoi(token);

    if (dimsInfo->numDims > 8) {
        SET_INFO("bad dimensions, no more than 8 dims allowed");
        return -1;
    } 

    if (dimsInfo->numDims == -1) {
        dimsInfo->numDims = -1;
        dimsInfo->width = -1;
        dimsInfo->dims = NULL;
        if  ((curPos) && (strlen(curPos) > 0))  {
            SET_INFO("bad dimensions if -1 numDims no more nums allowed");
            return -1;
        }
        return 0;
    }

    if ((curPos) && (strlen(curPos) > 0)) {
        for (idx = 0; idx < dimsInfo->numDims ; idx++) {
            token = strtok((idx == 0 ? curPos : NULL),",]");
            actDim = atoi(token);
            if (actDim == -1)
                totalDim = -1;
            else if (totalDim != -1)
                totalDim *= actDim;
            *(dimsInfo->dims+idx) = actDim;
        }
        dimsInfo->width = totalDim;
    } else {
        SET_INFO("bad dimensions specification, not enough tokens");
        return -1;
    }
    return 0;

}

// Function: typeNameIsBuiltIn ============================================
// Abstract: Check if the argument is a Simulink builtin type
static int typeNameIsBuiltIn(char* typeName) {
    if (!typeName)
        return INVALID_DTYPE_ID;
    else if (!strcmp(typeName, "double") || !strcasecmp(typeName, "lreal"))
        return SS_DOUBLE;
    else if (!strcmp(typeName, "single") || !strcasecmp(typeName, "real"))
        return SS_SINGLE;
    else if (!strcmp(typeName, "uint8") || !strcasecmp(typeName, "byte") || !strcasecmp(typeName, "usint"))
        return SS_UINT8;
    else if (!strcmp(typeName, "int8") || !strcasecmp(typeName, "sint"))
        return SS_INT8;
    else if (!strcmp(typeName, "uint16") || !strcasecmp(typeName, "word")  || !strcasecmp(typeName, "uint"))
        return SS_UINT16;
    else if (!strcmp(typeName, "int16") || !strcasecmp(typeName, "int"))
        return SS_INT16;
    else if (!strcmp(typeName, "uint32") || !strcasecmp(typeName, "dword") || !strcasecmp(typeName, "udint"))
        return SS_UINT32;
    else if (!strcmp(typeName, "int32") || !strcasecmp(typeName, "dint"))
        return SS_INT32;
    else if (!strcmp(typeName, "boolean") || !strcasecmp(typeName, "bool"))
        return SS_BOOLEAN;
    else if (!strcasecmp(typeName, "inherit"))
        return DYNAMICALLY_TYPED;
    else
        return INVALID_DTYPE_ID;
}


// Function: mdlCheckParameters ===========================================
// Abstract: Verify the mask paramters are valid, also called if run time 
// parameters change during simulation.
#ifdef MDL_CHECK_PARAMETERS
static void mdlCheckParameters(SimStruct *S){
    enum {PARAM = 0, NUM_PARAMS};
    if (!((ssGetSimMode(S) == SS_SIMMODE_SIZES_CALL_ONLY) \
            && mxIsEmpty(ssGetSFcnParam(S, PARAM)))){
        if (mxIsEmpty(ssGetSFcnParam(S, PARAM))) 
            ERROR("Invalid parameter specified (empty).");
    }
}
#endif

// Function: mdlInitializeSizes ===========================================
// Abstract: Verify the mask paramters are valid, also called if run time 
// parameters change during simulation.
static void mdlInitializeSizes(SimStruct *S) {
    
    DTypeId typeId; 
    int status;
    int nInput, nOutput;
    size_t i;
    DimsInfo_T dimInfo;
    int dimArray[8];
    char typeStr[64];
    char connStr[PARAM_STRLEN(P_CONNECTION)];
    char writeStr[PARAM_STRLEN(P_WRITES)];
    char readStr[PARAM_STRLEN(P_READS)];
    char *portStr, *portStrPtr;

    // PARAMETER DEFINITON ------------------------------------------------
    ssSetNumSFcnParams(S, N_PARAMS);

    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) 
        return;
    
    for (i = 0; i < N_PARAMS; i++) 
        ssSetSFcnParamTunable(S, i, 0);
    
    
    nInput = (int) PARAM_VAL(P_N_IN);
    nOutput = (int) PARAM_VAL(P_N_OUT);
    
    mxGetString(PARAM_PTR(P_CONNECTION), connStr, PARAM_STRLEN(P_CONNECTION));
    mxGetString(PARAM_PTR(P_WRITES), writeStr, PARAM_STRLEN(P_WRITES));
    mxGetString(PARAM_PTR(P_READS), readStr, PARAM_STRLEN(P_READS));
    
    // INPUT PORTS DEFINITION ---------------------------------------------
    // NOTE: ssRegisterDataTypeFromNamedExpr doesn't work with bus selector
    // so use ssRegisterTypeFromNamedObject with DIY builtin type guard.
    ssSetNumInputPorts(S, nInput);

    for (i = 0; i < nInput; i++) {

        if (i == 0) {
            portStrPtr = writeStr;
            ASSERT(strlen(portStrPtr) >= 1, "Not enough ip port strings!\n");
        }

        dimInfo.dims = dimArray;

        if (!(portStr = strtok_r(portStrPtr, ";", &portStrPtr)))
            ERROR("Not enough ip port strings!\n");

        if (parsePortString(portStr, &dimInfo, typeStr))
            ERROR("IP[%lu]: %s", i+1, GET_INFO());
        
        if ((typeId = typeNameIsBuiltIn(typeStr)) == INVALID_DTYPE_ID) {
            ssRegisterTypeFromNamedObject(S, typeStr, &typeId); 
            if (typeId == INVALID_DTYPE_ID) 
                ERROR("Bad IP type name <%s> specified!\n",typeStr);
        }
        ssSetInputPortDataType(S, i, typeId);
        ssSetInputPortDirectFeedThrough(S, i, 1);
        ssSetInputPortRequiredContiguous(S, i, 1);
        ssSetInputPortDimensionInfo(S, i, &dimInfo);
        if (ssIsDataTypeABus(S,typeId))
            ssSetBusInputAsStruct(S, i, typeId);
    }
    
    // OUTPUT PORTS DEFINITION --------------------------------------------
    ssSetNumOutputPorts(S, nOutput); 

    for (i = 0; i < nOutput; i++){

        if (i == 0) {
            portStrPtr = readStr;
            ASSERT(strlen(portStrPtr) >= 1, "Not enough op port strings!\n");
        }

        dimInfo.dims = dimArray;
        if (!(portStr = strtok_r(portStrPtr, ";", &portStrPtr)))
            ERROR("Not enough op port strings!\n");
        
        if (parsePortString(portStr, &dimInfo, typeStr))
            ERROR("OP[%lu]: %s", i+1, GET_INFO());
        
        if ((typeId = typeNameIsBuiltIn(&typeStr[0])) == INVALID_DTYPE_ID){
            ssRegisterTypeFromNamedObject(S, &typeStr[0], &typeId);
            if (typeId == INVALID_DTYPE_ID) 
                ERROR("Bad OP type name <%s> specified!\n", typeStr);
        }
        ssSetOutputPortDataType(S, i, typeId);
        ssSetOutputPortDimensionInfo(S, i, &dimInfo);
        if (ssIsDataTypeABus(S,typeId)) {
            ssSetBusOutputAsStruct(S, i, typeId);
            ssSetBusOutputObjectName(S, i, &typeStr[0]);
        }
    } 

    // D-WORK VECTOR DEFINITION -------------------------------------------
    ssSetNumDWork(S, N_DWORK);

    ssSetDWorkDataType(S, DW_SYSTEM, SS_POINTER);
    ssSetDWorkWidth(S, DW_SYSTEM, 1);
    ssSetDWorkComplexSignal(S, DW_SYSTEM, COMPLEX_NO);
    ssSetDWorkName(S, DW_SYSTEM, "DW_SYSTEM");

    ssSetDWorkDataType(S, DW_CONNECTION, SS_POINTER);
    ssSetDWorkWidth(S, DW_CONNECTION, 1);
    ssSetDWorkComplexSignal(S, DW_CONNECTION, COMPLEX_NO);
    ssSetDWorkName(S, DW_CONNECTION, "DW_CONNECTION");

    ssSetDWorkDataType(S, DW_WRITES, SS_POINTER);
    ssSetDWorkWidth(S, DW_WRITES, MAX(nInput, 1));
    ssSetDWorkComplexSignal(S, DW_WRITES, COMPLEX_NO);
    ssSetDWorkName(S, DW_WRITES, "DW_WRITES");

    ssSetDWorkDataType(S, DW_READS, SS_POINTER);
    ssSetDWorkWidth(S, DW_READS, MAX(nOutput, 1));
    ssSetDWorkComplexSignal(S, DW_READS, COMPLEX_NO);
    ssSetDWorkName(S, DW_READS, "DW_READS");

    // OTHER SIMULINK DEFINITIONS -----------------------------------------
    ssSetNumSampleTimes(S, 1);
    ssSetModelReferenceNormalModeSupport(S,DEFAULT_SUPPORT_FOR_NORMAL_MODE);
    ssSetSimStateCompliance(S, USE_DEFAULT_SIM_STATE);
    ssSetOptions(S, SS_OPTION_WORKS_WITH_CODE_REUSE | SS_OPTION_USE_TLC_WITH_ACCELERATOR);
    
    ssPrintf("SF Callback fired\n");
}

// Function: mdlSetInputPortWidth =========================================
// Abstract: ...
#ifdef MDL_SET_INPUT_PORT_DIMENSION_INFO
static void mdlSetInputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dims){
    ssSetInputPortDimensionInfo(S, port, dims);
}
#endif

// Function: mdlSetInputPortDataType ======================================
// Abstract: ...
#ifdef MDL_SET_INPUT_PORT_DATA_TYPE
void mdlSetInputPortDataType(SimStruct *S, int_T port, DTypeId id){
    ssSetInputPortDataType(S, port, id);
}
#endif

// Function: mdlSetOutputPortWidth ========================================
// Abstract: ...
#ifdef MDL_SET_OUTPUT_PORT_DIMENSION_INFO
void mdlSetOutputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dims){
    ssSetOutputPortDimensionInfo(S, port, dims);
}
#endif

// Function: mdlSetOutputPortDataType =====================================
// Abstract: ...
#ifdef MDL_SET_OUTPUT_PORT_DATA_TYPE
void mdlSetOutputPortDataType(SimStruct *S, int_T port, DTypeId id){
    ssSetOutputPortDataType(S, port, id);
}
#endif

// Function: mdlSetDefaultPortDimensionInfo ===============================
// Abstract: / If not specified (derivable) set scalar port dimensions
#ifdef MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S){

    // Default settings, a scalar
    int dimArray[1] = {1};
    DimsInfo_T dimInfo = {1,1, dimArray, NULL};

    for (int idx = 0 ; idx < ssGetNumInputPorts(S); idx++){
        if (ssGetInputPortWidth(S, idx) == DYNAMICALLY_SIZED){
            ssSetInputPortDimensionInfo(S, idx, &dimInfo);
            WARNING("Input port %d using default dims (scalar)", idx + 1);
        }
    }
    
    for (int idx = 0 ; idx < ssGetNumOutputPorts(S); idx++){
        if (ssGetOutputPortWidth(S, idx) == DYNAMICALLY_SIZED){
            ssSetOutputPortDimensionInfo(S, idx, &dimInfo);
            WARNING("Output port %d using default dims (scalar)", idx + 1);
        }
    }
}
#endif

// Function: mdlSetDefaultPortDataTypes ===================================
// Abstract: If not specified (or derivable) set double port data type
#ifdef MDL_SET_DEFAULT_PORT_DATA_TYPES
void mdlSetDefaultPortDataTypes(SimStruct *S){

    for (int idx = 0 ; idx < ssGetNumInputPorts(S); idx++){
        if (ssGetInputPortDataType(S, idx) == DYNAMICALLY_TYPED){
            ssSetInputPortDataType(S, idx, SS_DOUBLE);
            WARNING("Input port %d using default type (double)", idx+1);
        }
    }
    
    for (int idx = 0 ; idx < ssGetNumOutputPorts(S); idx++){
        if (ssGetOutputPortDataType(S, idx) == DYNAMICALLY_TYPED){
            ssSetOutputPortDataType(S, idx, SS_DOUBLE);
            WARNING("Output port %d using default type (double)", idx+1);
        }
    }
}
#endif

// Function: mdlSetWorkWidths =============================================
// Abstract: 
#ifdef MDL_SET_WORK_WIDTHS
void mdlSetWorkWidths(SimStruct *S) {
    //ssSetNumRunTimeParams(S, 1);
    //const char_T *rtpWrite = "JIMMY";
    //ssRegDlgParamAsRunTimeParam(S, 0, 0, rtpWrite, SS_DOUBLE);
}
#endif

// Function: mdlInitializeSampleTimes =====================================
// Abstract: Set the sample time, using one discrete block sample time.
static void mdlInitializeSampleTimes(SimStruct *S) {
    double sampleTime = PARAM_VAL(P_TS);   
    ssSetSampleTime(S, 0, sampleTime);
    ssSetOffsetTime(S, 0, 0);
    ssSetModelReferenceSampleTimeDefaultInheritance(S);
}

char* getPlcTypeStringFromTypeId(DTypeId id) {
    switch (id) {
        case SS_DOUBLE:
            return ((char*) "LREAL");
        case SS_SINGLE:
            return ((char*) "REAL");
        case SS_UINT8:
            return ((char*) "USINT");
        case SS_INT8:
            return ((char*) "SINT");
        case SS_UINT16:
            return ((char*) "UINT");
        case SS_INT16:
            return ((char*) "INT");
        case SS_UINT32:
            return ((char*) "ULINT");
        case SS_INT32:
            return ((char*) "LINT");
        case SS_BOOLEAN:
            return ((char*) "BIT");
        default:
            return ((char*) "USINT");
    }
}

void setPortStringWorkVector(char **vwp, DTypeId typeId, int width, char* pos) {

    char widthStr[5];
    char typeStr[32];
    int idx = 0;
    sprintf(widthStr,"%d", width);
    strcpy(typeStr,getPlcTypeStringFromTypeId(typeId));

    *vwp = (char*) calloc(32 + 4 + strlen(pos) , sizeof(char));

    strncpy(*vwp, pos, strlen(pos));
    idx += strlen(pos);
    
    *(*vwp + idx) = ':';
    idx += 1;

    strncpy(*vwp + idx, typeStr, 5);
    idx += strlen(typeStr);

    *(*vwp + idx) = '[';
    idx += 1;

    strncpy(*vwp + idx, widthStr, 32);
    idx += strlen(widthStr);
    
    *(*vwp + idx) = ']';
    
    idx += 1;
    
    *(*vwp + idx) = '\0';

}

int findCharInstanceIdx(char *str, char find, int idx) {
    // zero index'd return values indicates nth instance found
    int i;
    char* pcl = str;
    for (i = 0 ; i < idx ; i++ ) {
        pcl = strchr(pcl,  find);
        if ( (pcl) && (i < idx -1) )
            pcl++;
        else if (!pcl)
            return (-i-1);
    }
    *pcl = '\0';
    return 0;
}
// Function: mdlStart =====================================================
// Abstract: Do one shot heavy lifting, such as opening files and sockets 
// and setting work vectors for the mdlOutputs code.
#ifdef MDL_START
static void mdlStart(SimStruct *S) {

    int nIn, nOut;
    char *portPtr, *portToken;
    char readStr[PARAM_STRLEN(P_READS)];
    char writeStr[PARAM_STRLEN(P_WRITES)];

    nIn = ssGetNumInputPorts(S);
    nOut = ssGetNumOutputPorts(S);

    char** writes = (char**) ssGetDWork(S,DW_WRITES);
    char** reads = (char**) ssGetDWork(S,DW_READS);
    
    plc4c_system** system  = (plc4c_system**) ssGetDWork(S,DW_SYSTEM);
    plc4c_connection** connection = (plc4c_connection**) ssGetDWork(S,DW_CONNECTION);

    size_t idx;
    DTypeId typeId;

    int width;
    mxGetString(PARAM_PTR(P_READS), readStr, PARAM_STRLEN(P_READS));
    mxGetString(PARAM_PTR(P_WRITES), writeStr, PARAM_STRLEN(P_WRITES));

    for (idx = 0 ; idx < nIn ; idx++) {
        portToken = strtok_r(idx == 0 ? writeStr : portPtr, ";", &portPtr);
        
        if (findCharInstanceIdx(portToken, ':', 2))
            ERROR("failed to find port tokens");

        typeId = ssGetInputPortDataType(S,idx);
        if ssIsDataTypeABus(S,typeId)
            width = ssGetOutputPortBytes(S, idx);
        else
            width = ssGetOutputPortWidth(S, idx);
        setPortStringWorkVector(&writes[idx], typeId,  width, portToken);
        INFO("Write %lu: %s\n",idx, writes[idx]);
    }

    for (idx = 0 ; idx < nOut ; idx++) {
        portToken = strtok_r(idx == 0 ? readStr : portPtr, ";", &portPtr);
        if (findCharInstanceIdx(portToken, ':', 2))
            ERROR("failed to find port tokens");
        typeId = ssGetOutputPortDataType(S,idx);
        if ssIsDataTypeABus(S,typeId)
            width = ssGetOutputPortBytes(S, idx);
        else
            width = ssGetOutputPortWidth(S, idx);
        setPortStringWorkVector(&reads[idx], typeId,  width, portToken);
        INFO("Read %lu: %s\n",idx, reads[idx]);
    }
    
    // Connect 
    char connStr[30];  

    plc4c_return_code result;

    strcpy(connStr ,"s7:tcp://0.0.0.0:102");
    result = plc4c_system_create(system);
    ASSERT(result == OK, "plc4c_system_create failed");
    plc4c_driver *s7_driver = plc4c_driver_s7_create();
    result = plc4c_system_add_driver(*system, s7_driver);
    ASSERT(result == OK, "plc4c_system_add_driver failed");
    plc4c_transport *tcp_transport = plc4c_transport_tcp_create();
    result = plc4c_system_add_transport(*system, tcp_transport);
    ASSERT(result == OK, "plc4c_system_add_transport failed");
    result = plc4c_system_init(*system);
    ASSERT(result == OK, "plc4c_system_init failed");
    result = plc4c_system_connect(*system, connStr, connection);
    ASSERT(result == OK, "plc4c_system_connect failed");
    while (1) {
        plc4c_system_loop(*system);
        if (plc4c_connection_get_connected(*connection))
            break;
        else if (plc4c_connection_has_error(*connection))
            return;
    }
    INFO("connected");
}
#endif

typedef union {
    double *dbl;
    float *flt;
    uint8_t *ub;
    int8_t *sb;
    uint16_t *us;
    int16_t *ss;
    uint32_t *ui;
    int32_t *si;
    uint64_t *ul;
    int64_t *sl;
    bool *bit;
    void *misc;
} unionvalueptr;


typedef union {
    double dbl;
    float flt;
    uint8_t ub;
    int8_t sb;
    uint16_t us;
    int16_t ss;
    uint32_t ui;
    int32_t si;
    uint64_t ul;
    int64_t sl;
    bool bit;
} unionvalue;

plc4c_data* encodeWriteData(SimStruct *S, size_t port) {
    
    DTypeId dt = ssGetInputPortDataType(S, port);
    int nElem = ssGetInputPortWidth(S, port);
    unionvalue *sigPtrs = (unionvalue*) ssGetInputPortSignal(S, port);
    
    switch (dt) {
        case SS_DOUBLE:
            if (nElem > 1) 
                return (plc4c_data_create_double_array(&sigPtrs->dbl, nElem));
            else 
                return (plc4c_data_create_double_data(sigPtrs->dbl));
        case SS_SINGLE:
            if (nElem > 1) 
                return (plc4c_data_create_float_array(&sigPtrs->flt, nElem));
            else
                return (plc4c_data_create_float_data(sigPtrs->flt));
        case SS_INT8:
            if (nElem > 1) 
                return (plc4c_data_create_int8_t_array(&sigPtrs->sb, nElem));
            else
                return (plc4c_data_create_int8_t_data(sigPtrs->sb));
        case SS_UINT8:
            if (nElem > 1) 
                return (plc4c_data_create_uint8_t_array(&sigPtrs->ub, nElem));
            else
                return (plc4c_data_create_uint8_t_data(sigPtrs->ub));
        case SS_INT16:
            if (nElem > 1) 
                return (plc4c_data_create_int16_t_array(&sigPtrs->ss, nElem));
            else
                return (plc4c_data_create_int16_t_data(sigPtrs->ss));
        case SS_UINT16:
            if (nElem > 1) 
                return (plc4c_data_create_uint16_t_array(&sigPtrs->us, nElem));
            else
                return (plc4c_data_create_uint16_t_data(sigPtrs->us));
        case SS_INT32:
            if (nElem > 1) 
                return (plc4c_data_create_int32_t_array(&sigPtrs->si, nElem));
            else
                return (plc4c_data_create_int32_t_data(sigPtrs->si));
        case SS_UINT32:
            if (nElem > 1) 
                return (plc4c_data_create_uint32_t_array(&sigPtrs->ui, nElem));
            else
                return (plc4c_data_create_uint32_t_data(sigPtrs->ui));
        case SS_BOOLEAN:
            if (nElem > 1) 
                return (plc4c_data_create_bool_array(&sigPtrs->bit, nElem));
            else
                return (plc4c_data_create_bool_data(sigPtrs->bit));
        default: 
            nElem = ssGetInputPortBytes(S,port);
            if (nElem > 1) 
                return (plc4c_data_create_uint8_t_array(&sigPtrs->ub, nElem));
            else
                return (plc4c_data_create_uint8_t_data(sigPtrs->ub));
               
            // its a bus so proably getting complex 

        
    }
}

void decodeReadData(SimStruct *S, size_t port, plc4c_read_response* responce) {
    
    DTypeId dtIdx;
    int nElem, idx;
    void *sigPtrs;
    plc4c_data *itemData, *responceData;

    dtIdx = ssGetOutputPortDataType(S, port);
    nElem = ssGetOutputPortWidth(S, port);
    sigPtrs = ssGetOutputPortSignal(S, port);
    responceData = (plc4c_data *) ((plc4c_response_value_item *) 
        plc4c_utils_list_get_value(responce->items, port))->value;

    switch (dtIdx) {
        case SS_DOUBLE:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((double*)sigPtrs)[idx] = itemData->data.double_value;
            }
            break;
        case SS_SINGLE:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((float*)sigPtrs)[idx] = itemData->data.float_value;
            }
            break;
        case SS_INT8:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((int8_t*)sigPtrs)[idx] = itemData->data.char_value;
            }
            break;
        case SS_UINT8:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((uint8_t*)sigPtrs)[idx] = itemData->data.uchar_value;
            }
            break;
        case SS_INT16:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((int16_t*)sigPtrs)[idx] = itemData->data.short_value;
            }
            break;
        case SS_UINT16:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((uint16_t*)sigPtrs)[idx] = itemData->data.ushort_value;
            }
            break;
        case SS_INT32:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((int32_t*)sigPtrs)[idx] = itemData->data.int_value;
            }
            break;
        case SS_UINT32:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((uint32_t*)sigPtrs)[idx] = itemData->data.uint_value;
            }
            break;
        case SS_BOOLEAN:
            for (idx = 0 ; idx < nElem ; idx++) {
                itemData = nElem > 1 ? (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx) : responceData;
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((bool*)sigPtrs)[idx] = itemData->data.boolean_value;
            }
            break;
        default: 
            // its a bus encoded as a uint8_t array
            nElem = ssGetOutputPortBytes(S,port);
            for (idx = 0 ; idx <  nElem ; idx++) {
                itemData = (plc4c_data*) plc4c_utils_list_get_value(
                    &responceData->data.list_value, idx);
                ASSERT(itemData != NULL, "invalid data for outputs");
                ((uint8_t*)sigPtrs)[idx] = itemData->data.uchar_value;
            }
            break;

           
               
    }
}
// Function: mdlOutputs ===================================================
// Abstract: Use the inputs to write to the PLC and set the outputs once we
// have read data from the PLC. Data must be also cast to relevant type.
static void mdlOutputs(SimStruct *S, int_T tid) {
    
    int nIn, nOut, idx;
    char **writes, **reads;
    plc4c_data* data; 
    plc4c_return_code result;
    plc4c_write_request* write_request;
    plc4c_write_request_execution* write_execution;
    plc4c_write_response *write_response;
    plc4c_read_request* read_request;
    plc4c_read_request_execution* read_execution;
    plc4c_read_response *read_response;

    int xk= 3;
    plc4c_system* system  = *(plc4c_system**) ssGetDWork(S,DW_SYSTEM);
    plc4c_connection* connection = *(plc4c_connection**) ssGetDWork(S,DW_CONNECTION);

    nIn = ssGetNumInputPorts(S);
    nOut = ssGetNumOutputPorts(S);
    writes = (char**) ssGetDWork(S,DW_WRITES);
    reads = (char**) ssGetDWork(S,DW_READS);


    // Inputs and write requests
    result = plc4c_connection_create_write_request(connection, &write_request);
    ASSERT(result == OK, "plc4c_connection_create_write_request failed");
    for (idx = 0 ; idx < nIn ; idx++) {
        data = encodeWriteData(S, idx);
        result = plc4c_write_request_add_item(write_request, writes[idx] , data);
        ASSERT(result == OK,"plc4c_write_request_add_item failed");
    }

    result = plc4c_write_request_execute(write_request, &write_execution);
    ASSERT(result == OK, "plc4c_write_request_execute failed");
    
    while (1) {
        result = plc4c_system_loop(system);
        ASSERT(result == OK,"plc4c_system_loop failed");
        if (plc4c_write_request_check_finished_successfully(write_execution))
            break;
        else if (plc4c_write_request_execution_check_completed_with_error(write_execution))
            ERROR("write execution failed");
    } 
    
    // Outputs and read requests
    result = plc4c_connection_create_read_request(connection, &read_request);
    ASSERT(result == OK, "plc4c_connection_create_read_request failed");
    
    char namer[32];
    for (idx = 0 ; idx < nOut ; idx++) {
        sprintf(namer, "Port%d",idx);
        result = plc4c_read_request_add_item(read_request,namer , reads[idx]);
        ASSERT(result == OK,"plc4c_read_request_add_item failed");
    }

    result = plc4c_read_request_execute(read_request, &read_execution);
    ASSERT(result == OK, "plc4c_read_request_execute failed");

    while (1) {
        result = plc4c_system_loop(system);
        ASSERT(result == OK,"plc4c_system_loop failed");
        if (plc4c_read_request_execution_check_finished_successfully(read_execution))
            break;
        else if (plc4c_read_request_execution_check_finished_with_error(read_execution))
            ERROR("read execution failed");
    } 

    
    // Perform the write / read
    /*while (1) {
        result = plc4c_system_loop(system);
        ASSERT(result == OK,"plc4c_system_loop failed");
        if ((plc4c_write_request_check_finished_successfully(write_execution)) &&
            (plc4c_read_request_execution_check_finished_successfully(read_execution)))
            break;
        else if (plc4c_read_request_execution_check_finished_with_error(read_execution))
            ERROR("read execution failed");
        else if (plc4c_write_request_execution_check_completed_with_error(write_execution))
            ERROR("write execution failed");
    }*/

    write_response = plc4c_write_request_execution_get_response(write_execution);
    read_response = plc4c_read_request_execution_get_response(read_execution);
    ASSERT(write_response != NULL, "plc4c_write_request_execution_get_response failed");
    ASSERT(read_response != NULL, "plc4c_read_request_execution_get_response failed");

    
    for (idx = 0 ; idx < nOut ; idx++) {
        decodeReadData(S, idx, read_response);
    }

    // Clean up
    plc4c_write_destroy_write_response(write_response);
    plc4c_write_request_execution_destroy(write_execution);
    plc4c_write_request_destroy(write_request);

    plc4c_read_destroy_read_response(read_response);
    plc4c_read_request_execution_destroy(read_execution);
    plc4c_read_request_destroy(read_request);
}

// Function: mdlTerminate =================================================
// Abstract: free allocated memory and work vectors, and close files
static void mdlTerminate(SimStruct *S) {

    int nIn, nOut, idx;
    char **writes, **reads;
    plc4c_system* system;  
    plc4c_connection* connection; 
    plc4c_return_code result;

    nIn = ssGetNumInputPorts(S);
    nOut = ssGetNumOutputPorts(S);

    writes = (char**) ssGetDWork(S,DW_READS);
    reads = (char**) ssGetDWork(S,DW_WRITES);
    system  = *((plc4c_system**) ssGetDWork(S,DW_SYSTEM));
    connection = *((plc4c_connection**) ssGetDWork(S,DW_CONNECTION));

    for (idx = 0 ; idx < nIn ; idx++) 
        free(writes[idx]);
    
    for (idx = 0 ; idx < nOut ; idx++) 
        free(reads[idx]);
    
    result = plc4c_connection_disconnect(connection);
    ASSERT(result == OK,"plc4c_connection_disconnect failed");

    unsigned long lidx;

    while (1) {
        plc4c_system_loop(system);
        if (!plc4c_connection_get_connected(connection))
            break;
        else if (plc4c_connection_has_error(connection))
            ASSERT(false, "plc4c_connection_has_error");
        else if (lidx >1000)
            break;
        else
            lidx++;
    }

    plc4c_system_remove_connection(system, connection);
    plc4c_connection_destroy(connection);
    plc4c_system_shutdown(system);
    plc4c_system_destroy(system);

}

// Function: mdlRTW =======================================================
// Abstract: ...
#ifdef MDL_RTW
void mdlRTW(SimStruct *S) {
}
#endif

// Required S-function trailer ============================================
#ifdef MATLAB_MEX_FILE
    #include "simulink.c"
#else
    #include "cg_sfun.h"
#endif
