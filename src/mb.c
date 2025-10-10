#include "modbus_rtu_slave.h"
#include "mb.h"

#include <string.h>

#define MINIMAL_PACKET_LENGTH 7

// BN - byte number
#define BN_REGISTER_ADDRESS 2
#define BN_NUMBER_OF_REGISTERS 4
#define BN_FUNCTION_CODE 1
#define BN_ADDRESS 0
#define BN_REQUEST_NUMBER_OF_BYTES 6

#define BN_WRITE_REQUEST_DATA 7

#define BN_READ_ANSWER_NUMBER_OF_DATA_BYTES 2
#define BN_READ_ANSWER_DATA 3
#define BN_ERROR_CODE 2

#define BN_DIAG_SUBFUNCTION 2
#define BN_DIAG_DATA 4

#define GET_VAL_BUF(buf,byte_num) (__builtin_bswap16(*(uint16_t*)&(buf[byte_num])))
#define SET_VAL_BUF(buf,byte_num,value) (*(uint16_t*)&(buf[byte_num]) = __builtin_bswap16(value))

#define MINIMAL_BUFFER_SIZE 16
#define READ_ANSWER_LEN_WITHOUT_DATA 3
#define WRITE_ANSWER_LEN 5
#define DIAG_ANSWER_LEN 6
#define ERROR_ANSWER_LEN 3

uint16_t mbrs_crc16_add ( uint8_t data, uint16_t crc ) {
    #if MBRS_CRC_TABLE_CALCULATION == 1

    uint8_t* pcrc = (uint8_t*)&crc;

    uint8_t s = pcrc[0] ^ data;
    pcrc[0] = pcrc[1];
    pcrc[1] = table_crc_lo[s];
    pcrc[0] ^= table_crc_hi[s];

    #else

    uint8_t flag;
	crc ^= data;
	for ( uint8_t i = 0; i < 8; i++ ) {
		flag = (uint8_t)(crc & 0x0001);
		crc >>= 1;
		if (flag){
			crc ^= 0xA001;
		}
	}

    #endif

    return crc;
}

uint16_t mbrs_crc16 ( const uint8_t* buf, uint16_t len ) {
	uint16_t crc = 0xFFFF;
    for ( uint8_t byte_num = 0; byte_num < len; byte_num++ ) {
        crc = mbrs_crc16_add(buf[byte_num], crc);
    }
    return crc;
}

void fill_error ( struct mbrs_operation_t* op, enum mbrs_protocol_error ec ) {
    op->tx_buffer_pointer[BN_FUNCTION_CODE] |= 0x80;
    op->tx_buffer_pointer[BN_ERROR_CODE] = ec;
    op->tx_bytes = ERROR_ANSWER_LEN;

    #if MBRS_STATISTICS_ENABLED == 1
    op->context->stat.error_sended += 1;
    #endif
}

enum mbrs_internal_error read( struct mbrs_operation_t* op, mbrs_read_cb_t* read_callback ) {
    if ( read_callback ) {
        uint16_t register_address = GET_VAL_BUF(op->rx_buffer_pointer,BN_REGISTER_ADDRESS);
        uint16_t number_of_registers = GET_VAL_BUF(op->rx_buffer_pointer,BN_NUMBER_OF_REGISTERS);

        uint8_t* data = NULL;
        uint8_t data_len = 0;

        enum mbrs_protocol_error error = read_callback(register_address, number_of_registers, &data, &data_len);

        if ( error ) {
            fill_error(op, error);
            return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
        } else {
            if ( not data ) {
                fill_error(op, MBRS_PROTOCOL_ERROR_DEVICE_FAILURE);
                return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
            }

            if ( READ_ANSWER_LEN_WITHOUT_DATA + data_len > op->tx_buffer_len ) {
                fill_error(op, MBRS_PROTOCOL_ERROR_DATA_ADDRESS);
                return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
            }

            op->tx_buffer_pointer[BN_READ_ANSWER_NUMBER_OF_DATA_BYTES] = data_len;
            memcpy(&op->tx_buffer_pointer[BN_READ_ANSWER_DATA], data, data_len);

            op->tx_bytes = READ_ANSWER_LEN_WITHOUT_DATA + data_len;
        }


    } else {
        fill_error(op, MBRS_PROTOCOL_ERROR_ILLEGAL_FUNCTION);

        #if MBRS_STATISTICS_ENABLED == 1
        op->context->stat.invalid_packets_recieved += 1;
        #endif

        return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
    }

    return MBRS_INTERNAL_OK;
}

enum mbrs_internal_error write ( struct mbrs_operation_t* op, mbrs_write_cb_t* write_callback ) {
    if ( write_callback ) {
        uint16_t register_address = GET_VAL_BUF(op->rx_buffer_pointer,BN_REGISTER_ADDRESS);
        uint16_t number_of_registers = GET_VAL_BUF(op->rx_buffer_pointer,BN_NUMBER_OF_REGISTERS);

        enum mbrs_protocol_error error = write_callback(
            register_address,
            number_of_registers,
            &op->rx_buffer_pointer[BN_WRITE_REQUEST_DATA],
            op->rx_buffer_pointer[BN_REQUEST_NUMBER_OF_BYTES]);

        if ( error ) {
            fill_error(op, error);
            return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
        } else {
            //*(uint16_t*)&REGISTER_ADDRESS(mb->tx_buffer_pointer) = *register_address;
            //*(uint16_t*)&NUMBER_OF_REGISTERS(mb->tx_buffer_pointer) = *number_of_registers;
            memcpy(&op->tx_buffer_pointer[BN_REGISTER_ADDRESS], &op->rx_buffer_pointer[BN_REGISTER_ADDRESS], 4);
            op->tx_bytes = WRITE_ANSWER_LEN;
        }


    } else {
        fill_error(op, MBRS_PROTOCOL_ERROR_ILLEGAL_FUNCTION);

        #if MBRS_STATISTICS_ENABLED == 1
        op->context->stat.invalid_packets_recieved += 1;
        #endif

        return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
    }

    return MBRS_INTERNAL_OK;
}

enum mbrs_internal_error diagnostic( struct mbrs_operation_t* op ) {
    uint16_t subfunction = GET_VAL_BUF(op->rx_buffer_pointer,BN_DIAG_SUBFUNCTION);
    uint16_t data = GET_VAL_BUF(op->rx_buffer_pointer,BN_DIAG_DATA);
    uint16_t return_data = 0;

    // Echo
    if ( subfunction == 0 ) {
        memcpy(op->tx_buffer_pointer, op->rx_buffer_pointer, DIAG_ANSWER_LEN);
    } else

    #if MBRS_STATISTICS_ENABLED == 1

        if ((subfunction >= MBRS_STATISTICS_DIAGNOSTIC_START_ADDRESS) and (subfunction < MBRS_STATISTICS_DIAGNOSTIC_START_ADDRESS + 5) ) {
            switch ( subfunction - MBRS_STATISTICS_DIAGNOSTIC_START_ADDRESS ) {
                case MBRS_STAT_ANY_RECIEVED:                return_data = op->context->stat.any_recieved; break;
                case MBRS_STAT_ERROR_SENDED:                return_data = op->context->stat.error_sended; break;
                case MBRS_STAT_INVALID_PACKETS_RECIEVED:    return_data = op->context->stat.invalid_packets_recieved; break;
                case MBRS_STAT_MY_PACKETS_RECIEVED:         return_data = op->context->stat.my_packets_recieved; break;
                case MBRS_STAT_OK_SENDED:                   return_data = op->context->stat.ok_sended; break;
            }

            SET_VAL_BUF(op->tx_buffer_pointer, BN_REGISTER_ADDRESS, subfunction);
            SET_VAL_BUF(op->tx_buffer_pointer, BN_DIAG_DATA, return_data);

        } else

    #endif


    if ( op->context->diagnostic_cb ) {

        enum mbrs_protocol_error error = MBRS_PROTOCOL_OK;

        error = op->context->diagnostic_cb(subfunction, data, &return_data);

        if ( error ) {
            fill_error(op, error);
            return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
        } else {
            SET_VAL_BUF(op->tx_buffer_pointer, BN_REGISTER_ADDRESS, subfunction);
            SET_VAL_BUF(op->tx_buffer_pointer, BN_DIAG_DATA, return_data);
        }

    } else {
        fill_error(op, MBRS_PROTOCOL_ERROR_ILLEGAL_FUNCTION);

        #if MBRS_STATISTICS_ENABLED == 1
        op->context->stat.invalid_packets_recieved += 1;
        #endif

        return MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
    }

    op->tx_bytes = DIAG_ANSWER_LEN;
    return MBRS_INTERNAL_OK;
}

enum mbrs_internal_error mbrs_process ( struct mbrs_operation_t* op ) {
    if ( not op ) {
        return MBRS_INTERNAL_ERROR_STRUCTURE_POINTER_IS_NULL;
    }

    if ( op->rx_bytes < MINIMAL_PACKET_LENGTH ) {
        #if MBRS_STATISTICS_ENABLED == 1
        op->context->stat.invalid_packets_recieved += 1;
        #endif

        op->rx_bytes = 0;
        return MBRS_INTERNAL_ERROR_INVALID_PACKET;
    }

    op->rx_bytes = 0;

    #if MBRS_STATISTICS_ENABLED == 1
    op->context->stat.any_recieved += 1;
    #endif

    if ( op->crc != 0 ) {
        #if MBRS_STATISTICS_ENABLED == 1
        op->context->stat.invalid_packets_recieved += 1;
        #endif

        return MBRS_INTERNAL_ERROR_CRC;
    }

    bool broadcast = false;
    enum function_code fc = op->rx_buffer_pointer[BN_FUNCTION_CODE];

    if ( op->rx_buffer_pointer[BN_ADDRESS] != op->context->address ) {
        if ( op->rx_buffer_pointer[BN_ADDRESS] == 0 ) {
            if ( fc != CMD_WRITE_MULTIPLE_REGISTERS ) {
                return MBRS_INTERNAL_ERROR_BROADCAST_ONLY_FOR_MULTIPLE_REGISTERS;
            }
            broadcast = true;
        } else {
            return MBRS_INTERNAL_ERROR_ADDRESS_NOT_MATCH;
        }
    }

    #if MBRS_STATISTICS_ENABLED == 1
    op->context->stat.my_packets_recieved += 1;
    #endif

    op->tx_buffer_pointer[BN_ADDRESS] = op->context->address;
    op->tx_buffer_pointer[BN_FUNCTION_CODE] = op->rx_buffer_pointer[BN_FUNCTION_CODE];

    enum mbrs_internal_error error;

    switch ( fc ) {
        case CMD_READ_HOLDING_REGISTERS:    error = read(op, op->context->read_holding_register_cb); break;
        case CMD_READ_INPUT_STATUS:         error = read(op, op->context->read_input_status_cb); break;
        case CMD_READ_COIL_STATUS:          error = read(op, op->context->read_coil_status_cb); break;

        case CMD_WRITE_MULTIPLE_COILS:      error = write(op, op->context->write_multiple_coils_cb); break;
        case CMD_WRITE_MULTIPLE_REGISTERS:  error = write(op, op->context->write_multiple_registers_cb); break;
        case CMD_WRITE_SINGLE_REGISTER:     error = write(op, op->context->write_single_register_cb); break;

        case CMD_DIAGNOSTIC:                error = diagnostic(op); break;
        default:
            fill_error(op, MBRS_PROTOCOL_ERROR_ILLEGAL_FUNCTION);
            error = MBRS_INTERNAL_ERROR_ANSWERED_ERROR;
            break;
    }

    if ( not broadcast ) {

        #if MBRS_STATISTICS_ENABLED == 1
        if ( error == MBRS_INTERNAL_OK ) {
            op->context->stat.ok_sended += 1;
        }
        #endif

        uint16_t crc = mbrs_crc16( op->tx_buffer_pointer, op->tx_bytes );
        op->tx_buffer_pointer[op->tx_bytes] = crc;
        op->tx_buffer_pointer[op->tx_bytes + 1] = crc >> 8;

        op->tx_bytes += 2;

    } else {
        op->tx_bytes = 0;
    }

    return error;
}

void mbrs_input_byte ( struct mbrs_operation_t* op, uint8_t data, enum mbrs_internal_error* where_put_ret_code ) {
    if ( op->rx_bytes == 0 ) {
        op->crc = 0xFFFF;
    }

    op->rx_buffer_pointer[op->rx_bytes] = data;
    op->rx_bytes += 1;

    if ( op->rx_bytes >= op->rx_buffer_len ) {
        op->rx_bytes = 0;
        if ( where_put_ret_code ) {
            *where_put_ret_code = MBRS_INTERNAL_ERROR_TX_BUFFER_IS_OVER;
        }
    }

    op->crc = mbrs_crc16_add( data, op->crc );

    if ( where_put_ret_code ) {
        *where_put_ret_code = MBRS_INTERNAL_OK;
    }
}

uint8_t mbrs_output_byte ( struct mbrs_operation_t* op, enum mbrs_internal_error* where_put_ret_code ) {
    if ( op->tx_bytes == 0 ) {
        if ( where_put_ret_code ) {
            *where_put_ret_code = MBRS_INTERNAL_ERROR_MESSAGE_ENDED;
        }
        return 0;
    }

    uint8_t result = op->tx_buffer_pointer[op->tx_bytes];
    op->tx_bytes -= 1;
    if ( where_put_ret_code ) {
        *where_put_ret_code = MBRS_INTERNAL_OK;
    }

    return result;
}

