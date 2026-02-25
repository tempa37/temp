#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "lwip/ip_addr.h"
#include "cmsis_os.h"
#include <stdbool.h>

// modbus master/slave rtu, tcp
#include "Modbus.h"

#define STATE_UNDEFINED 0
#define STATE_OFFLINE 1
#define STATE_ONLINE 2
#define STATE_USE 3

#define RING_LINE_CLOSED 1
#define RING_LINE_BREAK 0

#ifdef DEBUG
extern UBaseType_t WM_MBSlaveRTU;
extern UBaseType_t WM_MBSlaveTCP;
extern UBaseType_t WM_MBMasterRTU;
extern UBaseType_t WM_MBMasterTCP;
extern UBaseType_t WM_LWIP;
extern UBaseType_t WM_Scheduler;
extern UBaseType_t WM_OLED;
extern UBaseType_t WM_HTTPD;
extern UBaseType_t WM_Line;
extern UBaseType_t WM_KTV;
extern uint8_t selftest_state;
#endif

#define MAIN_BIT (1 << 0)
#define TEST_BIT (1 << 1)
#define DATA_BIT (1 << 2)
#define MODBUS_BIT (1 << 3)
#define nRING_LINE_BIT (1 << 4)
#define nBAN_TEST_BIT (1 << 5)

extern osEventFlagsId_t event_flags;

extern uint64_t opto1[];
extern uint64_t opto2[];

extern uint64_t opto3[];
extern uint64_t opto4[];

extern bool checkline;

extern modbusHandler_t ModbusTCPs;
extern modbusHandler_t ModbusTCPm;

extern osSemaphoreId_t httpdbufSemaphore;

extern uint8_t group;

//extern uint8_t line1_status;
extern uint8_t line2_status;

typedef struct {
  uint8_t camera:4;
  uint8_t supply:4;
  uint8_t valve:4;
  uint8_t sensor:4;
} equipment_t;

extern equipment_t equipment[10];

typedef enum {
  NELEMENT = 0,
  CAMERA = 1,
  SUPPLY = 2,
  RELAY = 3,
  SENSOR = 4,
  TEMP = 5,
} element_type_t;

// sizeof 24
typedef struct {
  element_type_t type;
  char name[9]; /*!< element name, 8 symbols + null-terminator */
  ip_addr_t ip_addr; /*!< IP address */
  uint8_t id_group;
  uint8_t id_modbus;
  uint16_t id_out;
  uint8_t dummy[3];
} e_t;

// sizeof 12
typedef struct {
  element_type_t type;
  uint16_t temp[4];
  uint8_t id_group;
  uint8_t dummy[3];
} t_t;

// sizeof 20
typedef struct {
  ip_addr_t usk_ip_addr; /*!< IP address */
  ip_addr_t usk_mask_addr;
  ip_addr_t usk_gateway_addr;  
  uint16_t port_camera;
  uint16_t scan_rate;
  uint16_t timeout;
  uint8_t urm_id;
  uint8_t umvh_id;
} s_t;

typedef struct {
  bool conv_IsOnline[2];
  bool water_IsOn[2];
  int16_t temp[2];
  uint8_t line1_status;
  uint8_t line2_status;
  uint8_t emergency_level[2];  
  bool camera_status[2];
  bool supply_status[2];
  uint8_t conv_count;
  uint8_t error;
  uint8_t mode[2];
  uint32_t address_module[2];
} oled_t;

extern oled_t msg;

typedef union {
  uint32_t ToUint32;
  struct {
    uint16_t serial_number;
    uint8_t mac_0;
    uint8_t mac_1;
  };
} identification_t;

extern identification_t identification;

typedef struct {
  s_t se;
  e_t el[40];
  t_t te[10];
} settings_t;

extern settings_t settings;

typedef struct {
  uint8_t extinction;
  uint8_t transporter;
} threshold_t;

extern threshold_t thres[4];

#endif /* __GLOBAL_H__ */