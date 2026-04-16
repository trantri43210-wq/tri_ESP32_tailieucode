/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdlib.h"
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float integral;      // Thành phần tích phân tích lũy
    float prevError;     // Sai số lần trước
    float outMin;        // Giới hạn đầu ra Min
    float outMax;        // Giới hạn đầu ra Max
    float dt;            // Thời gian lấy mẫu (giây)
} PID_Controller;

/*typedef struct __attribute__((packed)) {
    uint8_t header;       // 0xAA
    
    float setpoint_pos;   // 1
    int32_t current_pos;  // 2
    float target_vel;     // 3
    float current_vel;    // 4
    float rpm_motor;      // 5
    float rpm_output;     // 6
    
    float v_Kp;           // 7
    float v_Ki;           // 8
    float v_Kd;           // 9
    
    float p_Kp;           // 10
    float p_Ki;           // 11
    float p_Kd;           // 12
    
    uint8_t checksum;
    uint8_t footer;       // 0x55
} TelemetryData; */

typedef struct __attribute__((packed)) {
    uint8_t header;       // 0xAA
    
    float setpoint_pos;   // 1
    int32_t current_pos;  // 2
    float target_vel;     // 3
    float current_vel;    // 4
    float rpm_motor;      // 5
    float rpm_output;     // 6
    
    float v_Kp;           // 7
    float v_Ki;           // 8
    float v_Kd;           // 9
    
    float p_Kp;           // 10
    float p_Ki;           // 11
    float p_Kd;           // 12
    
    float pwm_out;        // 13
    float current_ma;     // 14 <--- BẮT BUỘC PHẢI CÓ DÒNG NÀY
    
    uint8_t checksum;
    uint8_t footer;       // 0x55
} TelemetryData;

typedef struct __attribute__((packed)) {
    uint8_t header;    // 0xBB
    uint8_t cmd_id;    // 1: Setpoint, 2: Kp, 3: Ki, 4: Kd, 5: Enable/Disable
    float value;       // Giá trị lệnh
    uint8_t checksum;
    uint8_t footer;    // 0xCC
} ControlPacket;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PWM_MAX 4199    
#define INPUT_MAX 1000

// Thông số vật lý motor JGB37-520
#define PPR_MOTOR 11            
#define ENCODER_MODE 4          
#define GEAR_RATIO 30.0f        
#define PULSES_PER_REV_MOTOR (PPR_MOTOR * ENCODER_MODE) // 44 xung/vòng

// Ngưỡng Fuzzy (Dải 13200 xung như bạn yêu cầu)
#define E_NB -13200.0f
#define E_NS -1000.0f
#define E_ZE 0.0f
#define E_PS 1000.0f
#define E_PB 13200.0f

#define DE_NB -5000.0f
#define DE_NS -1000.0f
#define DE_ZE 0.0f
#define DE_PS 1000.0f
#define DE_PB 5000.0f

// Biên độ điều chỉnh Fuzzy cho nghiên cứu
#define MAX_D_KP 2.5f   
#define MAX_D_KI 0.5f   
#define MAX_D_KD 0.3f  

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim10;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
volatile int32_t global_pulse_count = 0; // Vị trí tuyệt đối (32-bit tránh tràn)
uint16_t last_tim3_cnt = 0;

float vong = 5;

// 2. Các đối tượng PID
PID_Controller pid_pos; // PID Vị trí
PID_Controller pid_vel; // PID Vận tốc

// 3. Biến trạng thái hệ thống
volatile float setpoint_pos = 0;    // Vị trí mong muốn (xung)
volatile float current_vel = 0;     // Vận tốc hiện tại (xung/s)
volatile float target_vel = 0;      // Vận tốc mong muốn (output của vòng pos)
volatile float motor_pwm_out = 0;   // PWM xuất ra động cơ

// 4. Biến lọc nhiễu vận tốc (Low Pass Filter)
float vel_filter = 0;
float alpha = 0.05f; // Hệ số lọc (0.0 - 1.0), càng nhỏ lọc càng mạnh nhưng trễ cao

// 5. Cờ bật/tắt động cơ
uint8_t motor_enable = 0; 

// Biến lưu tốc độ hiển thị
volatile float rpm_motor_shaft = 0;   // Vận tốc trục động cơ (trước hộp số) - RPM
volatile float rpm_output_shaft = 0;  // Vận tốc trục ra (sau hộp số) - RPM

float current_Kp, current_Ki, current_Kd; 

float target_Kp = 7.5f, target_Ki = 0.0f, target_Kd = 0.2f;
float smooth_alpha = 0.15f; // Hệ số làm mịn (0.05 - 0.2 là đẹp)
//////////////////////////////////////////////////////////////////////

TelemetryData txData;
uint32_t last_uart_send = 0; // Biến quản lý thời gian gửi

float b_kp = 7.5f; 
float b_ki = 0.0f; 
float b_kd = 0.2f;

// Thêm biến phục vụ nhận lệnh từ ESP32
//ControlPacket rxPacket;
//uint8_t rxBuf[sizeof(ControlPacket)]; // sua ngay 3/3
// --- MÁY TRẠNG THÁI NHẬN UART CHỐNG TRƯỢT KHUNG ---
uint8_t rx_byte;                           // Biến nhận từng byte
uint8_t rx_buffer[sizeof(ControlPacket)];  // Mảng lưu tạm gói tin
uint8_t rx_index = 0;                      // Con trỏ mảng
typedef enum { WAIT_HEADER, WAIT_PAYLOAD } RX_State;
RX_State rx_state = WAIT_HEADER;
///////////////////////////////////////////////////////

// 1. Đối tượng PID cho vòng thứ 3 (Dòng điện)
PID_Controller pid_curr; 

// 2. Biến trạng thái dòng điện
volatile float current_ma = 0;   // Dòng điện thực tế (mA)
volatile float target_curr = 0;  // Dòng điện mục tiêu từ vòng vận tốc (mA)

// 3. Thông số INA240A2 (Dựa trên thông số bạn đã đo)
const float V_DDA = 3.36f;       
const float ADC_RES = 4095.0f;
const float SENSITIVITY = 5.0f;  // 0.1 Ohm * Gain 50 = 5V/A
const float V_OFFSET = 1.68f;    // Điểm 0 Ampe bạn đo được
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM10_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void PID_Init(PID_Controller *pid, float kp, float ki, float kd, float min, float max, float dt) {
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->outMin = min;
    pid->outMax = max;
    pid->dt = dt;
    pid->integral = 0;
    pid->prevError = 0;
}

// Hàm tính toán PID
float PID_Compute(PID_Controller *pid, float setpoint, float measurement) {
    float error = setpoint - measurement;

    // 1. Khâu P
    float P = pid->Kp * error;

    // 2. Khâu I (Tích phân)
    pid->integral += error * pid->dt;
    
    // Anti-windup cho khâu I (Ngắt tích phân khi quá giới hạn)
    // Cách tối ưu: Kẹp giá trị I riêng biệt
    float I = pid->Ki * pid->integral;
    // (Optional: Clamp I term here if needed, but output clamping usually suffices)

    // 3. Khâu D (Đạo hàm)
    float derivative = (error - pid->prevError) / pid->dt;
    float D = pid->Kd * derivative;

    // Tổng hợp
    float output = P + I + D;

    // Lưu sai số cho lần sau
    pid->prevError = error;

    // Bão hòa đầu ra (Saturation) & Anti-windup feedback
    if (output > pid->outMax) {
        output = pid->outMax;
        // Chống tăng tích phân khi đã bão hòa (quan trọng!)
        if (error > 0) pid->integral -= error * pid->dt; 
    } else if (output < pid->outMin) {
        output = pid->outMin;
        if (error < 0) pid->integral -= error * pid->dt;
    }

    return output;
}

// Giữ nguyên hàm Motor_Control của bạn
void Motor_Control(int16_t input_val) 
{
    if (input_val > INPUT_MAX)  input_val = INPUT_MAX;
    if (input_val < -INPUT_MAX) input_val = -INPUT_MAX;

    uint32_t pulse = (uint32_t)((abs(input_val) * (float)PWM_MAX) / (float)INPUT_MAX);

    if (input_val < 0) 
    {
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);
    } 
    else if (input_val > 0) 
    {
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse);
    }
    else 
    {
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);
    }
}


float trimf(float x, float a, float b, float c) {
    if (x <= a || x >= c) return 0.0f;
    if (x == b) return 1.0f;
    if (x > a && x < b) return (x - a) / (b - a);
    return (c - x) / (c - b);
}

// Ma trận quy tắc cho Kp, Ki, Kd
/*const int8_t Rule_Kp[5][5] = {
    {2, 2, 2, 1, 0}, {2, 2, 1, 0, -1}, {1, 1, 0, -1, -1}, {-1, 0, 1, 2, 2}, {0, 1, 2, 2, 2}
};
const int8_t Rule_Ki[5][5] = {
    {-2, -2, -2, -2, -2}, {-1, 0, 1, 0, -1}, {-1, 1, 2, 1, -1}, {-1, 0, 1, 0, -1}, {-2, -2, -2, -2, -2}
};  */

const int8_t Rule_Kp[5][5] = {
    { 2,  2,  2,  1,  0},
    { 2,  2,  1,  1,  0}, // Đẩy NS lên 1 thay vì 0
    { 1,  1,  0,  1,  1}, // Tại ZE, nếu có de thì vẫn tăng Kp để chống lệch
    { 0,  1,  1,  2,  2}, 
    { 0,  1,  2,  2,  2}
};

// Ma trận Ki mới: Giúp bù tải nhanh hơn
const int8_t Rule_Ki[5][5] = {
    { 0,  0,  0,  0,  0},
    { 0,  1,  2,  1,  0}, // Tăng Ki mạnh hơn khi chớm lệch
    { 0,  2,  2,  2,  0}, // Tại ZE, Ki phải rất sẵn sàng
    { 0,  1,  2,  1,  0},
    { 0,  0,  0,  0,  0}
};
const int8_t Rule_Kd[5][5] = {
    {-2, -1, 0, 1, 2}, {-2, -1, 0, 1, 1}, {-2, -1, 0, 1, 1}, {-2, -1, 0, 1, 1}, {-2, -1, 0, 1, 2}
};

void Fuzzy_Tuning(float e, float de, float *dkp, float *dki, float *dkd) {
    float mu_e[5], mu_de[5];
    // Mờ hóa Error
    mu_e[0] = trimf(e, -30000, E_NB, E_NS); mu_e[1] = trimf(e, E_NB, E_NS, E_ZE);
    mu_e[2] = trimf(e, E_NS, E_ZE, E_PS);   mu_e[3] = trimf(e, E_ZE, E_PS, E_PB);
    mu_e[4] = trimf(e, E_PS, E_PB, 30000);
    // Mờ hóa Delta Error
    mu_de[0] = trimf(de, -10000, DE_NB, DE_NS); mu_de[1] = trimf(de, DE_NB, DE_NS, DE_ZE);
    mu_de[2] = trimf(de, DE_NS, DE_ZE, DE_PS);   mu_de[3] = trimf(de, DE_ZE, DE_PS, DE_PB);
    mu_de[4] = trimf(de, DE_PS, DE_PB, 10000);

    float sum_w = 0, s_kp = 0, s_ki = 0, s_kd = 0;
    for(int i=0; i<5; i++) {
        for(int j=0; j<5; j++) {
            float w = mu_e[i] * mu_de[j];
            if(w > 0) {
                sum_w += w;
                s_kp += w * (Rule_Kp[i][j] * (MAX_D_KP/2.0f));
                s_ki += w * (Rule_Ki[i][j] * (MAX_D_KI/2.0f));
                s_kd += w * (Rule_Kd[i][j] * (MAX_D_KD/2.0f));
            }
        }
    }
    if(sum_w > 0) { *dkp = s_kp/sum_w; *dki = s_ki/sum_w; *dkd = s_kd/sum_w; }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_TIM10_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
	
txData.header = 0xAA;
txData.footer = 0x55;

HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
HAL_TIM_Base_Start_IT(&htim10);
HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
HAL_ADC_Start(&hadc1); // Khởi động ADC

 PID_Init(&pid_curr, 0.7f, 20.0f, 0.01f, -1000.0f, 1000.0f, 0.001f);

// Vòng 2 (Vận tốc): Giới hạn dòng điện mục tiêu ở +/- 500mA (An toàn cho motor và ADC)
PID_Init(&pid_vel, 0.5f, 10.0f, 0.0f, -500.0f, 500.0f, 0.001f); 

// Vòng 1 (Vị trí): Giữ nguyên
PID_Init(&pid_pos, 7.5f, 0.0f, 0.2f, -1800.0f, 1800.0f, 0.001f);

   motor_enable = 1;
  setpoint_pos = vong*1320; 
	
	//HAL_UART_Receive_IT(&huart1, rxBuf, sizeof(ControlPacket));   sua ngay3/3
	// Bắt đầu lắng nghe UART từng byte một
   HAL_UART_Receive_IT(&huart1, &rx_byte, 1);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		
/*if (HAL_GetTick() - last_uart_send >= 50) 
    {
        last_uart_send = HAL_GetTick();

        // Gán dữ liệu
        txData.setpoint_pos = setpoint_pos;
        txData.current_pos  = global_pulse_count;
        txData.target_vel   = target_vel;
        txData.current_vel  = current_vel;
        txData.rpm_motor    = rpm_motor_shaft;
        txData.rpm_output   = rpm_output_shaft;
        txData.current_ma   = current_ma; // <--- Cập nhật dòng điện thực tế
        
        txData.v_Kp = pid_vel.Kp; txData.v_Ki = pid_vel.Ki; txData.v_Kd = pid_vel.Kd;
        txData.p_Kp = pid_pos.Kp; txData.p_Ki = pid_pos.Ki; txData.p_Kd = pid_pos.Kd;
        txData.pwm_out = motor_pwm_out;

        // Tính Checksum
        uint8_t sum = 0;
        uint8_t *ptr = (uint8_t*)&txData;
        for (int i = 1; i < sizeof(TelemetryData) - 2; i++) {
            sum += ptr[i];
        }
        txData.checksum = sum;

        // Gửi qua DMA
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&txData, sizeof(TelemetryData));
    } */
		
		
		// Rút ngắn thời gian xuống 20ms (Tốc độ 50Hz) để đồ thị Real-time mượt nhất
    if (HAL_GetTick() - last_uart_send >= 20) 
    {
        last_uart_send = HAL_GetTick();

        // KIỂM TRA DMA CÓ ĐANG RẢNH KHÔNG TRƯỚC KHI GỬI (Chống lỗi HAL_BUSY)
        if (huart1.gState == HAL_UART_STATE_READY) 
        {
            // Tạm tắt ngắt để copy dữ liệu (Chống lệch data do ngắt Timer chen ngang)
            __disable_irq();
            txData.setpoint_pos = setpoint_pos;
            txData.current_pos  = global_pulse_count;
            txData.target_vel   = target_vel;
            txData.current_vel  = current_vel;
            txData.rpm_motor    = rpm_motor_shaft;
            txData.rpm_output   = rpm_output_shaft;
            txData.current_ma   = current_ma; 
            
            txData.v_Kp = pid_vel.Kp; txData.v_Ki = pid_vel.Ki; txData.v_Kd = pid_vel.Kd;
            txData.p_Kp = pid_pos.Kp; txData.p_Ki = pid_pos.Ki; txData.p_Kd = pid_pos.Kd;
            txData.pwm_out = motor_pwm_out;
            __enable_irq(); // Mở lại ngắt

            // Tính Checksum
            uint8_t sum = 0;
            uint8_t *ptr = (uint8_t*)&txData;
            for (int i = 1; i < sizeof(TelemetryData) - 2; i++) {
                sum += ptr[i];
            }
            txData.checksum = sum;

            // Gửi qua DMA
            HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&txData, sizeof(TelemetryData));
        }
    }
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	//	Motor_Control(200);
		   // current_ma = Read_Current_Smooth(100);

		
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 4199;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 83;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 999;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/*void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        ControlPacket *p = (ControlPacket*)rxBuf;
        if (p->header == 0xBB && p->footer == 0xCC) {
            switch (p->cmd_id) {
                case 1: setpoint_pos = p->value; break;
                case 2: b_kp = p->value; break;
                case 3: b_ki = p->value; break;
                case 4: b_kd = p->value; break;
                case 5: motor_enable = (uint8_t)p->value; 
                        if(motor_enable == 0) Motor_Control(0); break;
            }
        }
        HAL_UART_Receive_IT(&huart1, rxBuf, sizeof(ControlPacket));
    }
}  */


/* USER CODE BEGIN 4 */
/*void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        ControlPacket *p = (ControlPacket*)rxBuf;
        if (p->header == 0xBB && p->footer == 0xCC) {
            // Nháy LED PC13 để báo hiệu nhận lệnh thành công
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); 
            
            switch (p->cmd_id) {
                case 1: setpoint_pos = p->value; break;
                case 5: 
                    motor_enable = (uint8_t)p->value;
                    if(motor_enable == 0) Motor_Control(0);
                    break;
            }
        }
        // Xóa các cờ lỗi nếu có trước khi nhận gói tiếp theo
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UART_Receive_IT(&huart1, rxBuf, sizeof(ControlPacket));
    }
}



// 2. Hàm xử lý lỗi UART (Cực kỳ quan trọng)
// Nếu không có hàm này, chỉ cần 1 byte lỗi là STM32 sẽ "điếc" luôn
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        __HAL_UART_CLEAR_OREFLAG(huart); // Xóa lỗi tràn bộ đệm
        __HAL_UART_CLEAR_NEFLAG(huart);  // Xóa lỗi nhiễu
        __HAL_UART_CLEAR_FEFLAG(huart);  // Xóa lỗi khung hình
        
        // Khởi động lại bộ nhận
        HAL_UART_Receive_IT(&huart1, rxBuf, sizeof(ControlPacket));
    }
}  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        
        switch (rx_state) {
            case WAIT_HEADER:
                if (rx_byte == 0xBB) {
                    rx_buffer[0] = rx_byte;
                    rx_index = 1;
                    rx_state = WAIT_PAYLOAD;
                }
                break;
                
            case WAIT_PAYLOAD:
                rx_buffer[rx_index++] = rx_byte;
                
                // Nếu đã nhận đủ chiều dài gói tin
                if (rx_index >= sizeof(ControlPacket)) {
                    ControlPacket *p = (ControlPacket*)rx_buffer;
                    
                    if (p->footer == 0xCC) { // Check Footer
                        // Check Checksum
                        uint8_t calc_sum = 0;
                        for (int i = 1; i < sizeof(ControlPacket) - 2; i++) {
                            calc_sum += rx_buffer[i];
                        }
                        
                        if (calc_sum == p->checksum) {
                            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); 
                            
                            switch (p->cmd_id) {
                                case 1: setpoint_pos = p->value; break;
                                case 5: 
                                    motor_enable = (uint8_t)p->value;
                                    if(motor_enable == 0) Motor_Control(0);
                                    break;
                            }
                        }
                    }
                    rx_state = WAIT_HEADER; // Xong 1 gói, quay lại chờ gói mới
                }
                break;
        }
        
        // Tiếp tục gọi ngắt nhận 1 byte tiếp theo
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        __HAL_UART_CLEAR_OREFLAG(huart); 
        __HAL_UART_CLEAR_NEFLAG(huart);  
        __HAL_UART_CLEAR_FEFLAG(huart);  
        
        // Bị nhiễu lỗi -> Xóa state machine và nghe lại từ đầu
        rx_state = WAIT_HEADER;
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}     


uint8_t fuzzy_count = 0;
//float b_kp = 7.5f, b_ki = 0.0f, b_kd = 0.2f; // Hệ số gốc bạn đã test ổn

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) 
    {
        // 1. ĐỌC PHẢN HỒI
        uint16_t curr_cnt = __HAL_TIM_GET_COUNTER(&htim3);
        int16_t delta_cnt = (int16_t)(curr_cnt - last_tim3_cnt);
        last_tim3_cnt = curr_cnt;
        global_pulse_count += delta_cnt;
        current_vel = (alpha * ((float)delta_cnt / 0.001f)) + (1.0f - alpha) * current_vel;

        // Đọc dòng điện - FIX: Poll & Đảo dấu
        HAL_ADC_Start(&hadc1);
        uint32_t wait = 1000;
        while(!(hadc1.Instance->SR & ADC_SR_EOC) && --wait);
        float v_out = ((float)hadc1.Instance->DR * V_DDA) / ADC_RES;
        float c_raw = -((v_out - V_OFFSET) / SENSITIVITY * 1000.0f); // ĐẢO DẤU TẠI ĐÂY
        current_ma = (0.1f * c_raw) + (0.9f * current_ma); // Lọc mạnh cho mát motor

        if (motor_enable) 
        {
            float err = setpoint_pos - (float)global_pulse_count;

            // 2. FUZZY & SMOOTHING (Giữ nguyên logic của bạn)
            if (++fuzzy_count >= 10) {
                float dkp=0, dki=0, dkd=0;
                Fuzzy_Tuning(err, -current_vel, &dkp, &dki, &dkd);
                target_Kp = b_kp + dkp; target_Kd = b_kd + dkd;
                target_Ki = (fabs(err) < 800) ? (b_ki + dki) : 0;
                fuzzy_count = 0;
            }
            pid_pos.Kp = (0.1f * target_Kp) + 0.9f * pid_pos.Kp;
            pid_pos.Ki = (0.1f * target_Ki) + 0.9f * pid_pos.Ki;
            pid_pos.Kd = (0.1f * target_Kd) + 0.9f * pid_pos.Kd;

            // 3. CASCADE 3 VÒNG
            if (fabs(err) < 3.0f) { // Tăng deadzone lên 3 cho ổn định
                pid_pos.integral = 0; pid_vel.integral = 0; pid_curr.integral = 0;
                motor_pwm_out = 0;
                Motor_Control(0); 
            }
            else {
                target_vel = PID_Compute(&pid_pos, setpoint_pos, (float)global_pulse_count);
                target_curr = PID_Compute(&pid_vel, target_vel, current_vel);
                motor_pwm_out = PID_Compute(&pid_curr, target_curr, current_ma);
                Motor_Control((int16_t)motor_pwm_out);
            }
        }
        else {
            pid_pos.integral = 0; pid_vel.integral = 0; pid_curr.integral = 0;
            Motor_Control(0);
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
