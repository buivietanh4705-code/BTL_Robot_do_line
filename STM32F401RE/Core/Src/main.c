/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Mã nguồn hoàn chỉnh Xe Dò Line PID - Giao tiếp MQTT
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
uint8_t rx_byte;
char rx_buffer[150];
int rx_index = 0;

volatile uint8_t data_ready = 0; // Cờ báo hiệu đã nhận xong JSON

volatile int robot_mode = 1;     // 1: Auto (Dò line), 0: Manual
volatile char robot_dir = 'S';

float error = 0;
float previous_error = 0;
float P = 0, I = 0, D = 0, PID_value = 0;

float Kp = 100.0; // Giảm Kp ban đầu để test an toàn
float Ki = 0.0;
float Kd = 100;
int base_speed = 180; // Tốc độ nền vừa phải
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART6_UART_Init(void);

/* USER CODE BEGIN PFP */
float get_line_error(void);
void set_motor_speed(int speed_left, int speed_right);
void calculate_pid(void);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USART6_UART_Init();

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

  // Kích hoạt ngắt nhận UART ký tự đầu tiên
  HAL_UART_Receive_IT(&huart6, &rx_byte, 1);

  HAL_Delay(2000);

  while (1)
  {
    // 1. CHỐNG TREO UART
    __HAL_UART_CLEAR_OREFLAG(&huart6);

    // 2. XỬ LÝ CHUỖI JSON
    if (data_ready == 1) {
            char *ptr;

            // Kiểm tra nếu chuỗi chứa các thông số cấu hình
            if (strstr(rx_buffer, "\"Kp\"")) {
                if ((ptr = strstr(rx_buffer, "\"Kp\":")) != NULL) Kp = atof(ptr + 5);
                if ((ptr = strstr(rx_buffer, "\"Ki\":")) != NULL) Ki = atof(ptr + 5);
                if ((ptr = strstr(rx_buffer, "\"Kd\":")) != NULL) Kd = atof(ptr + 5);

                // THÊM DÒNG NÀY: Tách giá trị Base Speed
                if ((ptr = strstr(rx_buffer, "\"Base\":")) != NULL) base_speed = atoi(ptr + 7);
            }
            // Các xử lý mode và dir giữ nguyên...
            else if (strstr(rx_buffer, "\"mode\"")) {
                if ((ptr = strstr(rx_buffer, "\"mode\":")) != NULL) robot_mode = atoi(ptr + 7);
                if ((ptr = strstr(rx_buffer, "\"dir\":\"")) != NULL) robot_dir = *(ptr + 7);
            }

            rx_index = 0;
            data_ready = 0;
        }

    // 3. ĐIỀU KHIỂN ĐỘNG CƠ
    // 3. ĐIỀU KHIỂN ĐỘNG CƠ
        if (robot_mode == 1) {
            // --- CHẾ ĐỘ AUTO (DÒ LINE) ---
            error = get_line_error();

            if (error != 99) {
                // ==========================================
                // TRƯỜNG HỢP 1: XE ĐANG NẰM TRÊN VẠCH
                // ==========================================
                calculate_pid();

                int left_motor_speed = base_speed - PID_value;
                int right_motor_speed = base_speed + PID_value;

                // Khống chế bão hòa PWM
                if (left_motor_speed > 1000) left_motor_speed = 1000;
                if (left_motor_speed < -1000) left_motor_speed = -1000;
                if (right_motor_speed > 1000) right_motor_speed = 1000;
                if (right_motor_speed < -1000) right_motor_speed = -1000;

                set_motor_speed(left_motor_speed, right_motor_speed);
            }
            else {
                // ==========================================
                // TRƯỜNG HỢP 2: XE BỊ VĂNG KHỎI VẠCH (error == 99)
                // ÁP DỤNG CƠ CHẾ "NHỚ" ĐỂ TÌM LẠI LINE
                // ==========================================
                int pivot_speed = 150; // Tốc độ cấp cho động cơ để xoay xe tại chỗ

                if (previous_error < -0.5) {
                    // Trước khi mất, vạch ở bên TRÁI -> Khóa bánh, xoay tại chỗ sang TRÁI
                    set_motor_speed(-pivot_speed, pivot_speed);
                }
                else if (previous_error > 0.5) {
                    // Trước khi mất, vạch ở bên PHẢI -> Khóa bánh, xoay tại chỗ sang PHẢI
                    set_motor_speed(pivot_speed, -pivot_speed);
                }
                else {
                    // Nếu mất line khi đang đi thẳng (có thể do vạch đứt quãng) -> Đi thẳng chậm lại
                    set_motor_speed(pivot_speed / 2, pivot_speed / 2);
                }
            }
        } else {
            // --- CHẾ ĐỘ MANUAL (LÁI TAY) ---
            // (Đoạn này giữ nguyên như cũ)
            int manual_speed = 400;
            if (robot_dir == 'F')      set_motor_speed(manual_speed, manual_speed);
            else if (robot_dir == 'B') set_motor_speed(-manual_speed, -manual_speed);
            else if (robot_dir == 'L') set_motor_speed(-manual_speed, manual_speed);
            else if (robot_dir == 'R') set_motor_speed(manual_speed, -manual_speed);
            else                       set_motor_speed(0, 0);
        }

    HAL_Delay(10);
  }
}

/* --- CÁC HÀM XỬ LÝ CON --- */

// QUAN TRỌNG: Đã thêm lại dấu (!) để lấy được tín hiệu mức 1 từ vạch đen
float get_line_error(void) {
    uint8_t s[5];
    s[0] = !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    s[1] = !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    s[2] = !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
    s[3] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
    s[4] = !HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1);

    int sum = s[0] + s[1] + s[2] + s[3] + s[4];

    if (sum == 0) return 99;

    float current_error = (s[0]*(-2.0) + s[1]*(-1.0) + s[2]*(0.0) + s[3]*(1.0) + s[4]*(2.0)) / sum;
    return current_error;
}

void set_motor_speed(int speed_left, int speed_right) {
    if (speed_left >= 0) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, speed_left);
    } else {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, -speed_left);
    }

    if (speed_right >= 0) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, speed_right);
    } else {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, -speed_right);
    }
}

void calculate_pid(void) {
    P = error;
    I = I + error;
    D = error - previous_error;
    PID_value = (Kp * P) + (Ki * I) + (Kd * D);
    previous_error = error;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        if (rx_byte == '\n' || rx_byte == '\r') {
            if(rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                data_ready = 1;
            }
        }
        else {
            if (rx_index < 149 && data_ready == 0) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
    }
}

/* -------------------------------------------------------------------------
   CÁC HÀM INIT PHẦN CỨNG TỰ ĐỘNG SINH BỞI CUBEMX
   ------------------------------------------------------------------------- */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
}

static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) { Error_Handler(); }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) { Error_Handler(); }

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) { Error_Handler(); }

  HAL_TIM_MspPostInit(&htim1);
}

static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) { Error_Handler(); }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) { Error_Handler(); }

  HAL_TIM_MspPostInit(&htim2);
}

static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK) { Error_Handler(); }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_4; // IR_R2_Pin
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5; // L298N IN1-IN4
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1; // IR_L2, IR_L1
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0; // IR_MID
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1; // IR_R1
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
