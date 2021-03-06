/*
 * hardware_init.c
 *
 *  Created on: 13/09/2013
 *      Author: alan
 */

#include "quadrotor.h"
#include "DebugConsole.h"
#include "board.h"
#include "types.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "mavlink.h"
#include "mavlink_bridge.h"

#include "qUART.h"
#include "qI2C.h"
#include "lpc17xx_gpio.h"

#include "leds.h"
#include "bmp085.h"
#include "HMC5883L.h"
#include "eMPL/inv_mpu.h"
#include "eMPL/inv_mpu_dmp_motion_driver.h"
#include "ultrasonic_sensor.h"
#include "math.h"
#include "qESC.h"
#include "qAnalog.h"

//TODO: Sacar esto de aca!
void DataCollection(void * p);
void Distance(void *);
void Telemetry(void *);
void beacon(void *p);

xTaskHandle beacon_hnd;


void hardware_init(void * p){
   uint16_t i;

   // =========================================================
   // Early init will turn the motors off for safety if a reset occurs
   qESC_Init();
   qESC_InitChannel(MOTOR1);
   qESC_InitChannel(MOTOR2);
   qESC_InitChannel(MOTOR3);
   qESC_InitChannel(MOTOR4);

   vTaskDelay(5000/portTICK_RATE_MS);
   //cuidado con la velocidad, antes estaba en 57600
   if (qUART_Init(UART_GROUNDCOMM,57600,8,QUART_PARITY_NONE,1)!=RET_OK){
      halt();
   }
   qUART_EnableTx(UART_GROUNDCOMM);

   debug("Initializing hardware \r\n");

   for (i=0;i<TOTAL_LEDS;i++){
      qLed_Init(leds[i]);
      qLed_TurnOn(leds[i]);
   }

   debug("Initializing I2C interface...");
   if (qI2C_Init()!=SUCCESS){
      debug("[ERROR]\r\n");
      halt();
   }
   debug("[OK]\r\n");

   debug("Downloading MPU6050 eMPL firmware...");
   if (mpu_init(NULL)==0){
      debug("[OK]\r\n");
   }else{
      debug("[ERROR]\r\n");
      halt();
   }

   debug("Configuring MPU6050 IMU...");
   /* Get/set hardware configuration. Start gyro. */
   /* Wake up all sensors. */
   mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
   /* Push both gyro and accel data into the FIFO. */
   mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);
   //********************************************************************************************************************************************
   //                                          SETEO DE FRECUENCIA DE MUESTREO!!
   //********************************************************************************************************************************************
   mpu_set_sample_rate(200);

   //********************************************************************************************************************************************
   dmp_load_motion_driver_firmware();
   mpu_set_gyro_fsr(2000);
   mpu_set_accel_fsr(2);
   mpu_set_lpf(150);
   dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL);
   dmp_set_fifo_rate(200);
   dmp_enable_gyro_cal(1);
   mpu_set_dmp_state(1);


   // TODO: estaria bueno que esto este adentro del driver (por ahi)
   // GPIO0.4 as input with interrupt
   GPIO_SetDir(0,(1<<4),0);
   GPIO_IntCmd(0,(1<<4),1);
   GPIO_ClearInt(0,(1<<4));
   NVIC_SetPriority(EINT3_IRQn, 6);
   NVIC_DisableIRQ(EINT3_IRQn);

   debug("[OK]\r\n");

#if 0
   //===========================================================
   // I2C Scan

   uint8_t address;
   debug("Starting I2C scan...\r\n");
   address = 0;
   do {
      if (qI2C_Write(address,NULL,0x00,0)==SUCCESS){
         //debug("0x");
         //debug_fmt("0x%x",address);
         debug(" Address found\r\n");
      }
   } while (address++ != 255);
   debug("I2C scan finished\r\n");
#endif

   BMP085_Init();
   RangeFinder_Init();

   //=========================================================================
   quadrotor.mavlink_system.state = MAV_STATE_CALIBRATING;

   quadrotor.sv.floor_pressure = 0.0;
/*
   for (i=0;i<100;i++){
      BMP085_GetTemperature();
      quadrotor.sv.floor_pressure += BMP085_GetPressure()/100.0;
      vTaskDelay(10/portTICK_RATE_MS);
   }
*/
   for (i=0;i<3;i++){
      quadrotor.rateController[i].AntiWindup = ENABLED;
      quadrotor.rateController[i].AntiWindup_fb = DISABLE;
      quadrotor.rateController[i].Bumpless = DISABLED;
      quadrotor.rateController[i].Mode = AUTOMATIC;
      quadrotor.rateController[i].OutputMax = 1.0;
      quadrotor.rateController[i].OutputMin = -1.0;
      quadrotor.rateController[i].Ts = 0.005;
      quadrotor.rateController[i].b = 1.0;
      quadrotor.rateController[i].c = 1.0;
      qPID_Init(&quadrotor.rateController[i]);
      //----------------------------------------
      //Identification init
      //----------------------------------------
      // Y(k) + a1Y(k-1) + ......anY(k-n)=bU(k-1) + ......blU(k-l) ===> Y$=[-Y(k-1)....-Y(k-n) U(k-1)....U(k-l)]*[a1...an b....bl]'

      quadrotor.id_rate[i].n=1; // n= orden de Y
      quadrotor.id_rate[i].l=1; // l= orden de U
      quadrotor.id_atti[i].n=3;
      quadrotor.id_atti[i].l=2;
      quadrotor.id_rate[i].k=0;
      quadrotor.id_atti[i].k=0;
      quadrotor.id_rate[i].FO=0.95;
      quadrotor.id_atti[i].FO=0.95;
      ident_Init(&quadrotor.id_rate[i]);
      //-----------------------------------------
   }

   for (i=0;i<3;i++){
      quadrotor.attiController[i].AntiWindup = ENABLED;
      quadrotor.attiController[i].AntiWindup_fb = DISABLE;         // Enable only one anti wind-up's
      quadrotor.attiController[i].Bumpless = ENABLED;
      quadrotor.attiController[i].Mode = AUTOMATIC;
      quadrotor.attiController[i].OutputMax = 250.0;
      quadrotor.attiController[i].OutputMin = -250.0;
      quadrotor.attiController[i].Ts = 0.005;
      quadrotor.attiController[i].b = 1.0;
      quadrotor.attiController[i].c = 1.0;
      qPID_Init(&quadrotor.attiController[i]);
   }


   quadrotor.altitudeController.Bumpless = ENABLED;
   quadrotor.altitudeController.Mode = MANUAL;//AUTOMATIC;
   quadrotor.altitudeController.OutputMax = 0.9;
   quadrotor.altitudeController.OutputMin = 0.0;
   quadrotor.altitudeController.Ts = 0.005;
   quadrotor.altitudeController.b = 1.0;
   quadrotor.altitudeController.c = 0.0;
   qPID_Init(&quadrotor.altitudeController);

   quadrotor.rateController[ROLL].K =0.005;
   quadrotor.rateController[ROLL].Ti =1/0.6;
   quadrotor.rateController[ROLL].Td = 0.0;
   quadrotor.rateController[ROLL].Nd = 5.0;
   quadrotor.rateController[ROLL].K_aw = 0.001;         // new anti wind-up gain

   quadrotor.rateController[PITCH].K =0.005;
   quadrotor.rateController[PITCH].Ti =1/0.6;
   quadrotor.rateController[PITCH].Td = 0.0;
   quadrotor.rateController[PITCH].Nd = 5.0;
   quadrotor.rateController[PITCH].K_aw = 0.001;         // new anti wind-up gain
   // --------------------------------------------------------
   quadrotor.attiController[ROLL].K = 3.5;//1.0;
   quadrotor.attiController[ROLL].Ti =1/0.008;
   quadrotor.attiController[ROLL].Td = 0.0;
   quadrotor.attiController[ROLL].Nd = 4.0;
   quadrotor.attiController[ROLL].K_aw = 0;            // new anti wind-up gain

   quadrotor.attiController[PITCH].K =3.5;//1.0;
   quadrotor.attiController[PITCH].Ti =1/0.008;
   quadrotor.attiController[PITCH].Td = 0.0;
   quadrotor.attiController[PITCH].Nd = 4.0;
   quadrotor.attiController[PITCH].K_aw = 0;            // new anti wind-up gain
   // --------------------------------------------------------
   quadrotor.rateController[YAW].K =0.06;
   quadrotor.rateController[YAW].Ti =1/0.2;//1/0.1;
   quadrotor.rateController[YAW].Td = 0.000;
   quadrotor.rateController[YAW].Nd = 5;
   quadrotor.rateController[YAW].K_aw = 0.001;            // new anti wind-up gain

   quadrotor.attiController[YAW].K = 2.7;//5;
   quadrotor.attiController[YAW].Ti = 1/0.005;//1/0.05;
   quadrotor.attiController[YAW].Td = 0.0;
   quadrotor.attiController[YAW].Nd = 4;
   quadrotor.attiController[YAW].K_aw = 0;               // new anti wind-up gain
   // --------------------------------------------------------
   quadrotor.altitudeController.K = 0.50;
   quadrotor.altitudeController.Ti = 1/1.50;
   quadrotor.altitudeController.Td = 0.000;
   quadrotor.altitudeController.Nd = 5;
   quadrotor.altitudeController.K_aw = 0.01;            // new anti wind-up gain
   //----------------------------------------------------------
   //       ENABLE/DISABLE CAS
   //-----------------------------------------------------------
   CAS=ENABLED;
   //------------------------------------------------------------
   //  Caracteristicas de los variadores:
   //------------------------------------------------------------
   //   Modo adaptacion de salidas
   //-----------------------------------------------------------
      speeds.Adapt_Mode = DISABLE;

   //pendientes de empuje:
   speeds.variador[0].Ke = 0.6932;
   speeds.variador[1].Ke = 0.6932;
   speeds.variador[2].Ke = 0.6932;
   speeds.variador[3].Ke = 0.6932;
   //ordenada al origen de empuje:
   speeds.variador[0].be= -68.88475;
   speeds.variador[1].be= -68.88475;
   speeds.variador[2].be= -68.88475;
   speeds.variador[3].be= -68.88475;
   //pendientes de torque:
   speeds.variador[0].Kt= 0.04645;
   speeds.variador[1].Kt= 0.04645;
   speeds.variador[2].Kt= 0.04645;
   speeds.variador[3].Kt= 0.04645;
   //ordenada al orÃ­gen de torque:
   speeds.variador[0].bt= 1.5345;
   speeds.variador[1].bt= 1.5345;
   speeds.variador[2].bt= 1.5345;
   speeds.variador[3].bt= 1.5345;
   // --------------------------------------------------------
   // PTOS
   // -------------------------------------------------------
   quadrotor.data_ptos.K_1=1;
   quadrotor.data_ptos.alpha=0.5;
   quadrotor.data_ptos.b_angle=1;
   quadrotor.data_ptos.sat=1;

   qAnalog_Init();
   qAnalog_InitPin(TEMPERATURE_ANALOG);
   qAnalog_InitPin(VOLTAGE_ANALOG);

/*
   qESC_SetOutput(MOTOR1,300);
   qESC_SetOutput(MOTOR1,0);

   qESC_SetOutput(MOTOR2,300);
   qESC_SetOutput(MOTOR2,0);

   qESC_SetOutput(MOTOR3,300);
   qESC_SetOutput(MOTOR3,0);

   qESC_SetOutput(MOTOR4,300);
   qESC_SetOutput(MOTOR4,0);
*/
   //=========================================================================
   //quadrotor.mavlink_system.state = MAV_STATE_STANDBY;

   quadrotor.mavlink_system.state = MAV_STATE_ACTIVE;
   quadrotor.mavlink_system.mode |= MAV_MODE_FLAG_SAFETY_ARMED;
   quadrotor.mode = ESC_STANDBY;
   xTaskCreate( Telemetry, "TLM", 300, NULL, tskIDLE_PRIORITY+1, NULL );
   xTaskCreate( DataCollection, "DATCOL", 500, NULL, tskIDLE_PRIORITY+2, NULL );
   xTaskCreate( beacon, "BEACON", 200, NULL, tskIDLE_PRIORITY+1, NULL);

   vTaskDelete(NULL);


}
