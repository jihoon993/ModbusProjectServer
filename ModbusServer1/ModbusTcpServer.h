#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <cstring>


#pragma comment(lib, "WS2_32.LIB")

#define SERVER_PORT 502
#define REQUEST_FLAG 1

WSADATA wsa;
SOCKET sock, client_sock;
struct sockaddr_in sock_addr, client_addr;

unsigned char* send_buff;
unsigned short address;
unsigned short register_value;
unsigned char bit_value;
unsigned short len;


int create_receive_len = 13;

bool AcceptClientConnection();
void ExitServer();
int AcceptClientConnection(SOCKET sock, struct sockaddr_in* client_addr);
unsigned short ReverseByteOrder(unsigned short value);
int ReceiveModbusRequest(SOCKET client_sock, unsigned char* recv_buff);
void ProcessModbusRequest(SOCKET client_sock, unsigned char* recv_buff, unsigned short* reg_r_map, unsigned char* reg_b_map, unsigned char flag);
void ModbusReplyRegister(SOCKET client_sock, unsigned char* recv_buff, unsigned short* reg_r_map, unsigned char flag);
void ModbusReplyBit(SOCKET client_sock, unsigned char* recv_buff, unsigned char* reg_b_map, unsigned char flag);
void DataSave(unsigned char* recv_buff);
void FirstValueSetting();


// 클라이언트에서 날아온 정보를 저장할 클래스
class Data {
public:
    unsigned short transaction_id;
    unsigned short protocol_id;
    unsigned short length;
    unsigned char device_id;
    unsigned char function_code;
    unsigned short start_address;
    unsigned short register_num;

    // 생성자
    Data() : transaction_id(0), protocol_id(0), length(0), device_id(0), function_code(0),
        start_address(0), register_num(0) {
    }

    Data(unsigned short tId, unsigned short pId, unsigned short len, unsigned char dId,
        unsigned char fCode, unsigned short sAddr, unsigned short qor)
        : transaction_id(tId), protocol_id(pId), length(len), device_id(dId),
        function_code(fCode), start_address(sAddr), register_num(qor) {
    }
};



// 기능별 데이터 저장 관련 부분
// Device ID의 기준이 빠졌으므로 각 기능별 startAddress 의 갯수로 설정
unsigned short reg_register_map[150];
unsigned char reg_bit_map[150];


//extern std::vector<bool> coils;
//extern std::vector<int> holdingRegisters;


typedef struct _modbus_mapping_t {
    int nb_bits;
    int start_bits;
    int nb_registers;
    int start_registers;
    uint8_t* tab_bits;
    uint16_t* tab_registers;
} modbus_mapping_t;


#define DISCRETE_NUM 2          
#define COILS_NUM 2            
#define INPUT_REGISTER_NUM 2     
#define HOLDING_REGISTER_NUM 2   


modbus_mapping_t* ModbusMappingNew(int nb_bits, int nb_registers);

modbus_mapping_t* ModbusMappingNewStartAddress(
    unsigned int start_bits, unsigned int nb_bits,
    unsigned int start_registers, unsigned int nb_registers);

// 초기값이 서버 구동 시 단 한번만 실행되도록 함
bool start_data = false;

// ---- (1) 간단한 상태 시뮬레이션용 전역 변수 ----
// 실제 공정값(초기: 25도, 760Torr, 유량 10sccm , O3값 0 , N2값 0 등 임의)
static int cham_temp_first_value = 25;
static int cham_press_first_value = 0;
static int cham_flow_first_value = 0;
static int gas_O3_first_value = 0;
static int gas_N2_first_value = 0;
static int gas_ZrO2_first_value = 0;
static int gas_HfO2_first_value = 0;
static int gas_H2O2_first_value = 0;
static int gas_TMA_first_value = 0;
static int wafer_size_first_value = 8;           // 8 , 12 인치만 존재
static int wafer_loading_first_value = 0;        // 0 : single , 1 : batch
static int wafer_flat_area_first_value = 10;     // 10 20 30 40 50 존재
static int wafer_amount_first_value = 100; // 초기값은 100

static int rf_power_first_value = 0; // 초기값은 0
static int plasma_forward_first_value = 0; // 초기값은 0
static int plasma_reflected_first_value = 0; // 초기값은 0

// Dry Pump On/Off 상태
static bool stop = false;
static bool valve_N2_on = false;
static bool valve_dry_pump_on = false;
static bool valve_O3_on = false;
static bool valve_ZrO2_on = false;
static bool valve_HfO2_on = false;
static bool valve_H2O2_on = false;
static bool valve_TMA_on = false;