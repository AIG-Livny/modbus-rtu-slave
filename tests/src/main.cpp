#include "gtest/gtest.h"

#include "modbus_rtu_slave.h"

const uint8_t test_data[] = {0x55,0x66};
uint8_t test_registers[4];


enum mbrs_protocol_error read_register(uint16_t address, uint16_t number_of_registers, uint8_t** data, uint8_t* data_len){
    if ( address != 0x1234 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    };

    if ( number_of_registers != 5 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    }

    *data = (uint8_t*)test_data;
    *data_len = sizeof(test_data);
    return MBRS_PROTOCOL_OK;
};

enum mbrs_protocol_error write_register(uint16_t address, uint16_t number_of_registers, uint8_t* data, uint8_t data_len) {
    if ( address != 0x1234 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    };

    if ( number_of_registers != 2 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    }

    memcpy(&test_registers,data,data_len);

    return MBRS_PROTOCOL_OK;
};

enum mbrs_protocol_error diagnostic(uint16_t subfunction, uint16_t data, uint16_t* return_data){
    if ( subfunction != 1 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    };

    if ( data != 5 ) {
        return MBRS_PROTOCOL_ERROR_DATA_ADDRESS;
    };

    *return_data = 0x1234;

    return MBRS_PROTOCOL_OK;
}


const uint8_t read_holding_registers[] = {0x01,0x03,0x12,0x34,0x00,0x05,0xC1,0x7F};
const uint8_t read_holding_registers_error[] = {0x01,0x03,0x12,0x38,0x00,0x05,0x01,0x7C};
const uint8_t diagnostic_01_05[] = {0x01,0x08,0x00,0x01,0x00,0x05,0x71,0xC8};
const uint8_t diagnostic_stat[] = {0x01,0x08,0xAA,0x00,0x00,0x00,0xC1,0xD3};
const uint8_t diagnostic_echo[] = {0x01,0x08,0x00,0x00,0x12,0x34,0xED,0x7C};
const uint8_t write_registers[] = {0x01,0x10,0x12,0x34,0x00,0x02,0x04,0x45,0x67,0x78,0x9A,0x23,0x50};
const uint8_t broadcast_write_registers[] = {0x00,0x10,0x12,0x34,0x00,0x02,0x04,0x45,0x67,0x78,0x9A,0x27,0xAC};


TEST(MainTest, All) {
    uint8_t tx_buffer[50];

    struct mbrs_context_t mb = {
        .address = 1,
        .read_holding_register_cb = read_register,
    };

    // Read registers
    struct mbrs_operation_t op = {
        .context=&mb,
        .rx_buffer_pointer=(uint8_t*)read_holding_registers,
        .tx_buffer_pointer=tx_buffer,
        .tx_buffer_len = sizeof(tx_buffer),
        .rx_bytes = sizeof(read_holding_registers),
    };

    mbrs_internal_error ec;

    ec = mbrs_process(&op);

    // Ok answer with test data
    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    EXPECT_EQ(op.tx_bytes, 7);
    EXPECT_EQ(op.tx_buffer_pointer[2], sizeof(test_data));
    for ( uint8_t i=0; i < sizeof(test_data); i++){
        EXPECT_EQ(op.tx_buffer_pointer[3+i], test_data[i]);
    }

    // Read wrong address
    op.rx_buffer_pointer = (uint8_t*)read_holding_registers_error;
    op.rx_bytes = sizeof(read_holding_registers_error);

    ec = mbrs_process(&op);

    // Error answer
    EXPECT_EQ(ec,MBRS_INTERNAL_ERROR_ANSWERED_ERROR);
    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(op.tx_buffer_pointer[1], 0x83);

    // Write registers is not supports now
    op.rx_buffer_pointer = (uint8_t*)write_registers;
    op.rx_bytes = sizeof(write_registers);

    ec = mbrs_process(&op);

    // Error answer
    EXPECT_EQ(ec,MBRS_INTERNAL_ERROR_ANSWERED_ERROR);
    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(op.tx_buffer_pointer[1], 0x90);

    // Add write support
    mb.write_multiple_registers_cb = write_register;
    op.rx_bytes = sizeof(write_registers);
    ec = mbrs_process(&op);

    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    for ( uint8_t i=0; i < sizeof(test_registers); i++){
        EXPECT_EQ(op.rx_buffer_pointer[7+i], test_registers[i]);
    }

    // broadcast writing
    op.rx_buffer_pointer = (uint8_t*)broadcast_write_registers;
    op.rx_bytes = sizeof(broadcast_write_registers);
    memset(test_registers, 0, 4);
    EXPECT_EQ(test_registers[0],0);

    ec = mbrs_process(&op);
    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    for ( uint8_t i=0; i < sizeof(test_registers); i++){
        EXPECT_EQ(op.rx_buffer_pointer[7+i], test_registers[i]);
    }

    // Diagnostic echo function
    op.rx_buffer_pointer = (uint8_t*)diagnostic_echo;
    op.rx_bytes = sizeof(diagnostic_echo);
    tx_buffer[4] = 0;
    tx_buffer[5] = 0;

    ec = mbrs_process(&op);
    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    EXPECT_EQ(tx_buffer[4],diagnostic_echo[4]);
    EXPECT_EQ(tx_buffer[5],diagnostic_echo[5]);


    // statistics
    op.rx_buffer_pointer = (uint8_t*)diagnostic_stat;
    op.rx_bytes = sizeof(diagnostic_stat);

    ec = mbrs_process(&op);

    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    EXPECT_EQ(op.tx_buffer_pointer[5],5);

    // Other diagnostic subfunction calls error
    op.rx_buffer_pointer = (uint8_t*)diagnostic_01_05;
    op.rx_bytes = sizeof(diagnostic_01_05);

    ec = mbrs_process(&op);
    EXPECT_EQ(ec,MBRS_INTERNAL_ERROR_ANSWERED_ERROR);
    EXPECT_EQ(mbrs_crc16(op.tx_buffer_pointer,op.tx_bytes), 0);
    EXPECT_EQ(op.tx_buffer_pointer[1], 0x88);

    // Set diagnostic handler
    mb.diagnostic_cb = diagnostic;

    op.rx_bytes = sizeof(diagnostic_01_05);
    ec = mbrs_process(&op);

    EXPECT_EQ(ec,MBRS_INTERNAL_OK);
    EXPECT_EQ(op.tx_buffer_pointer[4], 0x12);
    EXPECT_EQ(op.tx_buffer_pointer[5], 0x34);

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}