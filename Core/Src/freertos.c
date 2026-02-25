/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include "string.h"

#include "global_types.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
    
#include "httpd_netconn.h"

#include "selftest.h"
//
#include "netif.h"
//
#include "flash_if.h"
//
#include "version.h"
//
#include "ring_line.h"
//
#include "ktv.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


// "usart.c"
extern UART_HandleTypeDef huart1;
// "OLED_1in5_rgb_test.c"
extern void oled_thread(void *argument);

// "ktv.c"
extern tsKtvElem aKtvElem[];

// "lwip.c"
extern void MX_LWIP_Init(void);
extern struct netif gnetif;
extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;

extern uint8_t MACAddr[];

/* USER CODE END PTD */

#ifdef DEBUG
UBaseType_t WM_MBSlaveRTU;
UBaseType_t WM_MBSlaveTCP;

UBaseType_t WM_MBMasterRTU;
UBaseType_t WM_MBMasterTCP;

UBaseType_t WM_LWIP;
UBaseType_t WM_HTTPD;
UBaseType_t WM_ETH;
UBaseType_t WM_ETH_IN;

UBaseType_t WM_Scheduler;
UBaseType_t WM_OLED;

UBaseType_t WM_Line;
UBaseType_t WM_KTV;
UBaseType_t WM_URM;

uint32_t wd_tick;
uint8_t selftest_state;
#endif

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ip0 "192.168.70.5"
#define mk0 "255.255.255.0"
#define gw0 "192.168.70.1"

#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))

#define MODBUS_STEP_ADDR 50

#define ZERO_LEVEL 0
#define LOW_LEVEL 1
#define MEDIUM_LEVEL 2
#define HIGHT_LEVEL 3


#ifdef TEST_TIME
uint32_t zap[6] = {0};
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

// internal connection
modbusHandler_t ModbusRS2;
uint16_t ModbusDATA_RS2[1];

// modbus TCP master
modbusHandler_t ModbusTCPm;
uint16_t ModbusDATA_TCPm[30];


// modbus TCP slave
modbusHandler_t ModbusTCPs;
uint16_t ModbusDATA_TCPs[120];


threshold_t thres[4] = {0};

settings_t settings @ ".ccmram" = {0};

identification_t identification = {0};

oled_t msg = {0};

uint8_t group = 1;

bool configuration_change;

uint8_t urm_state = 0;

typedef struct {
  uint8_t camera:4;
  uint8_t supply:4;
  uint8_t supply_error:4;
  uint8_t supply_try:4;
} state_t;


#ifdef DEBUG
state_t equipment_state[10] = {0};
#endif

//
osThreadId_t wdi_task_handle;
const osThreadAttr_t wdi_task_handle_attr = {  
  .name = "WDI",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
//
osThreadId_t httpd_task_handle;

const osThreadAttr_t httpd_task_handle_attr = {  
  .name = "HTTP server",
  .stack_size = 128 * 64,
  .priority = (osPriority_t) osPriorityHigh,
};
//
osThreadId_t main_task_handle;
const osThreadAttr_t main_task_handle_attr = {
  .name = "Main",
  .stack_size = 128 * 12,
  .priority = (osPriority_t) osPriorityNormal,
};
//
osThreadId_t urm_task_handle;
const osThreadAttr_t urm_task_handle_attr = {
  .name = "LWIP & Relay",
  .stack_size = 128 * 13,
  .priority = (osPriority_t) osPriorityNormal,
};
//
osThreadId_t ring_line_task_handle;
const osThreadAttr_t ring_line_task_handle_attr = {
  .name = "Ring line",
  .stack_size = 128 * 6,
  .priority = (osPriority_t) osPriorityNormal,
};
//
osThreadId_t oled_task_handle;
const osThreadAttr_t oled_task_handle_attr = {
  .name = "Display",
  .stack_size = 128 * 12,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
//
osThreadId_t selftest_task_handle;
const osThreadAttr_t selftest_task_handle_attr = {
  .name = "Selftest",
  .stack_size = 128 * 6,
  .priority = (osPriority_t) osPriorityNormal,
};
//
osMessageQueueId_t oled_msg_queue;
//
osMessageQueueId_t relay_msg_queue;
//
osEventFlagsId_t event_flags;

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

static void urm_thread(void *argument);

static void wdi_thread(void *argument);

static void modbus_tcp_start(void);

static void modbus_rtu_start(void);

static void main_task_thread(void *argument);

void action_valve(uint8_t mode, uint8_t level, uint16_t* mb_supply_control_reg);

void action_relay(uint8_t mode, uint8_t level, uint16_t* mb_urm_control_reg, uint8_t element_id);

bool action_transporter(uint8_t mode, uint8_t level, uint8_t transporter_id);

/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {  
  identification.ToUint32 = (*(__IO uint32_t *)IDENTIFICATION_ADDR);
  
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x10;
  MACAddr[2] = 0xa1;
  MACAddr[3] = 0x71;
  MACAddr[4] = identification.mac_1;
  MACAddr[5] = identification.mac_0;
  
  readFlash(CONFIGURATION_ADDR, (uint32_t *)&settings, sizeof(settings_t));
  
  for (uint8_t el = 0; el < 10; el++) {
    if (settings.el[el].type == CAMERA &&
        (settings.el[el].id_group + 1) > group) 
    {
      group = settings.el[el].id_group + 1;
    }
  }
  
  // button "to the right" and button "to the left"
  if ((HAL_GPIO_ReadPin(K1_Port, K1_Pin) == 0 &&
       HAL_GPIO_ReadPin(K3_Port, K3_Pin) == 0) ||
       settings.se.usk_ip_addr.addr == IPADDR_NONE ||
       settings.se.usk_ip_addr.addr == IPADDR_ANY)
  {
    ip4_addr_set_u32(&settings.se.usk_ip_addr, ipaddr_addr(ip0));
    ip4_addr_set_u32(&settings.se.usk_mask_addr, ipaddr_addr(mk0));
    ip4_addr_set_u32(&settings.se.usk_gateway_addr, ipaddr_addr(gw0));
    
    settings.se.urm_id = 2;
    settings.se.umvh_id = 1;
    
    settings.se.port_camera = 1502;
    settings.se.scan_rate = 750;
    settings.se.timeout = 500;
  }
  
  ip4_addr_set(&ipaddr, &settings.se.usk_ip_addr);
  ip4_addr_set(&netmask, &settings.se.usk_mask_addr);
  ip4_addr_set(&gw, &settings.se.usk_gateway_addr);
  
  readFlash(STATE_ADDR, (uint32_t *)&thres, sizeof(threshold_t) * 4);
  
  if (thres[0].extinction > 1) {
    // 
    thres[0].extinction = 0;
    thres[0].transporter = 1;
    
    thres[1].extinction = 1;
    thres[1].transporter = 1;
    
    thres[2].extinction = 1;
    thres[2].transporter = 0;
    
    thres[3].extinction = 1;
    thres[3].transporter = 1;
  }
  
  /* USER CODE END Init */
  
  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */
  
  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */
  
  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  
  /* USER CODE END RTOS_TIMERS */
  
  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  
  oled_msg_queue = osMessageQueueNew(2, sizeof(oled_t), NULL);
  
  relay_msg_queue = osMessageQueueNew(2, sizeof(uint16_t), NULL);
 
  /* USER CODE END RTOS_QUEUES */
  
  event_flags = osEventFlagsNew(NULL);
  
  // oled display
  oled_task_handle = osThreadNew(oled_thread, NULL, &oled_task_handle_attr);
  
  // Create the thread(s)
  urm_task_handle = osThreadNew(urm_thread, NULL, &urm_task_handle_attr);
  
  /* USER CODE BEGIN RTOS_THREADS */
 
  // Watchdog
  wdi_task_handle = osThreadNew(wdi_thread, NULL, &wdi_task_handle_attr);
  
  // http server
  httpd_task_handle = osThreadNew(http_server_thread, NULL, &httpd_task_handle_attr);
  
  //httpd_task_handle = osThreadNew(web_server_thread, NULL, &httpd_task_handle_attr);
  
  
  //
  modbus_tcp_start();
  //
  modbus_rtu_start();
  //
  osEventFlagsSet(event_flags, MODBUS_BIT);
  //
  main_task_handle = osThreadNew(main_task_thread, NULL, &main_task_handle_attr);
  //  
  ring_line_task_handle = osThreadNew(ring_lines_thread, NULL, &ring_line_task_handle_attr);
  
  selftest_task_handle = osThreadNew(selftest_thread, NULL, &selftest_task_handle_attr);
  
  /* USER CODE END RTOS_THREADS */
  
  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  
  
  /* USER CODE END RTOS_EVENTS */
}

/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
static void urm_thread(void *argument) {
  (void) argument;
  //timer_delayed_start = xTimerCreate("delayed", 30000, pdFALSE, (void *)0, (TimerCallbackFunction_t)delayed_start_callback);
  // reset lan8710
  HAL_GPIO_WritePin(RST_PHYLAN_Port, RST_PHYLAN_Pin, GPIO_PIN_RESET);
  osDelay(1);
  HAL_GPIO_WritePin(RST_PHYLAN_Port, RST_PHYLAN_Pin, GPIO_PIN_SET);
  osDelay(1);
  //
  //xTimerStart(timer_delayed_start, 0);
  // init code for LWIP
  MX_LWIP_Init();

#ifdef DEBUG
  WM_LWIP = uxTaskGetStackHighWaterMark(NULL);
#endif
  
  uint32_t notification;
  
  uint16_t reg_urm = 0;

  modbus_t telegram_urm = {
    // function code 15
    .u8fct = MB_FC_WRITE_MULTIPLE_COILS,
    // start address
    .u16RegAdd = 0,
    // number of elements to read
    .u16CoilsNo = 16,
    // pointer to a memory array
    .u16reg = &reg_urm,
    // set the Modbus request parameters
    .u8id = settings.se.urm_id,
  };
  
  osEventFlagsWait(event_flags, MODBUS_BIT, osFlagsWaitAny|osFlagsNoClear, osWaitForever);

  while (1) {
    
#ifdef DEBUG
    WM_URM = uxTaskGetStackHighWaterMark(NULL);
#endif
    // get new data from queue
    osMessageQueueGet(relay_msg_queue, &reg_urm, NULL, 5U);

    notification = modbus_query(&ModbusRS2, &telegram_urm, 2);
    
    if (notification == MB_ERR_OK) {
      urm_state = STATE_ONLINE;
    } else {
      urm_state = STATE_OFFLINE;
    }
    osDelay(50U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == huart1.Instance) {
    HAL_UART_GetError(&huart1);
  }
}

void modbus_set_timeout(modbusHandler_t *mb) {
  if (osSemaphoreAcquire(mb->ModBusSphrHandle, 100) == osOK) { 
    set_timeout(mb, settings.se.timeout, settings.se.timeout);
    osSemaphoreRelease(mb->ModBusSphrHandle);
  }
}

extern osMessageQueueId_t addr_msg_queue;
uint8_t check_count;

osStatus_t set_ok(uint16_t reg) {
  osStatus_t stat = osError;
  stat = osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100);
  if (stat == osOK) {
    // ok -> o(---) k(-.-) -> 111101 -> 61
    ModbusTCPs.u16regs[reg] = 61;
    osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
  }
  return stat;
}

/**
 * @brief  
 *
 */
static void main_task_thread(void *argument) {
  (void) argument;
  
#ifdef RELEASE
  state_t equipment_state[10] = {0};
#endif

  uint8_t emergency_level[10] = {0};

  uint8_t danger_level[2][10] = {0};

  uint16_t mb_reg_urm = 0;

  uint8_t work_mode;  
  
  int16_t mb_reg_camera[30] = {0};

  modbus_t telegram_camera = {
    // function code
    .u8fct = MB_FC_READ_REGISTERS,
    // start address
    .u16RegAdd = 100,
    // number of elements to read
    .u16CoilsNo = 30,
    // pointer to a memory array
    .u16reg = (uint16_t *)mb_reg_camera,
    .u8clientID = 0,
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
    .u8clientID = 0,
  };
  
  uint16_t mb_reg_diagnostics_supply[7] = {0};
  
  modbus_t telegram_diagnostics_supply = {
    // function code
    .u8fct = MB_FC_READ_REGISTERS,
    // start address
    .u16RegAdd = 0,
    // number of elements to read
    .u16CoilsNo = 7,
    // pointer to a memory array
    .u16reg = mb_reg_diagnostics_supply,
    .u8clientID = 0,
  };
  
  uint32_t notification;

  oled_t msg_oled;
  
  uint8_t max_level;
  
  int16_t temp_max;

  //osDelay(3000);
  //HAL_GPIO_WritePin(GPIOD, CON_1_Pin, GPIO_PIN_SET);
  //osDelay(1000);
  //HAL_GPIO_WritePin(GPIOD, CON_2_Pin, GPIO_PIN_SET);  
  //osDelay(1000);

  osEventFlagsWait(event_flags, MODBUS_BIT, osFlagsWaitAny|osFlagsNoClear, osWaitForever);

  if (settings.se.timeout >= 500) {
    
    modbus_set_timeout(&ModbusTCPm);
    
    modbus_set_timeout(&ModbusRS2);
  }
  
  if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
/*    
    if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
      setTimeOut(&ModbusTCPs, 5000);
      osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
    }
*/    
    // Time between modbus polls
    ModbusTCPs.u16regs[5] = settings.se.scan_rate;
    // Modbus response timeout
    ModbusTCPs.u16regs[6] = settings.se.timeout;
    //
    for (uint8_t i = 0; i < 4; i++) {
      // set only for used
      if (settings.te[0].type == TEMP) {
        ModbusTCPs.u16regs[22 + i] = settings.te[0].temp[i];
      } else {
        ModbusTCPs.u16regs[22 + i] = 0;
      }
      // set only for used
      if (settings.te[1].type == TEMP) {
        ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + i] = settings.te[1].temp[i];
      } else {
        ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + i] = 0;
      }
    }
    osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
  }
  
  configuration_change = false;
  
  uint32_t flags;
  
  uint32_t diagnostics_supply = 0;
  uint8_t max_used_element = 0;
  
  uint8_t client_id;
  uint32_t client_ip;
  
  //uint32_t tick = osKernelGetTickCount();
  
  uint8_t user_fire[2] = {0};
  
  while (1) {
#ifdef DEBUG
    WM_Scheduler = uxTaskGetStackHighWaterMark(NULL);
#endif
    
/*
    if (settings.se.scan_rate < 300) {
      //osDelay(300);
      tick += pdMS_TO_TICKS(300);
    } else {
      //osDelay(settings.se.scan_rate);
      tick += pdMS_TO_TICKS(settings.se.scan_rate);      
    }
*/

    osEventFlagsClear(event_flags, MAIN_BIT);
    
    flags = osEventFlagsGet(event_flags);
    
    if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
      if (ModbusTCPs.u16regs[4] == 55) {
        ModbusTCPs.u16regs[4] = 56;
        set_before_selftest();
        osEventFlagsSet(event_flags, TEST_BIT);
      }
      
      if ((flags & DATA_BIT) != 0) {
        ModbusTCPs.u16regs[4] = 57;
        for (uint8_t el = 0; el < 10; el++) {
          
          if (settings.el[el].type == CAMERA &&
              settings.el[el].ip_addr.addr != IPADDR_ANY &&
              settings.el[el].ip_addr.addr != IPADDR_NONE)
          {
            if (settings.el[el].id_group == 0) {
              if (equipment[el].camera == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[11], el);
              } else {
                bitClear(ModbusTCPs.u16regs[11], el);
              }
            } else {
              if (equipment[el].camera == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[11 + MODBUS_STEP_ADDR], el);
              } else {
                bitClear(ModbusTCPs.u16regs[11 + MODBUS_STEP_ADDR], el);
              }
            }
          }
          
          if (settings.el[el + 10].type == SUPPLY &&
              settings.el[el + 10].ip_addr.addr != IPADDR_ANY &&
              settings.el[el + 10].ip_addr.addr != IPADDR_NONE)
          {
            if (settings.el[el + 10].id_group == 0) {
              
              if (equipment[el].supply == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[12], el);
              } else {
                bitClear(ModbusTCPs.u16regs[12], el);
              }
              
              if (equipment[el].valve == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[13], el);
              } else {
                bitClear(ModbusTCPs.u16regs[13], el);
              }
            } else {
              
              if (equipment[el].supply == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[12 + MODBUS_STEP_ADDR], el);
              } else {
                bitClear(ModbusTCPs.u16regs[12 + MODBUS_STEP_ADDR], el);
              }
              
              if (equipment[el].valve == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[13 + MODBUS_STEP_ADDR], el);
              } else {
                bitClear(ModbusTCPs.u16regs[13 + MODBUS_STEP_ADDR], el);
              }
            }
          }
          
          if (settings.el[el + 30].type == SENSOR)
          {
            if (settings.el[el + 30].id_group == 0) {
              if (equipment[el].sensor == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[14], el);
              } else {
                bitClear(ModbusTCPs.u16regs[14], el);
              }
            } else { 
              if (equipment[el].sensor == STATE_OFFLINE) {
                bitSet(ModbusTCPs.u16regs[14 + MODBUS_STEP_ADDR], el);
              } else {
                bitClear(ModbusTCPs.u16regs[14 + MODBUS_STEP_ADDR], el);
              }
            }
            //equipment[el].sensor = STATE_OFFLINE;
          }
        }
        
        osEventFlagsClear(event_flags, DATA_BIT);
      }
      osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
    }
    
    if ((flags & (MAIN_BIT|TEST_BIT|DATA_BIT)) == 0) {
      
#ifdef DEBUG
      selftest_state = 0;
#endif
      if (configuration_change) {
        //modbus_set_timeout(&ModbusTCPs);
        
/*
        if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
          setTimeOut(&ModbusTCPs, 5000);
          osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
        }
*/

        modbus_set_timeout(&ModbusTCPm);
        modbus_set_timeout(&ModbusRS2);
        
        configuration_change = false;
      }

      // check -> c(-.-.) h(....) e(.) c(-.-.) k(-.-)  -> 1010 0000 0 1010 101  -> 41045
      if (ModbusTCPs.u16regs[16] == 41045) {
        if (osMessageQueueGetCount(addr_msg_queue) == 0 && 
            (KTV_State() == ksEnd || KTV_State() == ksNoActive)) {
          KTVmsg addr_module = kmStart;
          osStatus_t status = osMessageQueuePut(addr_msg_queue, &addr_module, 0U, 0U);
          if (status == osOK) {
            check_count++;
            if (check_count > 2) {
              check_count = 0;
              set_ok(16);
            }
          }
        }
      }
      
      msg.conv_count = group;
      
      for (uint8_t g = 0; g < group; g++) {
        msg.temp[g] = -1;
        // CAMERA
        msg.camera_status[g] = true;
        //
        for (uint8_t el = 0; el < 10; el++) {
          // IP is set?
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

#ifdef TEST_TIME
            zap[0] = HAL_GetTick();
#endif
            
            for (uint8_t i = 0; i < 30; i++) {
              mb_reg_camera[i] = -1;
            }

            notification = modbus_query(&ModbusTCPm, &telegram_camera, 2);
            
#ifdef TEST_TIME
            zap[1] = HAL_GetTick();
            zap[2] = zap[1] - zap[0];
#endif
            
            if (notification == MB_ERR_OK) {
              temp_max = -1;
              //if (osSemaphoreAcquire(ModbusTCPm.ModBusSphrHandle, 100) == osOK) {
              // search for maximum temperature
              for (uint8_t j = 0; j < 30; j++) {
                if (mb_reg_camera[j] > temp_max) {
                  temp_max = mb_reg_camera[j];
                }
              }
              //  osSemaphoreRelease(ModbusTCPm.ModBusSphrHandle);
              //}
              
              if (msg.temp[g] < temp_max) {
                msg.temp[g] = temp_max;
              }
              
              // if value is less than the reset threshold
              if (temp_max < (int16_t)settings.te[g].temp[0]) {
                danger_level[g][el] = ZERO_LEVEL;
              } else {
                // warning level + extinguishing switch on + conveyor shutdown
                if (temp_max >= (int16_t)settings.te[g].temp[3]) {
                  danger_level[g][el] = HIGHT_LEVEL;
                }
                // warning level + extinguishing switch on
                else if (temp_max >= (int16_t)settings.te[g].temp[2] && danger_level[g][el] < 2) {
                  danger_level[g][el] = MEDIUM_LEVEL;
                }
                // warning level
                else if (temp_max >= (int16_t)settings.te[g].temp[1] && danger_level[g][el] < 1) {
                  danger_level[g][el] = LOW_LEVEL;
                }
              }
              equipment_state[el].camera = STATE_ONLINE + danger_level[g][el];
            } else {            
              equipment_state[el].camera = STATE_OFFLINE;
              // if one of the cameras used is not connected then on the display status offline
              msg.camera_status[g] = false;
            }
          }
        } // for (uint8_t el = 0; el < 10; el++)
        
        // search for the highest trigger level
        max_level = 0;
        for (uint8_t el = 0; el < 10; el++) {
          if (danger_level[g][el] > max_level) {
            max_level = danger_level[g][el];
          }
        }
        
        emergency_level[g] = max_level;
        msg.emergency_level[g] = max_level;
        
        flags = osEventFlagsGet(event_flags);
        
        if ((flags & nRING_LINE_BIT) == 0) {
          work_mode = RING_LINE_CLOSED;
        } else {
          work_mode = RING_LINE_BREAK;
        }
        
        if (max_level > LOW_LEVEL) {
          osEventFlagsClear(event_flags, nBAN_TEST_BIT); 
        } else {
          osEventFlagsSet(event_flags, nBAN_TEST_BIT);
        }
        
/*
        if (line1_status == 0) {
          work_mode = 1;
          osEventFlagsClear(event_flags, RING_LINE_BIT);
        } else {
          work_mode = 0;
          osEventFlagsSet(event_flags, RING_LINE_BIT);
        }
*/
        
        if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
        
        // fire -> f(..-.) i(..) r(.-.) e(.) 1(.----)  -> 0010 00 010 0 01111  -> 4239
        if ((ModbusTCPs.u16regs[17] > 0) && (ModbusTCPs.u16regs[4] == 4239)) {
          user_fire[0] = 2;
          //if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
            // ok -> o(---) k(-.-) -> 111101-> 61
            ModbusTCPs.u16regs[4] = 61;
            //osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
          //}
          // fire -> f(..-.) i(..) r(.-.) e(.) 2(..---)  -> 0010 00 010 0 00111  -> 4231
        } else if ((ModbusTCPs.u16regs[17 + MODBUS_STEP_ADDR] > 0) && (ModbusTCPs.u16regs[4] == 4231)) {
          user_fire[1] = 2;
          //work_mode = 2;
          //if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
            // ok -> o(---) k(-.-) -> 111101-> 61
            ModbusTCPs.u16regs[4] = 61;
            //osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
          //}
        } else if ((ModbusTCPs.u16regs[17] == 0) && (ModbusTCPs.u16regs[4] == 4239)) {
          user_fire[0] = 0;
          //if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
            // ok -> o(---) k(-.-) -> 111101-> 61
            ModbusTCPs.u16regs[4] = 61;
            //osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
          //}
        } else if ((ModbusTCPs.u16regs[17 + MODBUS_STEP_ADDR] == 0) && (ModbusTCPs.u16regs[4] == 4231)) {
          user_fire[1] = 0;
          //if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
            // ok -> o(---) k(-.-) -> 111101-> 61
            ModbusTCPs.u16regs[4] = 61;
            //osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
          //}
        }
        
        osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
        }
        
        if (user_fire[g] > 0) {
          work_mode = user_fire[g];
        } else if (user_fire[g] == 0 && work_mode == 2) {
          work_mode = 0;
        }

        msg.mode[g] = work_mode;

        // SUPPLY
        msg.supply_status[g] = true;
        //
        for (uint8_t el = 0; el < 10; el++) {
          if (settings.el[el + 10].id_group == g &&
              settings.el[el + 10].type == SUPPLY &&
                settings.el[el + 10].ip_addr.addr != IPADDR_ANY &&
                  settings.el[el + 10].ip_addr.addr != IPADDR_NONE)
          {
            // set the Modbus request parameters
            telegram_supply.u8id = settings.el[el + 10].id_modbus;
            telegram_supply.xIpAddress = settings.el[el + 10].ip_addr.addr;
            telegram_supply.u16Port = settings.el[el + 10].id_out;
            
            if (el == 0) {
              client_id = el + 10;
              client_ip = settings.el[el + 10].ip_addr.addr;
              telegram_supply.u8clientID = el + 10;
            } else {
              
              if (settings.el[el + 10].ip_addr.addr == client_ip) {
                telegram_supply.u8clientID = client_id;
                osDelay(300);
              } else {
                client_id = el + 10;
                telegram_supply.u8clientID = client_id;
                client_ip = settings.el[el + 10].ip_addr.addr;
              }
            }
            /*
            uint8_t dup_ip = 11;
            if (el > 0) {
              for (uint8_t dub_el = 0; dub_el < el; dub_el++) {
                if (settings.el[dub_el + 10].id_group == g &&
                    settings.el[dub_el + 10].type == SUPPLY &&
                      settings.el[dub_el + 10].ip_addr.addr != IPADDR_ANY &&
                        settings.el[dub_el + 10].ip_addr.addr != IPADDR_NONE &&
                          settings.el[dub_el + 10].ip_addr.addr == telegram_supply.xIpAddress)
                {
                  dup_ip = dub_el;
                  break;
                }
              }              
            }
            
            if (el > 0 && dup_ip != 11) {
              telegram_supply.u8clientID = dup_ip + 10;
            } else {
              telegram_supply.u8clientID = el + 10;
            }
            */
            telegram_diagnostics_supply.u8id = telegram_supply.u8id;
            telegram_diagnostics_supply.xIpAddress = telegram_supply.xIpAddress;
            telegram_diagnostics_supply.u16Port = telegram_supply.u16Port;
            telegram_diagnostics_supply.u8clientID = telegram_supply.u8clientID;
            
            action_valve(work_mode, max_level, &mb_reg_supply);
            
            if (mb_reg_supply) {
              msg.water_IsOn[g] = true;
            } else {
              msg.water_IsOn[g] = false;
            }
            
#ifdef TEST_TIME
            zap[3] = HAL_GetTick();
#endif
            
            notification = modbus_query(&ModbusTCPm, &telegram_supply, 2);
            //notification = modbus_query(&ModbusTCPm, &telegram_diagnostics_supply, 2);
            
#ifdef TEST_TIME
            zap[4] = HAL_GetTick();
            zap[5] = zap[4] - zap[3];
#endif
            
            if (notification == MB_ERR_OK) {
              equipment_state[el].supply_try = 0;
              if (equipment_state[el].supply_error) {
                equipment_state[el].supply = equipment_state[el].supply_error;
              } else {
                equipment_state[el].supply = STATE_ONLINE + mb_reg_supply;
              }
            } else {
              if (equipment_state[el].supply_try < 8) {
                equipment_state[el].supply_try++;
              }
              
              if (equipment_state[el].supply_try < 2) {
                if (equipment_state[el].supply_error) {
                  equipment_state[el].supply = equipment_state[el].supply_error;
                } else {
                  equipment_state[el].supply = STATE_ONLINE + mb_reg_supply;
                }
              } else {
                equipment_state[el].supply = STATE_OFFLINE;
                // if one of the cameras used is not connected then on the display status offline
                msg.supply_status[g] = false;
              }
            }
            
            if (max_used_element < el) {
              max_used_element = el;
            }
           
            if (diagnostics_supply < HAL_GetTick()) {
              if (el == max_used_element) {
                diagnostics_supply = HAL_GetTick() + 10000;
              }
              osDelay(300);
              notification = modbus_query(&ModbusTCPm, &telegram_diagnostics_supply, 2);
              
              if (notification == MB_ERR_OK) {
                equipment_state[el].supply_error = 0;
                if (mb_reg_diagnostics_supply[0] == 0 && mb_reg_diagnostics_supply[6] == 1) {
                  /// error in output voltage measurement
                  equipment_state[el].supply_error = 4;
                }
                if (mb_reg_diagnostics_supply[4] == 3) {
                  /// no battery
                  equipment_state[el].supply_error = 5;
                }
                if (mb_reg_diagnostics_supply[4] == 4) {
                  /// bms error
                  equipment_state[el].supply_error = 6;
                }
                if (mb_reg_diagnostics_supply[3] == 0) {
                  /// No external power supply
                  equipment_state[el].supply_error = 7;
                }
              }
            }           
          }
        } // for (uint8_t el = 0; el < 10; el++)
        
        // RELAY
        for (uint8_t el = 0; el < 10; el++) {
          if (settings.el[el + 20].id_group == g && 
              settings.el[el + 20].type == RELAY)
          {
            action_relay(work_mode, max_level, &mb_reg_urm, el);
          }
        }
        
        osMessageQueuePut(relay_msg_queue, &mb_reg_urm, 0U, 5U);
        // suspend thread
        osThreadYield();
        
        // SENSOR -> KTV | SENSOR -> UMVH
        
        // set the state of the conveyors
        
        msg.conv_IsOnline[g] = action_transporter(work_mode, max_level, g);
        
        // ... in the extended version, perform switching for next groups
        // switch via universal relay module
        
      } // for (uint8_t g = 0; g < group; g++)

      if (osSemaphoreAcquire(ModbusTCPs.ModBusSphrHandle, 100) == osOK) {
        // save -> s(...) a(.-) v(...-) e(.) -> 000 01 0001 0  -> 34
        if (ModbusTCPs.u16regs[4] == 34) {
          
          if (ModbusTCPs.u16regs[5] >= 300 && ModbusTCPs.u16regs[5] <= 15000) {
            // Time between modbus polls
            settings.se.scan_rate = ModbusTCPs.u16regs[5];
          }
          if (ModbusTCPs.u16regs[6] >= 500 && ModbusTCPs.u16regs[6] <= 10000) {
            // Modbus response timeout
            settings.se.timeout = ModbusTCPs.u16regs[6];
          }
          //
          if (settings.te[0].type == TEMP) {
            if (ModbusTCPs.u16regs[22 + 0] >= ModbusTCPs.u16regs[22 + 1] ||
                ModbusTCPs.u16regs[22 + 1] >= ModbusTCPs.u16regs[22 + 2] ||
                  ModbusTCPs.u16regs[22 + 2] >= ModbusTCPs.u16regs[22 + 3]
                    )
            {
              // error e(.) r(.-.) r(.-.) 0(---) r(.-.) 0010010111010 -> 1210
              ModbusTCPs.u16regs[4] = 1210;
            } else {
              for (uint8_t i = 0; i < 4; i++) {
                settings.te[0].temp[i] = ModbusTCPs.u16regs[22 + i];
              }
            }
          }
          //
          if (settings.te[1].type == TEMP) {
            if (ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 0] >= ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 1] ||
                ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 1] >= ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 2] ||
                  ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 2] >= ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + 3]
                    )
            {
              // error e(.) r(.-.) r(.-.) 0(---) r(.-.) 0010010111010 -> 1210
              ModbusTCPs.u16regs[4] = 1210;
            } else {
              for (uint8_t i = 0; i < 4; i++) {
                settings.te[1].temp[i] = ModbusTCPs.u16regs[22 + MODBUS_STEP_ADDR + i];
              }
            }
          }
          
          // sector 14
          FlashWORD(CONFIGURATION_ADDR, (uint32_t *)&settings, sizeof(settings_t));
          
          // ok -> o(---) k(-.-) -> 111101-> 61
          ModbusTCPs.u16regs[4] = 61;
          //
          configuration_change = true;
        }
        
        ModbusTCPs.u16regs[0] = VERSION_MAJOR;
        ModbusTCPs.u16regs[1] = VERSION_MINOR;
        ModbusTCPs.u16regs[2] = VERSION_BUILD;      
        ModbusTCPs.u16regs[3] = identification.serial_number;  
        // [4] save
        // [5] scan_rate
        // [6] timeout
        ModbusTCPs.u16regs[7] = urm_state;
        
        ModbusTCPs.u16regs[8] = group;

        uint32_t up_time = HAL_GetTick()/1000;
        memcpy(&ModbusTCPs.u16regs[9], &up_time, sizeof(up_time));
        
        // [9] 
        // ...
        // [18]
        
        ModbusTCPs.u16regs[18] = (uint16_t)msg.water_IsOn[0];
        ModbusTCPs.u16regs[19] = msg.temp[0];
        ModbusTCPs.u16regs[20] = emergency_level[0];
        ModbusTCPs.u16regs[21] = line2_status;
        
        ModbusTCPs.u16regs[18 + MODBUS_STEP_ADDR] = (uint16_t)msg.water_IsOn[1];
        ModbusTCPs.u16regs[19 + MODBUS_STEP_ADDR] = msg.temp[1];
        ModbusTCPs.u16regs[20 + MODBUS_STEP_ADDR] = emergency_level[1];
        ModbusTCPs.u16regs[21 + MODBUS_STEP_ADDR] = line2_status;      
        
        msg.address_module[0] = 0;
        msg.address_module[1] = 0;
        
        for (uint8_t i = 0; i < 10; i++) {
          if (settings.el[i].id_group == 0 && settings.el[i].type == CAMERA) {
            ModbusTCPs.u16regs[26 + i] = equipment_state[i].camera;
          } else if (settings.el[i].id_group == 1 && settings.el[i].type == CAMERA) {
            ModbusTCPs.u16regs[26 + MODBUS_STEP_ADDR + i] = equipment_state[i].camera;
          }
          if (settings.el[i + 10].id_group == 0 && settings.el[i + 10].type == SUPPLY) {
            ModbusTCPs.u16regs[36 + i] = equipment_state[i].supply;
          } else if (settings.el[i + 10].id_group == 1 && settings.el[i + 10].type == SUPPLY) {
            ModbusTCPs.u16regs[36 + MODBUS_STEP_ADDR + i] = equipment_state[i].supply;
          }
          
          uint8_t address_modul_state = 0;          
          if (settings.el[i + 30].id_group == 0 && settings.el[i + 30].type == SENSOR) {
            // KTV addresses start with 1
            address_modul_state = 1;
            if (aKtvElem[i + 1].Enum == 1) {
              address_modul_state = 3;
              bitSet(msg.address_module[0], i+1);
            } else if (aKtvElem[i + 1].Enum == 3) {
              address_modul_state = 2;
              bitClear(msg.address_module[0], i+1);
            } else if (aKtvElem[21].Enum == 3) {
              address_modul_state = 4;
            }
            //ModbusTCPs.u16regs[46 + i] = aKtvElem[i + 1].Enum;
            ModbusTCPs.u16regs[46 + i] = address_modul_state;            
          } else if (settings.el[i + 30].id_group == 1 && settings.el[i + 30].type == SENSOR) {
            // KTV addresses start with 1
            address_modul_state = 1;
            if (aKtvElem[i + 1].Enum == 1) {
              address_modul_state = 3;
              bitSet(msg.address_module[1], i+1);
            } else if (aKtvElem[i + 1].Enum == 3) {
              address_modul_state = 2;
              bitClear(msg.address_module[1], i+1);
            } else if (aKtvElem[21].Enum == 3) {
              address_modul_state = 4;
            }
            //ModbusTCPs.u16regs[46 + MODBUS_STEP_ADDR + i] = aKtvElem[i + 1].Enum;
            ModbusTCPs.u16regs[46 + MODBUS_STEP_ADDR + i] = address_modul_state;
          }
        }
        
        if (aKtvElem[21].Enum == 3) {
          bitSet(msg.address_module[0], 21);
          bitSet(msg.address_module[1], 21);
        }
        
        osSemaphoreRelease(ModbusTCPs.ModBusSphrHandle);
      }
      
      if (memcmp(&msg, &msg_oled, sizeof(oled_t)) != 0) {
        memcpy(&msg_oled, &msg, sizeof(oled_t));
        
        osMessageQueuePut(oled_msg_queue, &msg, 0U, 5U);
        // suspend thread
        osThreadYield();
      }
      
      osEventFlagsSet(event_flags, MAIN_BIT);
    
    }
    
    //osDelayUntil(tick);

    if (settings.se.scan_rate < 300) {
      osDelay(300);
    } else {
      osDelay(settings.se.scan_rate);
    }

  }
}

/**
 * @brief  
 *
 */
void action_valve(uint8_t mode, uint8_t level, uint16_t* mb_supply_control_reg) {
  // if manual mode
  if (mode == 1 || mode == 2) {
    if (thres[3].extinction == 1) {
      *mb_supply_control_reg = 1;
    } else {
      *mb_supply_control_reg = 0;
    }
  } else {
    // prepare data for sending
    switch (level) {
      case ZERO_LEVEL: {
        *mb_supply_control_reg = 0;
        break;
      }
      case LOW_LEVEL: {
        if (thres[0].extinction == 1) {
          *mb_supply_control_reg = 1;
        } else {
          *mb_supply_control_reg = 0;
        }
        break;
      }
      case MEDIUM_LEVEL: {
        if (thres[1].extinction == 1) {
          *mb_supply_control_reg = 1;
        } else {
          *mb_supply_control_reg = 0;
        }
        break;
      }
      case HIGHT_LEVEL: {
        if (thres[2].extinction == 1) {
          *mb_supply_control_reg = 1;
        } else {
          *mb_supply_control_reg = 0;
        }
        break;
      }
      default:{
        break;
      }
    }
  }
}

/**
 * @brief  
 *
 */
void action_relay(uint8_t mode, uint8_t level, uint16_t* mb_urm_control_reg, uint8_t element_id) {
  element_id += 20;
  uint8_t relay_out_number = settings.el[element_id].id_out - 1;
  // if manual mode
  if (mode == 1 || mode == 2) {
    if (thres[3].extinction == 1) {
      // set relay to ON state
      // relay output addresses start with 0
      bitSet(*mb_urm_control_reg, relay_out_number);
    } else {
      // set relay to OFF state
      // relay output addresses start with 0
      bitClear(*mb_urm_control_reg, relay_out_number);
    }
  } else {
    // prepare data for sending
    switch (level) {
      case ZERO_LEVEL: {
        // set relay to OFF state
        // relay output addresses start with 0
        bitClear(*mb_urm_control_reg, relay_out_number);
        break;
      }
      case LOW_LEVEL: {
        if (thres[0].extinction == 1) {
          // set relay to ON state
          // relay output addresses start with 0
          bitSet(*mb_urm_control_reg, relay_out_number);
        } else {
          // set relay to OFF state
          // relay output addresses start with 0
          bitClear(*mb_urm_control_reg, relay_out_number);
        }
        break;
      }
      case MEDIUM_LEVEL: {
        if (thres[1].extinction == 1) {
          // set relay to ON state
          // relay output addresses start with 0
          bitSet(*mb_urm_control_reg, relay_out_number);
        } else {
          // set relay to OFF state
          // relay output addresses start with 0
          bitClear(*mb_urm_control_reg, relay_out_number);
        }
        break;
      }
      case HIGHT_LEVEL: {
        if (thres[2].extinction == 1) {
          // set relay to ON state
          // relay output addresses start with 0
          bitSet(*mb_urm_control_reg, relay_out_number);
        } else {
          // set relay to OFF state
          // relay output addresses start with 0
          bitClear(*mb_urm_control_reg, relay_out_number);
        }
        break;
      }
      default:{
        break;
      }
    }
  }
}

#define TRANSPORTER_1_SWITCH_ON  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET)
#define TRANSPORTER_1_SWITCH_OFF HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET)

#define TRANSPORTER_2_SWITCH_ON  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET)
#define TRANSPORTER_2_SWITCH_OFF HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET)

#define TRANSPORTER_ON 1
#define TRANSPORTER_1 0

/**
 * @brief  
 *
 */
bool action_transporter(uint8_t mode, uint8_t level, uint8_t transporter_id) {
  bool res = false;
  if (transporter_id > 1) {
    return res;
  }
  // if manual mode
  if (mode == 1 || mode == 2) {
    if (thres[3].transporter == TRANSPORTER_ON) {
      if (transporter_id == TRANSPORTER_1) {
        TRANSPORTER_1_SWITCH_ON;
      } else {
        TRANSPORTER_2_SWITCH_ON;
      }
      res = true;
    } else {
      if (transporter_id == TRANSPORTER_1) {
        TRANSPORTER_1_SWITCH_OFF;
      } else {
        TRANSPORTER_2_SWITCH_OFF;
      }
      res = false;
    }
  } else {
    switch (level) {
      case ZERO_LEVEL: {
        if (transporter_id == TRANSPORTER_1) {
          TRANSPORTER_1_SWITCH_ON;
        } else {
          TRANSPORTER_2_SWITCH_ON;
        }
        res = true;
        break;
      }
      case LOW_LEVEL: {
        if (thres[0].transporter == TRANSPORTER_ON) {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_ON;
          } else {
            TRANSPORTER_2_SWITCH_ON;
          }
          res = true;
        } else {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_OFF;
          } else {
            TRANSPORTER_2_SWITCH_OFF;
          }
          res = false;
        }
        break;
      }
      case MEDIUM_LEVEL: {
        if (thres[1].transporter == TRANSPORTER_ON) {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_ON;
          } else {
            TRANSPORTER_2_SWITCH_ON;
          }
          res = true;
        } else {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_OFF;
          } else {
            TRANSPORTER_2_SWITCH_OFF;
          }
          res = false;
        }
        break;
      }
      case HIGHT_LEVEL: {
        if (thres[2].transporter == TRANSPORTER_ON) {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_ON;
          } else {
            TRANSPORTER_2_SWITCH_ON;
          }
          res = true;
        } else {
          if (transporter_id == TRANSPORTER_1) {
            TRANSPORTER_1_SWITCH_OFF;
          } else {
            TRANSPORTER_2_SWITCH_OFF;
          }
          res = false;
        }
        break;
      }
      default:{
        break;
      }
    }
  }
  return res;
}

/**
 * @brief Modbus RTU Master and Slave
 * @retval None
 */
static void modbus_rtu_start(void) {
  memset(&ModbusRS2, 0, sizeof(modbusHandler_t));
  // Modbus RTU master - internal connection
  ModbusRS2.uModbusType = MB_MASTER;
  ModbusRS2.num_tcp_conn = 0;
  ModbusRS2.port = &huart1;
  // RS485 Enable port
  ModbusRS2.EN_Port = GPIOA;
  // RS485 Enable pin
  ModbusRS2.EN_Pin = GPIO_PIN_11;
  // For master it must be 0
  ModbusRS2.u8id = 0;
  ModbusRS2.sendtimeout = 1000;
  ModbusRS2.recvtimeout = 1000;
  ModbusRS2.u16regs = ModbusDATA_RS2;
  ModbusRS2.u16regsize = sizeof(ModbusDATA_RS2)/sizeof(ModbusDATA_RS2[0]);
  ModbusRS2.xTypeHW = USART_HW;
  ModbusInit(&ModbusRS2);
  //Start capturing traffic on serial Port
  ModbusStart(&ModbusRS2);
}

/**
 * @brief Modbus TCP Master and Slave
 * @retval None
 */
static void modbus_tcp_start(void) {
  memset(&ModbusTCPm, 0, sizeof(modbusHandler_t));
  ModbusTCPm.uModbusType = MB_MASTER;
  //ModbusTCPm.newconns = master_newconns;
  ModbusTCPm.num_tcp_conn = 20;
  ModbusTCPm.u8id = 0;
  // for a master it could be higher depending on the slave speed
  ModbusTCPm.sendtimeout = 1000;
  ModbusTCPm.recvtimeout = 1000;
  // No RS485
  ModbusTCPm.EN_Port = NULL;
  ModbusTCPm.u16regs = ModbusDATA_TCPm;
  ModbusTCPm.u16regsize = sizeof(ModbusDATA_TCPm)/sizeof(ModbusDATA_TCPm[0]);
  // TCP hardware
  ModbusTCPm.xTypeHW = TCP_HW;
  ModbusInit(&ModbusTCPm);
  // Start capturing traffic on serial Port and initialize counters
  ModbusStart(&ModbusTCPm);
  
  memset(&ModbusTCPs, 0, sizeof(modbusHandler_t));  
  // Modbus TCP slave - external connection
  ModbusTCPs.uModbusType = MB_SLAVE;
  //ModbusTCPs.newconns = slave_newconns;
  ModbusTCPs.num_tcp_conn = 3;
  ModbusTCPs.u8id = 1;
  // for a master it could be higher depending on the slave speed
  ModbusTCPs.sendtimeout = 5000;
  ModbusTCPs.recvtimeout = 5000;
  // No RS485
  ModbusTCPs.EN_Port = NULL;
  ModbusTCPs.u16regs = ModbusDATA_TCPs;
  ModbusTCPs.u16regsize = sizeof(ModbusDATA_TCPs)/sizeof(ModbusDATA_TCPs[0]);
  // TCP hardware
  ModbusTCPs.xTypeHW = TCP_HW;
  ModbusTCPs.uTcpPort = 502;
  ModbusInit(&ModbusTCPs);
  // Start capturing traffic on serial Port and initialize counters
  ModbusStart(&ModbusTCPs);
}

/**
 * @brief Watchdog  
 *
 */
static void wdi_thread(void *argument) {
  (void) argument;
#ifdef DEBUG
  wd_tick = 0;
#endif
  while (1) {
#ifdef DEBUG
    wd_tick++;
#endif
    osDelay(2000);
    HAL_GPIO_TogglePin(WDI_GPIO_Port, WDI_Pin);
  }
}

/* USER CODE END Application */
