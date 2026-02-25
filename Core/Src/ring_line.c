#include "global_types.h"
#include "ktv.h"
#include "tim.h"
#include "cmsis_os2.h"
#include "ring_line.h"

const uint16_t buf_size = 320;

static void addr_module_thread(void *argument);

osThreadId_t addr_module_task_handle;

const osThreadAttr_t addr_module_task_handle_attr = {
  .name = "Thread address module",
  .stack_size = 128 * 6,
  .priority = (osPriority_t) osPriorityNormal1,
};

osMessageQueueId_t addr_msg_queue;

uint64_t opto1[5] = {0};
uint64_t opto2[5] = {0};

uint64_t opto3[5] = {0};
uint64_t opto4[5] = {0};

bool checkline = false;

//uint8_t line1_status = 2;
uint8_t line2_status = 2;

/**
 * @brief  
 *
 */
bool IsKbNorm() {
  if (line2_status >= 1) {
    return true;
  } else {
    return false;
  }
}

uint16_t count_bits_set_parallel(uint64_t x) {
  // put count of each 2 bits into those 2 bits
  x -= (x >> 1) & 0x5555555555555555UL;
  // put count of each 4 bits into those 4 bits
  x = (x & 0x3333333333333333UL) + ((x >> 2) & 0x3333333333333333UL);
  // put count of each 8 bits into those 8 bits
  x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fUL;
  // returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
  return (x * 0x0101010101010101UL) >> 56;
}

#ifdef FAST_TICK
extern uint64_t fast_tick;
uint64_t runtime[3] = {0};
#endif

/**
 * @brief  
 *
 */
void ring_lines_thread(void *argument) {
  (void) argument;
  
  uint16_t line_state1;
  uint16_t line_state2;
  
  uint16_t line_state3;
  uint16_t line_state4;

/*  
  uint8_t pos = 0;
  uint8_t ind = 0;
  
  uint64_t b = 1;
  uint64_t mask = 0;
*/
  
  // waiting for flag to be set
  osEventFlagsWait(event_flags, MODBUS_BIT, osFlagsWaitAny|osFlagsNoClear, osWaitForever);
  
  HAL_GPIO_WritePin(GPIOE, BDU1_M_S_Pin, GPIO_PIN_SET);
  osDelay(1000);
  HAL_GPIO_WritePin(GPIOE, BDU2_M_S_Pin, GPIO_PIN_SET);
  osDelay(1000);
  
  KTV_Init();
  // creation of Queue
  addr_msg_queue = osMessageQueueNew(5, sizeof(uint16_t), NULL);
  // creating a Task
  addr_module_task_handle = osThreadNew(addr_module_thread, NULL, &addr_module_task_handle_attr);
  //
  Start_IT_TIM12();
  
#ifdef FAST_TICK
  Start_IT_TIM11();
#endif
  
  while(1) {
    checkline = true;
    line_state1 = 0;
    line_state2 = 0;    
    line_state3 = 0;
    line_state4 = 0;
    
    while(checkline == true) {
      osDelay(10);
    }
    
#ifdef FAST_TICK    
    runtime[0] = fast_tick;
#endif
/*    
    for (uint16_t i = 0; i < buf_size; i++) {
      pos = i / 64;
      ind = i % 64;
      mask = (b << ind);
      line_state1 += (opto1[pos] & mask) ? 1 : 0;
      line_state2 += (opto2[pos] & mask) ? 1 : 0;
      
      line_state3 += (opto3[pos] & mask) ? 1 : 0;
      line_state4 += (opto4[pos] & mask) ? 1 : 0;
    }
*/    

    for (uint8_t i = 0; i < 5; i++) {
      line_state1 += count_bits_set_parallel(opto1[i]);
      line_state2 += count_bits_set_parallel(opto2[i]);
      line_state3 += count_bits_set_parallel(opto3[i]);
      line_state4 += count_bits_set_parallel(opto4[i]);
    }

#ifdef FAST_TICK
    runtime[1] = fast_tick;
    runtime[2] = runtime[1] - runtime[0];
#endif
    
    // ¯\_/¯\_/¯\  ¯\_/¯\_/¯\  ¯¯¯¯¯¯¯¯¯¯  ¯¯¯¯¯¯¯¯¯¯
    // ¯¯¯¯¯¯¯¯¯¯   /¯\_/¯\_/  ¯\_/¯\_/¯\  ¯¯¯¯¯¯¯¯¯¯
    
    // (40% - 60%) and >90%
    if ((line_state1 >= 128 && line_state1 <= 192) && line_state2 >= 288) {
      // diode
      //line1_status = 2;
      msg.line1_status = 2;
      osEventFlagsSet(event_flags, nRING_LINE_BIT);
      // suspend thread
      //osThreadYield();
    }
    // (40% - 60%) and >90%
    if ((line_state2 >= 128 && line_state2 <= 192) && line_state1 >= 288) {
      // diode
      //line1_status = 2;
      msg.line1_status = 2;
      osEventFlagsSet(event_flags, nRING_LINE_BIT);
      // suspend thread
      //osThreadYield();
    }
    
    // >90% and >90%
    if (line_state1 >= 288 && line_state2 >= 288) {
      // break
      //line1_status = 0;
      msg.line1_status = 0;
      osEventFlagsClear(event_flags, nRING_LINE_BIT);
      // suspend thread
      //osThreadYield();
    }
    
    // (40% - 60%) and (40% - 60%)
    if ((line_state1 >= 128 && line_state1 <= 192) && 
        (line_state2 >= 128 && line_state2 <= 192))
    {
      // closed
      //line1_status = 1;
      msg.line1_status = 1;
      osEventFlagsSet(event_flags, nRING_LINE_BIT);
      // suspend thread
      //osThreadYield();
    }
    
    // (40% - 60%) and >90%
    if ((line_state3 >= 128 && line_state3 <= 192) && line_state4 >= 288) {
      // diode
      line2_status = 2;
      msg.line2_status = 2;
    }
    // (40% - 60%) and >90%
    if ((line_state4 >= 128 && line_state4 <= 192) && line_state3 >= 288) {
      // diode
      line2_status = 2;
      msg.line2_status = 2;
    }
    // >90% and >90%
    if (line_state3 >= 288 && line_state4 >= 288) {
      // break
      line2_status = 0;
      msg.line2_status = 0;
    }
    // (40% - 60%) and (40% - 60%)
    if ((line_state3 >= 128 && line_state3 <= 192) &&
        (line_state4 >= 128 && line_state4 <= 192))
    {
      // closed
      line2_status = 1;
      msg.line2_status = 1;
    }
    
    if (osMessageQueueGetCount(addr_msg_queue) == 0 && line2_status == 0) {
      if (KTV_State() == ksEnd || KTV_State() == ksNoActive) {
        KTVmsg addr_module = kmStart;
        osMessageQueuePut(addr_msg_queue, &addr_module, 0U, 10U);
        // suspend thread
        //osThreadYield();
      }
    }
    
#ifdef DEBUG
    WM_Line = uxTaskGetStackHighWaterMark(NULL);
#endif
    
    osDelay(500);
  }  
}

/**
 * @brief  
 *
 */
static void addr_module_thread(void *argument) {
  (void) argument;
  KTVmsg addr_module;
  
  uint64_t gKtvStartTime = 0;
  uint64_t gKtvEndTime = 0;
  
  osStatus_t status;
  
  while (1) {
    status = osMessageQueueGet(addr_msg_queue, &addr_module, NULL, 10U);
    if (status != osOK) {
      addr_module = kmNone;
    }
    if (gKtvEndTime == 0) {
      if ((addr_module != kmNone) || (KTV_State() == ksStarted)) {
        gKtvStartTime = HAL_GetTick();
        gKtvEndTime = KTV_MAX_POLL_INTERVAL;
        gKtvEndTime += gKtvStartTime;
      }
    } else {
      if (HAL_GetTick() >= gKtvEndTime) {
        gKtvEndTime = 0;
      }
      osDelay(10);
      continue;
    }
    KTV_Process(addr_module);
    if (addr_module != kmNone) {
      addr_module = kmFinish;
    }
    
#ifdef DEBUG
    WM_KTV = uxTaskGetStackHighWaterMark(NULL);
#endif
    
    osDelay(10);
  }
}