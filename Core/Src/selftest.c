#include "global_types.h"
#include "ktv.h"
#include "tim.h"
#include "cmsis_os2.h"
#include "selftest.h"
#include "string.h"

equipment_t equipment[10] = {0};

extern osMessageQueueId_t addr_msg_queue;
extern tsKtvElem aKtvElem[];

void set_before_selftest() {
  memset(equipment, 0, sizeof(equipment_t) * 10);
  
  for (uint8_t g = 0; g < 10; g++) {
    if (osSemaphoreAcquire(httpdbufSemaphore, 100) == osOK) {
      for (uint8_t el = 0; el < 10; el++) {
        if (settings.el[el].id_group == g &&
            settings.el[el].type == CAMERA)
        {
          equipment[el].camera = STATE_USE;
        }
        if (settings.el[el + 10].id_group == g &&
            settings.el[el + 10].type == RELAY)
        {
          equipment[el].supply = STATE_USE;
          equipment[el].valve = STATE_USE;
        }
        if (settings.el[el + 30].id_group == g &&
            settings.el[el + 30].type == SENSOR)
        {
          equipment[el].sensor = STATE_USE;
        }
      }
      osSemaphoreRelease(httpdbufSemaphore);
    }
  }
}

/**
 * @brief  
 *
 */
void selftest_thread(void *argument) {
  (void) argument;
  
  uint16_t mb_reg_camera[30] = {0};
  
  modbus_t telegram_camera = {
    // function code
    .u8fct = MB_FC_READ_REGISTERS,
    // start address
    .u16RegAdd = 100,
    // number of elements to read
    .u16CoilsNo = 30,
    // pointer to a memory array
    .u16reg = mb_reg_camera,
  };
  
  uint16_t mb_reg_supply = 0;
  
  modbus_t telegram_supply = {
    // function code
    .u8fct = MB_FC_WRITE_REGISTER,
    // start address
    .u16RegAdd = 6,
    // number of elements to read
    .u16CoilsNo = 1,
    // pointer to a memory array
    .u16reg = &mb_reg_supply,
  };
  
  uint32_t notification;
  
  uint32_t flags;
  
  while (1) {
    osEventFlagsWait(event_flags, MAIN_BIT|TEST_BIT|nRING_LINE_BIT|nBAN_TEST_BIT, osFlagsWaitAll|osFlagsNoClear, osWaitForever);
#ifdef DEBUG
    selftest_state = 1;
#endif

    if (osMessageQueueGetCount(addr_msg_queue) == 0) {
      if (KTV_State() == ksEnd || KTV_State() == ksNoActive) {
        KTVmsg addr_module = kmStart;
        osMessageQueuePut(addr_msg_queue, &addr_module, 0U, 10U);
        osMessageQueuePut(addr_msg_queue, &addr_module, 0U, 10U);
        osMessageQueuePut(addr_msg_queue, &addr_module, 0U, 10U);
        //osThreadYield();
      }
    }
    uint8_t count = 0;
    do {
      osDelay(500);
      count++;
    } while (KTV_State() != ksEnd || count < 20);
   
    for (uint8_t g = 0; g < group; g++) {
      for (uint8_t el = 0; el < 10; el++) {
        if (settings.el[el].id_group == g &&
            settings.el[el].type == CAMERA &&
              settings.el[el].ip_addr.addr != IPADDR_ANY &&
                settings.el[el].ip_addr.addr != IPADDR_NONE)
        {
          // set the Modbus request parameters
          telegram_camera.u8id = settings.el[el].id_modbus;
          telegram_camera.xIpAddress = settings.el[el].ip_addr.addr;
          telegram_camera.u8clientID = el;
          telegram_camera.u16Port = settings.se.port_camera;
          
          notification = modbus_query(&ModbusTCPm, &telegram_camera, 2);
          
          if (osSemaphoreAcquire(httpdbufSemaphore, 100) == osOK) {
            if (notification == MB_ERR_OK) {
              equipment[el].camera = STATE_ONLINE;
            } else {
              equipment[el].camera = STATE_OFFLINE;
            }
            osSemaphoreRelease(httpdbufSemaphore);
          }
        }
        
        flags = osEventFlagsGet(event_flags);
        if ((flags & TEST_BIT) == 0) {
          break;
        }
        
        if (settings.el[el + 10].id_group == g &&
            settings.el[el + 10].type == SUPPLY &&
              settings.el[el + 10].ip_addr.addr != IPADDR_ANY &&
                settings.el[el + 10].ip_addr.addr != IPADDR_NONE)
        {
          // set the Modbus request parameters
          telegram_supply.u8id = settings.el[el + 10].id_modbus;
          telegram_supply.xIpAddress = settings.el[el + 10].ip_addr.addr;
          telegram_supply.u16Port = settings.el[el + 10].id_out;
          telegram_supply.u8clientID = el + 10;
          telegram_supply.u16RegAdd = 6;
          
          // valve ON
          mb_reg_supply = 1;
          
          notification = modbus_query(&ModbusTCPm, &telegram_supply, 2);
          
          if (osSemaphoreAcquire(httpdbufSemaphore, 100) == osOK) {
            if (notification == MB_ERR_OK) {
              equipment[el].supply = STATE_ONLINE;
            } else {
              equipment[el].supply = STATE_OFFLINE;
            }
            osSemaphoreRelease(httpdbufSemaphore);
          }
        }
        
        flags = osEventFlagsGet(event_flags);
        if ((flags & TEST_BIT) == 0) {
          break;
        }

        if (settings.el[el + 30].id_group == g &&
            settings.el[el + 30].type == SENSOR)
        {
          if (aKtvElem[el + 1].Enum & KTV_TRIGGERED || aKtvElem[el + 1].Enum & KTV_ONLINE) {
            equipment[el].sensor = STATE_ONLINE;
          } else {
            equipment[el].sensor = STATE_OFFLINE;
          }
        }

        if (equipment[el].supply == STATE_ONLINE) {
          osDelay(2000);
        }
        
        if (settings.el[el + 10].id_group == g &&
            settings.el[el + 10].type == SUPPLY &&
            settings.el[el + 10].ip_addr.addr != IPADDR_ANY &&
            settings.el[el + 10].ip_addr.addr != IPADDR_NONE)
        {
          telegram_supply.u8id = settings.el[el + 10].id_modbus;
          telegram_supply.xIpAddress = settings.el[el + 10].ip_addr.addr;
          telegram_supply.u16Port = settings.el[el + 10].id_out;
          telegram_supply.u8clientID = el + 10;          
          telegram_supply.u16RegAdd = 1;
          telegram_supply.u8fct = MB_FC_READ_REGISTERS;
          mb_reg_supply = 0;
          
          notification = modbus_query(&ModbusTCPm, &telegram_supply, 2);
          
          if (osSemaphoreAcquire(httpdbufSemaphore, 100) == osOK) {
            if (notification == MB_ERR_OK && mb_reg_supply > 100) {
              equipment[el].valve = STATE_ONLINE;
            } else {
              equipment[el].valve = STATE_OFFLINE;
            }
            osSemaphoreRelease(httpdbufSemaphore);
          }
        }      

        if (settings.el[el + 10].id_group == g &&
            settings.el[el + 10].type == SUPPLY &&
            settings.el[el + 10].ip_addr.addr != IPADDR_ANY &&
            settings.el[el + 10].ip_addr.addr != IPADDR_NONE)
        {
          // set the Modbus request parameters
          telegram_supply.u8id = settings.el[el + 10].id_modbus;
          telegram_supply.xIpAddress = settings.el[el + 10].ip_addr.addr;
          telegram_supply.u16Port = settings.el[el + 10].id_out;
          telegram_supply.u8clientID = el + 10;
          telegram_supply.u16RegAdd = 6;
          telegram_supply.u8fct = MB_FC_WRITE_REGISTER;
          
          // valve OFF
          mb_reg_supply = 0;
          
          notification = modbus_query(&ModbusTCPm, &telegram_supply, 2);
          if (osSemaphoreAcquire(httpdbufSemaphore, 100) == osOK) {
            if (notification == MB_ERR_OK) {
              equipment[el].supply = STATE_ONLINE;
            } else {
              equipment[el].supply = STATE_OFFLINE;
            }
            osSemaphoreRelease(httpdbufSemaphore);
          }
        }
        flags = osEventFlagsGet(event_flags);
        if ((flags & TEST_BIT) == 0) {
          break;
        }
      }
    }
    osEventFlagsSet(event_flags, DATA_BIT);
    
    osEventFlagsClear(event_flags, TEST_BIT);
    
    osThreadYield();
  }
}