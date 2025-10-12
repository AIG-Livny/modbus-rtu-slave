#ifndef _MBRS_H
#define _MBRS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <inttypes.h>
#include <iso646.h>
#include <stdbool.h>

/// Library configurations. 1 - enabled, 0 - disabled

#ifndef MBRS_CRC_TABLE_CALCULATION
    // Calculate CRC16 by precalculated tables. Increases flash memory consumption, but faster runtime code
    #define MBRS_CRC_TABLE_CALCULATION 1
#endif

#ifndef MBRS_STATISTICS_ENABLED
    // Support statistics: num of sended / recieved, errors etc.
    #define MBRS_STATISTICS_ENABLED 1
#endif

#ifndef MBRS_STATISTICS_DIAGNOSTIC_START_ADDRESS
    // To get statistics, you need to refer to the diagnostic function and the subfunction code that starts with this address
    #define MBRS_STATISTICS_DIAGNOSTIC_START_ADDRESS 0xAA00
#endif

#define MBRS_STAT_ANY_RECIEVED 0
#define MBRS_STAT_MY_PACKETS_RECIEVED 1
#define MBRS_STAT_OK_SENDED 2
#define MBRS_STAT_ERROR_SENDED 3
#define MBRS_STAT_INVALID_PACKETS_RECIEVED 4


/// Library configurations END

/// Internal Errors

enum mbrs_internal_error {
    MBRS_INTERNAL_OK=0,
    MBRS_INTERNAL_ERROR_ADDRESS_NOT_MATCH=1,
    MBRS_INTERNAL_ERROR_CRC=2,
    MBRS_INTERNAL_ERROR_INVALID_PACKET=3,
    MBRS_INTERNAL_ERROR_ANSWERED_ERROR=4,
    MBRS_INTERNAL_ERROR_MESSAGE_ENDED=5,
    MBRS_INTERNAL_ERROR_BROADCAST_ONLY_FOR_MULTIPLE_REGISTERS=6,

    // Error codes above this level is critical
    MBRS_INTERNAL_CRITICAL_LEVEL_ERRORS=100,

    MBRS_INTERNAL_ERROR_STRUCTURE_POINTER_IS_NULL=101,
    MBRS_INTERNAL_ERROR_TX_BUFFER_IS_OVER=102,
};

/// Internal Errors END

/// MODBUS Protocol errors

enum mbrs_protocol_error {
    MBRS_PROTOCOL_OK=0,

    // The function code received in the query is not an allowable action.
    MBRS_PROTOCOL_ERROR_ILLEGAL_FUNCTION=0x01,

    // The data address received in the query is not an allowable address (i.e. the combination of register and transfer length is invalid).
    MBRS_PROTOCOL_ERROR_DATA_ADDRESS=0x02,

    // A value contained in the query data field is not an allowable value.
    MBRS_PROTOCOL_ERROR_DATA_VALUE=0x03,

    // Device internal error
    MBRS_PROTOCOL_ERROR_DEVICE_FAILURE=0x04,

    // Device not ready to answer, repeat later
    MBRS_PROTOCOL_ERROR_ACKNOWLEDGE=0x05,

    // Device busy
    MBRS_PROTOCOL_ERROR_BUSY=0x06,

    // Request rejected
    MBRS_PROTOCOL_ERROR_NEGATIVE_ACKNOWLEDGE=0x07,
};

/// MODBUS Protocol errors END

// Read function callback type. data* - place answer there, data_len* - place length there
typedef enum mbrs_protocol_error (mbrs_read_cb_t)(uint16_t address, uint16_t number_of_registers, uint8_t** data, uint16_t* data_len);

// Write function callback type. data* - input data buffer
typedef enum mbrs_protocol_error (mbrs_write_cb_t)(uint16_t address, uint16_t number_of_registers, uint8_t* data, uint16_t data_len);

// Diagnostic function callback type
typedef enum mbrs_protocol_error (mbrs_diagnostic_cb_t)(uint16_t subfunction, uint16_t data, uint16_t* return_data);

struct mbrs_context_t;

struct mbrs_operation_t {
    struct mbrs_context_t* context;
    uint8_t* rx_buffer_pointer;
    uint8_t* tx_buffer_pointer;
    uint16_t rx_buffer_len;
    uint16_t tx_buffer_len;
    uint16_t rx_bytes;
    uint16_t tx_bytes;
    uint16_t crc;
};

struct mbrs_context_t {
    uint8_t address;

    mbrs_read_cb_t* read_coil_status_cb;
    mbrs_read_cb_t* read_input_status_cb;
    mbrs_read_cb_t* read_holding_register_cb;

    mbrs_write_cb_t* write_multiple_coils_cb;
    mbrs_write_cb_t* write_single_register_cb;
    mbrs_write_cb_t* write_multiple_registers_cb;

    mbrs_diagnostic_cb_t* diagnostic_cb;

    #if MBRS_STATISTICS_ENABLED == 1

    struct stat_t {
        uint16_t any_recieved;
        uint16_t my_packets_recieved;
        uint16_t ok_sended;
        uint16_t error_sended;
        uint16_t invalid_packets_recieved;
    }stat;

    #endif
};

// Run modbus process
enum mbrs_internal_error mbrs_process ( struct mbrs_operation_t* op );

// Input byte from USART
void mbrs_input_byte ( struct mbrs_operation_t* op, uint8_t data, enum mbrs_internal_error* where_put_ret_code );

// Input byte to USART
uint8_t mbrs_output_byte ( struct mbrs_operation_t* op, enum mbrs_internal_error* where_put_ret_code );

// Calculate crc16
uint16_t mbrs_crc16( const uint8_t* buf, uint16_t len );

#ifndef __cplusplus
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif