/**************************************************************************
* File:             plc4c.c
*
* Description:      Synchronous IO operations in MATLAB using PLC4c
*
* Notes:            Only one instanace (connection) in MATLAB at a time. 
*                   To create a new one call 'release' and then 'clear mex'
*
* Revsions:         1.00 14/04/21 (tf) first release, no read arrays 
*
* See also:         make_plc4c.m, test_plc4c.m
*
* SPDX-License-Identifier: Apache-2.0
**************************************************************************/

#include "mex.hpp"
#include "mexAdapter.hpp"

#include <iostream>
#include <unistd.h>
#include <cstddef>

#include <plc4c/driver_s7.h>
#include <plc4c/plc4c.h>
#include <plc4c/transport_tcp.h>
#include <plc4c/spi/types_private.h>

#define ASSERT(chk, fs)                                                     \
    do {                                                                    \
        if ((chk) == false) {                                               \
            ArrayFactory _f;                                                \
            std::shared_ptr<matlab::engine::MATLABEngine> _e = getEngine(); \
            _e->feval(u"error", 0, std::vector<Array>({_f.createScalar(fs)}));\
        }                                                                   \
    } while (0)

#define DISP(fs)                                                            \
    do {                                                                    \
        ArrayFactory _f;                                                    \
        std::shared_ptr<matlab::engine::MATLABEngine> _e = getEngine();     \
        _e->feval(u"disp", 0, std::vector<Array>({_f.createScalar(fs)}));   \
    } while (0)

#define ERROR(fs)                                                           \
    do {                                                                    \
        ArrayFactory _f;                                                    \
        std::shared_ptr<matlab::engine::MATLABEngine> _e = getEngine();     \
        _e->feval(u"error", 0, std::vector<Array>({_f.createScalar(fs)}));  \
    } while (0)

using namespace matlab::data;
using matlab::mex::ArgumentList;

class MexFunction : public matlab::mex::Function {
    public:
        void operator()(ArgumentList outputs, ArgumentList inputs);
        MexFunction() { mexLock(); }
        ~MexFunction() {}
    private:
        char connStr[254];
        bool connected = false;
        void connect(ArgumentList inputs);
        void release() { mexUnlock(); }
        void read(ArgumentList inputs, ArgumentList outputs);
        void write(ArgumentList inputs);
        void disconnect();
        void status();
        bool isValueType(ArrayType type);
        bool isWordType(ArrayType type);
        void checkWriteArgs(ArgumentList inputs);
        void checkReadArgs(ArgumentList inputs);
        StructArray formatReadArgs(ArgumentList inputs);
        StructArray formatWriteArgs(ArgumentList inputs);
        plc4c_data* encodeWriteData(StructArray data, size_t idx);
        Array decodeReadData(plc4c_data* responce_data);
        plc4c_system* system = nullptr;
        plc4c_connection* connection = nullptr;
        plc4c_return_code result;
};


void MexFunction::status()
{
    std::cout << connected << std::endl;
    std::cout << connStr << std::endl;
}

void MexFunction::disconnect()
{
    ASSERT(connected, "must be connected to disconnect");

    result = plc4c_connection_disconnect(connection);
    ASSERT(result == OK,"plc4c_connection_disconnect failed");

    while (1) {
        plc4c_system_loop(system);
        if (!plc4c_connection_get_connected(connection))
            break;
        else if (plc4c_connection_has_error(connection))
            ASSERT(false, "plc4c_connection_has_error");
    }
    connected = false;
    std::cout << "Disconencted!" << std::endl;

    plc4c_system_remove_connection(system, connection);
    plc4c_connection_destroy(connection);
    plc4c_system_shutdown(system);
    plc4c_system_destroy(system);
}

void MexFunction::connect(ArgumentList inputs)
{
    DISP("Connecting");
    ASSERT(!connected, "must be disconnected to connected");
    if (inputs.size() == 2)
        strcpy(connStr, ((CharArray)inputs[1]).toAscii().c_str());
    else
        strcpy(connStr ,"s7:tcp://0.0.0.0:102");
    
    result = plc4c_system_create(&system);
    ASSERT(result == OK, "plc4c_system_create failed");
    plc4c_driver *s7_driver = plc4c_driver_s7_create();
    result = plc4c_system_add_driver(system, s7_driver);
    ASSERT(result == OK, "plc4c_system_add_driver failed");
    plc4c_transport *tcp_transport = plc4c_transport_tcp_create();
    result = plc4c_system_add_transport(system, tcp_transport);
    ASSERT(result == OK, "plc4c_system_add_transport failed");
    result = plc4c_system_init(system);
    ASSERT(result == OK, "plc4c_system_init failed");
    result = plc4c_system_connect(system, connStr, &connection);
    ASSERT(result == OK, "plc4c_system_connect failed");

    while (1) {
        plc4c_system_loop(system);
        if (plc4c_connection_get_connected(connection))
            break;
        else if (plc4c_connection_has_error(connection))
            return;
    }
    DISP("connected");
    connected = true;
}



bool MexFunction::isValueType(ArrayType type)
{
    switch (type) {
        case ArrayType::DOUBLE:
        case ArrayType::SINGLE:
        case ArrayType::INT8:
        case ArrayType::UINT8:
        case ArrayType::INT16:
        case ArrayType::UINT16:
        case ArrayType::INT32:
        case ArrayType::UINT32:
        case ArrayType::INT64:
        case ArrayType::UINT64:
        case ArrayType::LOGICAL:
            return true;
        default:
            return false;
    }
}

bool MexFunction::isWordType(ArrayType type)
{
    switch (type) {
        case ArrayType::CHAR:
        case ArrayType::MATLAB_STRING:
            return true;
        default:
            return false;
    }
}


StructArray MexFunction::formatWriteArgs(ArgumentList inputs)
{
    // for a write we can have:
    // 1) name value pairs. ie address strings and values
    // 2) two cell arrays. address in first cell, value in second cell
    // 3) one structures array. fields required are "address" & "value"
    ArrayFactory factory;
    size_t idx;

    switch (inputs[1].getType()) {
        case ArrayType::CELL: {
            ASSERT((inputs[2].getType() == ArrayType::CELL),
                "if first arg is cell so must 2nd");
            ASSERT((inputs[1].getNumberOfElements() == 
                inputs[2].getNumberOfElements()), "both 1st and 2nd arg "
                "cells must be same size");
            StructArray sa = factory.createStructArray({1, 
             inputs[1].getNumberOfElements()},{"name", "address", "value"});
            for (idx = 0; idx < inputs[1].getNumberOfElements(); idx++) {
                CharArray addr = inputs[1][idx];
                Array values = inputs[2][idx];
                sa[idx]["address"] = addr;
                sa[idx]["name"] = factory.createCharArray("HelloWorld");
                sa[idx]["value"] = values;
            }
            return sa;
            break;
        }
        case ArrayType::STRUCT: {
            ASSERT(inputs.size() == 2, "only two args if structure");
            return ((StructArray)inputs[1]);
            break;
        }
        case ArrayType::CHAR:
        case ArrayType::MATLAB_STRING: {
            for (int i = 1 ; i < inputs.size(); i += 2) {
                if (!isWordType(inputs[i].getType()))
                    ERROR("must be strings or chars");
                if (!isValueType(inputs[i+1].getType()))
                    ERROR("must be value type array");
            }
            break;
        }
        default:
            ASSERT(1, "Input format/types incorrect");
    }
}

StructArray MexFunction::formatReadArgs(ArgumentList inputs)
{
    // for a write we can have:
    // 1) multiple name arguments of address strings
    // 2) one cell arrays of address 
    // 3) one structures array. fields required are "address" & "value", 
    // if so a structure will be returned
    // in all case we convert the inputs to a structure array for subsquent
    // functions
    ArrayFactory factory;                                               
    size_t idx;

    switch (inputs[1].getType()) {
        case ArrayType::CELL: {
            ASSERT(inputs.size() == 2, "only two args required");
            StructArray sa = factory.createStructArray({1, 
             inputs[1].getNumberOfElements()},{"name", "address", "value"});
            for (idx = 0; idx < inputs[1].getNumberOfElements(); idx++) {
                CharArray addr = inputs[1][idx];
                sa[idx]["address"] = addr;
                sa[idx]["name"] = factory.createCharArray("HelloWorld");
            }
            return sa;
        }
        
        case ArrayType::STRUCT: {
            ASSERT(inputs.size() == 2, "only two args required 2");
            bool hasAddr = false;
            bool hasValue = false;
            bool hasName = false;
            auto a = ((StructArray)inputs[1]).getFieldNames();
            std::vector<std::string> fieldNames(a.begin(), a.end());
            for (auto fn : fieldNames)
                if (fn == "address")
                    hasAddr = true;
                else if (fn == "name")
                    hasName = true;
                else if (fn == "value") 
                    hasValue = true;
            ASSERT(hasAddr && hasName && hasValue, "struture "
                "requires 'address', 'name' and 'value' fields.");
            return ((StructArray)inputs[1]);
        }
        case ArrayType::CHAR:
        case ArrayType::MATLAB_STRING: {
            for (idx = 1 ; idx < inputs.size(); idx++) 
                if (!isWordType(inputs[idx].getType()))
                    ERROR("must be strings or chars");
            StructArray sa = factory.createStructArray({1,inputs.size()-(size_t)1},
             {"name", "address", "value"});
            for (idx = 1; idx < inputs.size(); idx++) {
                CharArray addr = inputs[idx];
                sa[idx]["address"] = addr;
                sa[idx]["name"] = factory.createCharArray("HelloWorld");
            }
            return sa;
        }
        default:
            ASSERT(1, "Input format/types incorrect");
    }
}

plc4c_data* MexFunction::encodeWriteData(StructArray data, size_t idx)
{
    
    Array addr = data[idx]["address"];
    Array values = data[idx]["value"];
    size_t nElem = values.getNumberOfElements();
    size_t i;

    switch (ArrayType::SINGLE) {
        
        case ArrayType::DOUBLE:{
            if (nElem > 1) {
                float vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_float_array(vs, nElem));
            } else {
                return (plc4c_data_create_float_data(values[0]));
            }
        }

        case ArrayType::SINGLE:{
            if (nElem > 1) {
                float vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_float_array(vs, nElem));
            } else {
                return (plc4c_data_create_float_data(values[0]));
            }
        }

        case ArrayType::INT8:{
            if (nElem > 1) {
                int8_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_int8_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_int8_t_data(values[0]));
            }
        }

        case ArrayType::UINT8:{
            if (nElem > 1) {
                uint8_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_uint8_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_uint8_t_data(values[0]));
            }
        }

        case ArrayType::INT16:{
            if (nElem > 1) {
                int16_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_int16_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_int16_t_data(values[0]));
            }
        }
        case ArrayType::UINT16:{
            if (nElem > 1) {
                uint16_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_uint16_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_uint16_t_data(values[0]));
            }
        }
        case ArrayType::INT32:{
            if (nElem > 1) {
                int32_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_int32_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_int32_t_data(values[0]));
            }
        }
        case ArrayType::UINT32:{
            if (nElem > 1) {
                uint32_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_uint32_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_uint32_t_data(values[0]));
            }
        }
        case ArrayType::INT64:{
            if (nElem > 1) {
                int64_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_int64_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_int64_t_data(values[0]));
            }
        }
        case ArrayType::UINT64:{
            if (nElem > 1) {
                uint64_t vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return (plc4c_data_create_uint64_t_array(vs, nElem));
            } else {
                return (plc4c_data_create_uint64_t_data(values[0]));
            }
        }
        case ArrayType::LOGICAL:{
            if (nElem > 1) {
                bool vs[nElem];
                for (i = 0 ; i < nElem ; i++)
                    vs[i] = values[i];
                return plc4c_data_create_bool_data_array(vs, nElem));
            } else {
                return (plc4c_data_create_bool_data(values[0]));
            }
        }
        default:
            return nullptr;
    }
}


Array MexFunction::decodeReadData(plc4c_data* responce)
{
    
    
    ArrayFactory factory;
    size_t bytes;
    
    switch (responce->data_type) {
        /*case PLC4C_BOOL: {
            TypedArray<bool> b = factory.createArray(responce_data->size);
            return (b);
        }

        case PLC4C_CHAR: {
            return (nullptr);
        }

        case PLC4C_UCHAR: {
            return (nullptr);
        }

        case PLC4C_SHORT: {
            return (nullptr);
        }

        case PLC4C_USHORT: {
            return (nullptr);
        }

        case PLC4C_INT: {
            return (nullptr);
        }

        case PLC4C_UINT: {
            return (nullptr);
        }

        case PLC4C_LINT: {
            return (nullptr);
        }

        case PLC4C_ULINT: {
            return (nullptr);
        }
*/
        case PLC4C_FLOAT: {
            std::cout << responce->size;
            TypedArray<float> a = factory.createArray<float>({1,1},
                &(responce->data.float_value), ((&(responce->data.float_value))+8) );
            return (a);

        }
/*
        case PLC4C_DOUBLE: {
            return (nullptr);
        }

        case PLC4C_STRING_POINTER: {
            return (nullptr);
        }

        case PLC4C_CONSTANT_STRING: {
            return (nullptr);
        }

        case PLC4C_VOID_POINTER: {
            return (nullptr);
        }
        */
        case PLC4C_LIST: {
            plc4c_list_element* list_item;
            plc4c_data* listdata;
            size_t x = 0;
            size_t nList;
            plc4c_data_type type;
            bool homogenous = true;

            list_item = plc4c_utils_list_tail(&responce->data.list_value);
            nList = plc4c_utils_list_size(&responce->data.list_value);

            listdata = (plc4c_data*)(list_item->value);
            type = listdata->data_type;

            do {
                if (!list_item)
                    break;
                listdata = (plc4c_data*)(list_item->value);
                if (type != listdata->data_type) {
                    homogenous = false;
                    break;
                }
                list_item = list_item->next;
                x++;
            } while (x < nList);

            list_item = plc4c_utils_list_tail(&responce->data.list_value);
            x = 0;
            if (!homogenous) {
                float nl[nList];
                do {
                    listdata = (plc4c_data*)(list_item->value);
                    std::cout << "adding to nl " << listdata->data.float_value << std::endl;
                    nl[x++] = listdata->data.float_value;
                    list_item = list_item->next;
                } while (x < nList);
                TypedArray<float> a = factory.createArray<float>({1,nList}, &nl[0], &nl[nList]);
                return (a);
            } else {
                TypedArray<float> ca  = factory.createArray<float>({1,nList});
                float nl[nList];
                do {
                    listdata = (plc4c_data*)(list_item->value);
                    std::cout << "adding to nl " << listdata->data.float_value << std::endl;
                    nl[x] = listdata->data.float_value;
                    list_item = list_item->next;
                    //ca[0][x] = factory.createScalar<float>(nl[x]);
                    ca[0][x] = nl[x];
                    x++;
                } while (x < nList);
                return (ca);
            }
        }
        
/*
        case PLC4C_STRUC: {
            return (nullptr);
        }
*/
        default:
            return factory.createArray<bool>({1,1});
    }
}


void MexFunction::write(ArgumentList inputs)
{
    // Locals
    plc4c_write_request *request;
    plc4c_write_request_execution *execution;
    plc4c_write_response *response;
    plc4c_data *data;
    size_t idx;

    // Parse the input arguments
    ASSERT(connected, "must be connected to write");
    StructArray writes = formatWriteArgs(inputs);

    // Setup the write request
    result = plc4c_connection_create_write_request(connection, &request);
    ASSERT(result == OK,"plc4c_connection_create_write_request failed");
    for (idx = 0; idx < writes.getNumberOfElements(); idx++) {
        data = encodeWriteData(writes, idx);
        ASSERT(data !=  nullptr, "encodeWriteData failed");
        CharArray addr = writes[idx]["address"];
        result = plc4c_write_request_add_item(request,
            (char*) addr.toAscii().c_str(), data);
        ASSERT(result == OK,"plc4c_write_request_add_item failed");
    }
    result = plc4c_write_request_execute(request, &execution);
    ASSERT(result == OK,"plc4c_write_request_execute failed");
    
    // Perform the write
    while(1) {
        result = plc4c_system_loop(system);
        ASSERT(result == OK, "plc4c_system_loop failed");
        if (plc4c_write_request_execution_check_completed_with_error(execution)) 
            ERROR("write execution failed");
        else if (plc4c_write_request_check_finished_successfully(execution)) 
            break;
    }
    response = plc4c_write_request_execution_get_response(execution);
    ASSERT(response != NULL, "plc4c_write_request_execution_get_response failed");

    // Clean up
    plc4c_write_destroy_write_response(response);
    plc4c_write_request_execution_destroy(execution);
    plc4c_write_request_destroy(request);

}

void MexFunction::read(ArgumentList inputs, ArgumentList outputs)
{
    // Locals
    plc4c_read_request *request;
    plc4c_read_request_execution *execution;
    plc4c_read_response *response;
    plc4c_list_element *responce_list;
    plc4c_response_value_item *responce_value;
    ArrayFactory factory;
    size_t idx;

    // Parse the input arguments
    ASSERT(connected, "must be connected to read");
    StructArray reads = formatReadArgs(inputs);

    // Setup the read request
    result = plc4c_connection_create_read_request(connection, &request);
    ASSERT(result == OK, "plc4c_connection_create_read_request failed");
    for (idx = 0 ; idx < reads.getNumberOfElements() ; idx++) {
        CharArray addr = reads[idx]["address"];
        CharArray name = reads[idx]["name"];
        result = plc4c_read_request_add_item(request, 
            (char*) name.toAscii().c_str(), (char*) addr.toAscii().c_str());
    }
    result = plc4c_read_request_execute(request, &execution);
    ASSERT(result == OK, "plc4c_read_request_execute failed");

    // Perform the read
    while (1) {
        result = plc4c_system_loop(system);
        ASSERT(result == OK,"plc4c_system_loop failed");
        if (plc4c_read_request_execution_check_finished_successfully(execution))
            break;
        else if (plc4c_read_request_execution_check_finished_with_error(execution))
            ERROR("read execution failed");
    }
    response = plc4c_read_request_execution_get_response(execution);
    ASSERT(response != NULL, "plc4c_read_request_execution_get_response failed");
    
    // Assign read results to outputs
    responce_list = plc4c_utils_list_tail(response->items);
    idx = 0;
    while (responce_list != NULL) {
        responce_value = (plc4c_response_value_item *) responce_list->value;
        responce_list = responce_list->next;
        reads[idx]["value"] = decodeReadData(responce_value->value);
        DISP("decoded");
        idx++;
    }
    outputs[0] = reads;

    // Clean up
    plc4c_read_destroy_read_response(response);
    plc4c_read_request_execution_destroy(execution);
    plc4c_read_request_destroy(request);

}

void MexFunction::operator()(ArgumentList outputs, ArgumentList inputs)
{
        std::string mexOperation = ((CharArray)inputs[0]).toAscii();
        
        if (mexOperation == "init")
            return;
        else if  (mexOperation == "status")
            status();
        else if (mexOperation == "release")
            release();
        else if (mexOperation == "connect")
            connect(inputs);
        else if (mexOperation == "disconnect")
            disconnect();
        else if (mexOperation == "read")
            read(inputs, outputs);
        else if (mexOperation == "write")
            write(inputs);
        else 
            ERROR("mex operation not recognised");  
}
