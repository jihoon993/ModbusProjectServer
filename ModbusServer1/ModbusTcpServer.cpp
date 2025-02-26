#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "ModbusTCPServer.h"

modbus_mapping_t* mb_mapping = nullptr;

std::vector<Data> dataList;

int main() {

    if (!AcceptClientConnection()) {
        printf("서버 연결 실패.\n");
        return -1;
    }
    unsigned char recv_buff[37];

    printf("클라이언트와 통신을 시작합니다.\n");


    while (true)
    {
        memset(recv_buff, 0, sizeof(recv_buff));

        int bytes_read = ReceiveModbusRequest(client_sock, recv_buff);

        if (bytes_read <= 0)
        {
            printf("클라이언트 연결이 끊어졌거나 오류 발생.\n");
            break;
        }

        if (recv_buff[7] == 0x11)
        {
            printf("종료 명령 수신 (0x11)\n");
            break;
        }

        if (!start_data)
        {
            mb_mapping = ModbusMappingNew(COILS_NUM, 5);
            if (mb_mapping == NULL) {
                std::cerr << "mb mapping ERROR" << std::endl;
                break;
            }

            FirstValueSetting();
        }
        start_data = true;

        printf("======================================================\n");
        printf("Client->Server: ");
        for (int i = 0; i < sizeof(recv_buff); i++) {
            printf("%02X ", recv_buff[i]);
        }
        printf("\n");

        unsigned char flag = recv_buff[7];
        ProcessModbusRequest(client_sock, recv_buff,
            reg_register_map, reg_bit_map, flag);
    }

    ExitServer();
    return 0;
}

#pragma region 서버 연결 및 종료 

void ExitServer()
{
    closesocket(client_sock);
    closesocket(sock);
    WSACleanup();
}


bool AcceptClientConnection()
{
    memset(reg_register_map, 0x00, sizeof(short) * 150);
    memset(reg_bit_map, 0x00, sizeof(char) * 150);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    memset(&sock_addr, 0, sizeof(sock_addr));

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    if (listen(sock, 1) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    printf("Modbus TCP Simulator for Server is working!\n");
    printf("Waiting Client Connection!\n");


    client_sock = AcceptClientConnection(sock, &client_addr);

    if (client_sock == INVALID_SOCKET) {
        printf("클라이언트 연결 오류\n");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    return true;
}


int AcceptClientConnection(SOCKET sock, struct sockaddr_in* client_addr) {
    int client_addr_size = sizeof(*client_addr);
    SOCKET client_sock = accept(sock, (struct sockaddr*)client_addr, &client_addr_size);

    if (client_sock != INVALID_SOCKET) {
        printf("클라이언트 연결 : %s:%d\n",
            inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    }
    else {
        printf("클라이언트 연결 실패\n");
    }

    return client_sock;
}
#pragma endregion

int ReceiveModbusRequest(SOCKET client_sock, unsigned char* recv_buff) {
    int header_len = 8;

    int bytes_read = recv(client_sock, (char*)recv_buff, header_len, 0);
    if (bytes_read == SOCKET_ERROR || bytes_read < header_len) {
        printf("Error receiving data (Header).\n");
        return -1;
    }

    if (recv_buff[7] == 0x10) {
        create_receive_len = 31;
    }
    else
    {
        create_receive_len = 13;
    }

    int remaining_len = create_receive_len - header_len;
    if (remaining_len > 0) {
        int additional_bytes = recv(client_sock, (char*)(recv_buff + header_len), remaining_len, 0);
        if (additional_bytes == SOCKET_ERROR) {
            printf("Error receiving data (Body).\n");
            return -1;
        }
        bytes_read += additional_bytes;
        printf("check:" + bytes_read);
    }

    return bytes_read;
}


void FirstValueSetting()
{
    if (mb_mapping == nullptr) {
        std::cerr << "mb_mapping이 초기화되지 않았습니다!" << std::endl;
        return;
    }

    // Coils
    mb_mapping->tab_bits[0] = stop;
    mb_mapping->tab_bits[1] = valve_dry_pump_on;
    mb_mapping->tab_bits[2] = valve_N2_on;
    mb_mapping->tab_bits[3] = valve_O3_on;
    mb_mapping->tab_bits[4] = valve_ZrO2_on;
    mb_mapping->tab_bits[5] = valve_HfO2_on;
    mb_mapping->tab_bits[6] = valve_H2O2_on;
    mb_mapping->tab_bits[7] = valve_TMA_on;


    // Register
    mb_mapping->tab_registers[0] = (unsigned short)cham_temp_first_value;
    mb_mapping->tab_registers[1] = (unsigned short)cham_press_first_value;
    mb_mapping->tab_registers[2] = (unsigned short)cham_flow_first_value;
    mb_mapping->tab_registers[3] = (unsigned short)gas_O3_first_value;
    mb_mapping->tab_registers[4] = (unsigned short)gas_N2_first_value;
    mb_mapping->tab_registers[5] = (unsigned short)gas_ZrO2_first_value;
    mb_mapping->tab_registers[6] = (unsigned short)gas_HfO2_first_value;
    mb_mapping->tab_registers[7] = (unsigned short)gas_H2O2_first_value;
    mb_mapping->tab_registers[8] = (unsigned short)gas_TMA_first_value;
    mb_mapping->tab_registers[9] = (unsigned short)wafer_size_first_value;
    mb_mapping->tab_registers[10] = (unsigned short)wafer_loading_first_value;
    mb_mapping->tab_registers[11] = (unsigned short)wafer_flat_area_first_value;
    mb_mapping->tab_registers[12] = (unsigned short)wafer_amount_first_value;
    mb_mapping->tab_registers[13] = (unsigned short)rf_power_first_value;
    mb_mapping->tab_registers[14] = (unsigned short)plasma_forward_first_value;
    mb_mapping->tab_registers[15] = (unsigned short)plasma_reflected_first_value;

}


#pragma region DataList 관련
void DataSave(unsigned char* recv_buff)
{
    unsigned short check = (recv_buff[0] << 8) | recv_buff[1];

    for (auto& data : dataList)
    {
        if (data.transaction_id == check)
        {
            data.transaction_id = (recv_buff[0] << 8) | recv_buff[1];
            data.protocol_id = (recv_buff[2] << 8) | recv_buff[3];
            data.length = (recv_buff[4] << 8) | recv_buff[5];
            data.device_id = recv_buff[6];
            data.function_code = recv_buff[7];
            data.start_address = (recv_buff[8] << 8) | recv_buff[9];
            data.register_num = (recv_buff[10] << 8) | recv_buff[11];

            return;
        }
    }

    Data new_data((recv_buff[0] << 8) | recv_buff[1],
        (recv_buff[2] << 8) | recv_buff[3],
        (recv_buff[4] << 8) | recv_buff[5],
        recv_buff[6], recv_buff[7],
        (recv_buff[8] << 8) | recv_buff[9],
        (recv_buff[10] << 8) | recv_buff[11]);
    dataList.push_back(new_data);
}


Data* DataFound(unsigned char* recv_buff)
{
    Data* found_data = nullptr;
    unsigned short check = (recv_buff[0] << 8) | recv_buff[1];

    for (auto& data : dataList)
    {
        if (data.transaction_id == check)
        {
            return &data;
        }
    }

    printf("해당 Transaction ID는 존재하지 않습니다.\n");
    return nullptr;

}

void DataPrint()
{
    for (const auto& data : dataList) {
        printf("*******************************************************\n");
        printf("[ Transaction ID ] : %u\n", data.transaction_id);
        printf("-------------------------------------------------------\n");
        printf("[  Function Code ] : %u\n", data.function_code);
        printf("-------------------------------------------------------\n");
        printf("*******************************************************\n");
    }
}
#pragma endregion

void ProcessModbusRequest(SOCKET client_sock, unsigned char* recv_buff,
    unsigned short* reg_r_map, unsigned char* reg_b_map, unsigned char flag)
{
    // 클라이언트 데이터 저장
    DataSave(recv_buff);

    bool coil_state = false;
    uint16_t register_value = 0;

    uint16_t start_address = (recv_buff[8] << 8) | recv_buff[9];
    uint16_t transaction_id = (recv_buff[0] << 8) | recv_buff[1];

    uint16_t coil_value = (recv_buff[10] << 8) | recv_buff[11];

    switch (flag)
    {

    case 0x01:
        // Read Coils
        reg_b_map = mb_mapping->tab_bits;
        ModbusReplyBit(client_sock, recv_buff, reg_b_map, flag);
        break;

    case 0x03:
        // Read Holding Register
        reg_r_map = mb_mapping->tab_registers;
        ModbusReplyRegister(client_sock, recv_buff, reg_r_map, flag);
        break;

    case 0x05:
        if (coil_value == 0x0000) { coil_state = false; }
        else if (coil_value == 0xFF00) { coil_state = true; }
        else { printf("잘못된 값 도착"); }

        mb_mapping->tab_bits[start_address] = coil_state;

        break;
    case 0x10:
        if (transaction_id == 0x04)
        {
            mb_mapping->tab_registers[9] = (recv_buff[13] << 8) | recv_buff[14];     // wafer_size
            mb_mapping->tab_registers[10] = (recv_buff[15] << 8) | recv_buff[16];    // wafer_loading
            mb_mapping->tab_registers[11] = (recv_buff[17] << 8) | recv_buff[18];    // wafer_flat_area
            mb_mapping->tab_registers[12] = (recv_buff[19] << 8) | recv_buff[20];    // wafer_amount
        }
        else if (transaction_id == 0x01)
        {
            mb_mapping->tab_registers[0] = (recv_buff[13] << 8) | recv_buff[14];     // cham_temp
            mb_mapping->tab_registers[1] = (recv_buff[15] << 8) | recv_buff[16];     // cham_press
            mb_mapping->tab_registers[2] = (recv_buff[17] << 8) | recv_buff[18];     // cham_flow
            mb_mapping->tab_registers[3] = (recv_buff[19] << 8) | recv_buff[20];     // gas_O3
            mb_mapping->tab_registers[4] = (recv_buff[21] << 8) | recv_buff[22];     // gas_N2
            mb_mapping->tab_registers[5] = (recv_buff[23] << 8) | recv_buff[24];     // gas_ZrO2
            mb_mapping->tab_registers[6] = (recv_buff[25] << 8) | recv_buff[26];     // gas_HfO2
            mb_mapping->tab_registers[7] = (recv_buff[27] << 8) | recv_buff[28];     // gas_H2O2
            mb_mapping->tab_registers[8] = (recv_buff[29] << 8) | recv_buff[30];     // gas_TMA
        }
        else if (transaction_id == 0x0B)
        {
            mb_mapping->tab_registers[13] = (recv_buff[13] << 8) | recv_buff[14];    // rf_power
            mb_mapping->tab_registers[14] = (recv_buff[15] << 8) | recv_buff[16];    // plasma_forward
            mb_mapping->tab_registers[15] = (recv_buff[17] << 8) | recv_buff[18];    // plasma_reflected
        }
        else
        {
            printf("transaction_id와 function_code가 맞지 않습니다.");
        }
        break;
    default:
        printf("Unknown flag: 0x%02X\n", flag);
        break;
    }


}

// 0x01 처리(리드 코일)
void ModbusReplyBit(SOCKET client_sock, unsigned char* recv_buff,
    unsigned char* reg_b_map, unsigned char flag)
{
    memcpy(&address, &recv_buff[2], 2);
    address = ReverseByteOrder(address);

    printf("Read Coil\n");
    unsigned char len = 4;
    unsigned char coils = (len + 7) / 8;

    unsigned short buff_size = 9 + coils;
    send_buff = (unsigned char*)malloc(buff_size);

    Data* found_data = DataFound(recv_buff);
    if (!found_data) {
        return;
    }

    send_buff[0] = (found_data->transaction_id >> 8) & 0xFF;
    send_buff[1] = found_data->transaction_id & 0xFF;

    send_buff[2] = (found_data->protocol_id >> 8) & 0xFF;
    send_buff[3] = found_data->protocol_id & 0xFF;

    send_buff[4] = (buff_size - 6 >> 8) & 0xFF;
    send_buff[5] = buff_size - 6 & 0xFF;

    send_buff[6] = found_data->device_id;
    send_buff[7] = found_data->function_code;

    send_buff[8] = coils & 0xFF;

    send_buff[9] = 0x00;

    for (unsigned char i = 0; i < len; i++) {

        if (reg_b_map[address + i]) {
            send_buff[9] |= 1 << i;
        }
    }

    printf("Server->Client: ");
    for (int i = 0; i < 9 + len; i++) {
        printf("%02X ", send_buff[i]);
    }
    printf("\n");

    send(client_sock, (char*)send_buff, buff_size, 0);

    free(send_buff);

}

// 0x03 레지스터 
void ModbusReplyRegister(SOCKET client_sock, unsigned char* recv_buff,
    unsigned short* reg_r_map, unsigned char flag)
{
    memcpy(&address, &recv_buff[2], 2);
    address = ReverseByteOrder(address);

    printf("Read Register\n");
    unsigned short len = 16;

    send_buff = (unsigned char*)malloc(12 + len * 2);
    Data* found_data = DataFound(recv_buff);
    if (!found_data) {
        return;
    }

    send_buff[0] = (found_data->transaction_id >> 8) & 0xFF;
    send_buff[1] = found_data->transaction_id & 0xFF;

    send_buff[2] = (found_data->protocol_id >> 8) & 0xFF;
    send_buff[3] = found_data->protocol_id & 0xFF;

    send_buff[4] = 0x00;
    send_buff[5] = 0x10;

    send_buff[6] = found_data->device_id;
    send_buff[7] = found_data->function_code;

    send_buff[8] = (found_data->start_address >> 8) & 0xFF;
    send_buff[9] = found_data->start_address & 0xFF;

    send_buff[10] = (found_data->register_num >> 8) & 0xFF;
    send_buff[11] = found_data->register_num & 0xFF;


    // Read 0x03번 : Register
    for (unsigned short i = 0; i < len; i++) {
        register_value = reg_r_map[address + i]; // reg_r_map[0] == tab_registers[0]
        register_value = ReverseByteOrder(register_value);
        memcpy(&send_buff[12 + i * 2], &register_value, 2);
    }

    printf("Server->Client: ");
    for (int i = 0; i < 12 + len * 2; i++) {
        printf("%02X ", send_buff[i]);
    }
    printf("\n");

    send(client_sock, (char*)send_buff, 12 + len * 2, 0);

    free(send_buff);

}

// --------------------------------------------------
// 바이트 오더 뒤집기
// --------------------------------------------------
unsigned short ReverseByteOrder(unsigned short value) {
    unsigned short ret = 0;
    ((char*)&ret)[0] = ((char*)&value)[1];
    ((char*)&ret)[1] = ((char*)&value)[0];
    return ret;

}


#pragma region libModbus 참고하여 만든 mapping을 이용한 기능별 데이터 저장
// --------------------------------------------------
// 기능별 데이터를 저장할 새로운 매핑 생성
// --------------------------------------------------
modbus_mapping_t* ModbusMappingNew(int nb_bits,
    int nb_registers)
{
    return ModbusMappingNewStartAddress(
        0, nb_bits, 0, nb_registers);
}


// --------------------------------------------------
// nb_bit     : 저장할 비트의 개수
// start_bits : 비트의 시작 주소
// --------------------------------------------------
modbus_mapping_t* ModbusMappingNewStartAddress(
    unsigned int start_bits, unsigned int nb_bits,
    unsigned int start_registers, unsigned int nb_registers)
{
    modbus_mapping_t* mb_mapping;

    mb_mapping = (modbus_mapping_t*)malloc(sizeof(modbus_mapping_t));
    if (mb_mapping == NULL) {
        return NULL;
    }

    /* 1X */
    mb_mapping->nb_bits = nb_bits;
    mb_mapping->start_bits = start_bits;
    if (nb_bits == 0) {
        mb_mapping->tab_bits = NULL;
    }
    else {
        /* Negative number raises a POSIX error */
        mb_mapping->tab_bits =
            (uint8_t*)malloc(nb_bits * sizeof(uint8_t));
        if (mb_mapping->tab_bits == NULL) {
            free(mb_mapping);
            return NULL;
        }
        memset(mb_mapping->tab_bits, 0, nb_bits * sizeof(uint8_t));
    }

    /* 4X */
    mb_mapping->nb_registers = nb_registers;
    mb_mapping->start_registers = start_registers;
    if (nb_registers == 0) {
        mb_mapping->tab_registers = NULL;
    }
    else {
        mb_mapping->tab_registers =
            (uint16_t*)malloc(nb_registers * sizeof(uint16_t));
        if (mb_mapping->tab_registers == NULL) {
            free(mb_mapping->tab_bits);
            free(mb_mapping);
            return NULL;
        }
        memset(mb_mapping->tab_registers, 0, nb_registers * sizeof(uint16_t));
    }

    return mb_mapping;
}

#pragma endregion