/*
 * data_collection.c

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
#include "lpc17xx_gpio.h"

#include "math.h"
#include "bmp085.h"
#include "HMC5883L.h"
#include "eMPL/inv_mpu.h"
#include "eMPL/inv_mpu_dmp_motion_driver.h"
#include "ultrasonic_sensor.h"
#include "qESC.h"

#define PRESCALER_VALUE 10
xSemaphoreHandle mpuSempahore;
uint8_t prescaler = PRESCALER_VALUE;
float z_bias;

#define PI 3.14159265359

#define ATTI_THRESHOLD 3.0
float atti_bias[3];
float control[4]={0.0};
float Output[4]={0.0};
float AdaptedOutput[4]={0.0};
float Tk[2]={0.0}; //tiempo 1 y tiempo 2 ( para poder sacar el tiempo de muestreo real haciendo la resta entre los 2)

uint8_t MPU6050_dmpGetEuler(float *euler, int32_t q[]) {

   float q1[4];
   uint8_t i;

   for(i = 0; i < 4; i++ ) {
      q1[i] = ((float) (q[i]>>16)) / 16384.0f;
   }

   euler[0] = atan2(2*q1[1]*q1[2] - 2*q1[0]*q1[3], 2*q1[0]*q1[0] + 2*q1[1]*q1[1] - 1);
   euler[1] = -asin(2*q1[1]*q1[3] + 2*q1[0]*q1[2]);
   euler[2] = atan2(2*q1[2]*q1[3] - 2*q1[0]*q1[1], 2*q1[0]*q1[0] + 2*q1[3]*q1[3] - 1);

   return 0;
}


float map(long x, long in_min, long in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Hardware IRQ for the MPU6050 DMP algorithm.
void EINT3_IRQHandler(void)
{
   static signed portBASE_TYPE xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

   if(GPIO_GetIntStatus(0, 4, 1))
   {
      GPIO_ClearInt(0,(1<<4));
      if (mpuSempahore!=NULL){
         xSemaphoreGiveFromISR(mpuSempahore,&xHigherPriorityTaskWoken);
      }
   }
   portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

void DataCollection(void *p){

   int16_t gyro[3];
   int16_t accel[3];
   int32_t quat[4];
   float scale;
   uint8_t more;
   float atti_buffer[3];
   int16_t sensors;
   float bmp_temp, pressure, alt=0.0, c;
   uint16_t i;
   int l; //contador
   l=0;
   mpu_get_gyro_sens(&scale);
   vSemaphoreCreateBinary(mpuSempahore);
   xSemaphoreTake(mpuSempahore,0);
   NVIC_EnableIRQ(EINT3_IRQn);



   // ---- Barometer config ------------
   c = 0.1;

   //===========================================
   prescaler = PRESCALER_VALUE;
   while(1){

      // Wait here for MPU DMP interrupt at 200Hz
      xSemaphoreTake(mpuSempahore,portMAX_DELAY); //FIXME: instead of portMAX it would be nice to have a time out for errors

      //-----------------------------------------------------------------------
      // Data input stage
      //-----------------------------------------------------------------------

      portENTER_CRITICAL();
      dmp_read_fifo(gyro, accel, quat, NULL, &sensors, &more);
      portEXIT_CRITICAL();

      MPU6050_dmpGetEuler(atti_buffer,quat);

      atti_buffer[0] = atti_buffer[0]*180.0/PI;
      atti_buffer[1] = atti_buffer[1]*180.0/PI;
      atti_buffer[2] = atti_buffer[2]*180.0/PI;



#if USE_BAROMETER
      //quadrotor.sv.temperature = BMP085_GetTemperature();
      //quadrotor.sv.current_pressure = BMP085_GetPressure();
      //quadrotor.sv.altitude =  c*BMP085_CalculateAltitude(quadrotor.sv.floor_pressure, quadrotor.sv.current_pressure) + (1-c)*quadrotor.sv.altitude;
#endif




      // --------------------- Biasing ---------------------

      if ((quadrotor.mavlink_control.buttons & BTN_START) != 0){
         atti_bias[ROLL] = atti_buffer[2];
         atti_bias[PITCH] = -atti_buffer[1];
         atti_bias[YAW] = atti_buffer[0];


         uint8_t i;
         for (i=0;i<3;i++){
            qPID_Init(&quadrotor.rateController[i]);
         }

         for (i=0;i<3;i++){
            qPID_Init(&quadrotor.attiController[i]);
         }
         qPID_Init(&quadrotor.altitudeController);

      }


      quadrotor.sv.attitude[0] = atti_buffer[2] - atti_bias[ROLL];
      quadrotor.sv.attitude[1] = -atti_buffer[1] - atti_bias[PITCH];
      quadrotor.sv.attitude[2] = atti_buffer[0] - atti_bias[YAW];
      quadrotor.sv.rate[ROLL] = -gyro[0]/scale;
      quadrotor.sv.rate[PITCH] = gyro[1]/scale;
      quadrotor.sv.rate[YAW] = -gyro[2]/scale;

      // The axis correspond to the assigment on the qground control and the mapping of the mavlink_control functions.
      //*******************************************************************************************
      //Prueba por Facu A. (mapping del control z de altitud, antes tenia un setpoint de 0.5
      quadrotor.sv.setpoint[ALTITUDE] = map(quadrotor.mavlink_control.z,-1000,1000,-1.0,1.0)+0.6171;
      //quadrotor.sv.setpoint[ALTITUDE] = 0.5;
      //z_bias = map((quadrotor.mavlink_control.z < 100)?0:quadrotor.mavlink_control.z,0,1000,0.0,1.0);

      //***********************************************************************************************
      quadrotor.sv.setpoint[ROLL] = map(quadrotor.mavlink_control.y,-1000,1000,-40.0,40.0);
      quadrotor.sv.setpoint[PITCH] = map(quadrotor.mavlink_control.x,-1000,1000,-40.0,40.0);
      quadrotor.sv.setpoint[YAW] = map(quadrotor.mavlink_control.r,-1000,1000,-180.0,180.0);
      if ((quadrotor.mavlink_control.buttons & BTN_CROSS) != 0){
    	  quadrotor.sv.setpoint[ROLL]=5;
      }
      if ((quadrotor.mavlink_control.buttons & BTN_TRIANGLE) != 0){
    	  quadrotor.sv.setpoint[ROLL]=-5;
          }
      if ((quadrotor.mavlink_control.buttons & BTN_UP) != 0){
          	  Identification=ENABLED;
                }

      //-----------------------------------------------------------------------
      // Controller processing stage
      //-----------------------------------------------------------------------
      if (CAS==ENABLED){
      // CAS
      quadrotor.sv.attiCtrlOutput[ROLL] = qPID_Procees(&quadrotor.attiController[ROLL],quadrotor.sv.setpoint[ROLL],quadrotor.sv.attitude[ROLL]);
      quadrotor.sv.attiCtrlOutput[PITCH] = qPID_Procees(&quadrotor.attiController[PITCH],quadrotor.sv.setpoint[PITCH],quadrotor.sv.attitude[PITCH]);
      // SAS
      quadrotor.sv.rateCtrlOutput[ROLL] = qPID_Procees(&quadrotor.rateController[ROLL],quadrotor.sv.attiCtrlOutput[ROLL],quadrotor.sv.rate[ROLL]);
      quadrotor.sv.rateCtrlOutput[PITCH] = qPID_Procees(&quadrotor.rateController[PITCH],quadrotor.sv.attiCtrlOutput[PITCH],quadrotor.sv.rate[PITCH]);
       //quadrotor.sv.rateCtrlOutput[ROLL] = qPID_Procees(&quadrotor.rateController[ROLL],quadrotor.sv.setpoint[ROLL],quadrotor.sv.rate[ROLL]);
       //quadrotor.sv.rateCtrlOutput[PITCH] = qPID_Procees(&quadrotor.rateController[PITCH],quadrotor.sv.setpoint[PITCH],quadrotor.sv.rate[PITCH]);
        quadrotor.sv.rateCtrlOutput[YAW] = qPID_Procees(&quadrotor.rateController[YAW],quadrotor.sv.setpoint[YAW],quadrotor.sv.rate[YAW]);
      }else{
    	  quadrotor.sv.rateCtrlOutput[ROLL] = qPID_Procees(&quadrotor.rateController[ROLL],quadrotor.sv.setpoint[ROLL],quadrotor.sv.rate[ROLL]);
    	  quadrotor.sv.rateCtrlOutput[PITCH] = qPID_Procees(&quadrotor.rateController[PITCH],quadrotor.sv.setpoint[PITCH],quadrotor.sv.rate[PITCH]);
    	  quadrotor.sv.rateCtrlOutput[YAW] = qPID_Procees(&quadrotor.rateController[YAW],quadrotor.sv.setpoint[YAW],quadrotor.sv.rate[YAW]);
    	        }

      //if (prescaler-- == 0){
      //  RangeFinder_getDistance();
      //   prescaler = PRESCALER_VALUE;
         quadrotor.sv.altitudeCtrlOutput = qPID_Procees(&quadrotor.altitudeController,quadrotor.sv.setpoint[ALTITUDE],quadrotor.sv.altitude);
      //}
      //-----------------------------------------------------------------------
      // Bloque de identificación
      //-----------------------------------------------------------------------

  if(Identification == ENABLED){
         estimate(&quadrotor.id_rate[ROLL], quadrotor.sv.rateCtrlOutput[ROLL],quadrotor.sv.rate[ROLL]);
        // estimate(&quadrotor.id_rate[PITCH], quadrotor.sv.rateCtrlOutput[PITCH],quadrotor.sv.rate[PITCH]);
        // estimate(&quadrotor.id_rate[YAW], quadrotor.sv.rateCtrlOutput[YAW],quadrotor.sv.rate[YAW]);
        // estimate(&quadrotor.id_atti[ROLL], quadrotor.sv.attiCtrlOutput[ROLL],quadrotor.sv.attitude[ROLL]);
        // estimate(&quadrotor.id_atti[PITCH], quadrotor.sv.attiCtrlOutput[PITCH],quadrotor.sv.attitude[PITCH]);
        // estimate(&quadrotor.id_atti[YAW], quadrotor.sv.attiCtrlOutput[YAW],quadrotor.sv.attitude[YAW]);
  }
      //------------------------------------------------------------------------------------
     //toma de tiempos
     //------------------------------------------------------------------------------------
       if(l==0){
         Tk[0]=xTaskGetTickCount()/portTICK_RATE_MS;
         l=1;
       }else {Tk[1]=xTaskGetTickCount()/portTICK_RATE_MS;
       quadrotor.sv.Tm=Tk[1]-Tk[0];
       l=0;
       };

         //-----------------------------------------------------------------------
       // Control Output stage
      //-----------------------------------------------------------------------

      control[ROLL] = quadrotor.sv.rateCtrlOutput[ROLL];
      control[PITCH] =  quadrotor.sv.rateCtrlOutput[PITCH];
      control[YAW] = -quadrotor.sv.rateCtrlOutput[YAW]; //FIXME: there is a problem with the sign (maybe in the Mq)
      control[ALTITUDE] = quadrotor.sv.altitudeCtrlOutput;

      //***************************************************************************************************************************************
      // CONTROL OUTPUTS * PONDERATION
      //***************************************************************************************************************************************
      Output[ROLL]       =    control[ROLL]*K_PHI;
      Output[PITCH]      =    control[PITCH]*K_THETA;
      Output[YAW]        =    control[YAW]*K_PSI;
      Output[ALTITUDE]    =    control[ALTITUDE]*K_Z;
      //****************************************************************************************************************************************
      //                  ADAPTATION OF OUTPUTS ( THROTTLE OF SPEEDS => Y=K*x + b CHANGE TO a => Y=K*(x +b/K) i.e. FOR EACH x IT PLUS b/K
      //****************************************************************************************************************************************

      //linealizo las salidas
      if(speeds.Adapt_Mode == ENABLED){

      //AdaptedOutput[ROLL]      = Adapt_ESCs(&speeds.variador[0],Output[ROLL], THRUST);
      //AdaptedOutput[PITCH]   = Adapt_ESCs(&speeds.variador[1],Output[PITCH], THRUST);
      AdaptedOutput[YAW]      = Adapt_ESCs(&speeds.variador[2],Output[YAW], TORQUE);
      //AdaptedOutput[ALTITUDE]   = Adapt_ESCs(&speeds.variador[3],Output[ALTITUDE],THRUST);

      //*****************************************************************************************************************************************
      //                              MIX OF ADAPTED OUTPUTS TO THE MOTORS
      //*****************************************************************************************************************************************

      	  	quadrotor.sv.motorOutput[0] = (   control[ALTITUDE]*K_Z - control[ROLL]*K_PHI - control[PITCH]*K_THETA -  AdaptedOutput[YAW]);
            quadrotor.sv.motorOutput[1] = (   control[ALTITUDE]*K_Z - control[ROLL]*K_PHI + control[PITCH]*K_THETA +  AdaptedOutput[YAW]);
            quadrotor.sv.motorOutput[2] = (   control[ALTITUDE]*K_Z + control[ROLL]*K_PHI + control[PITCH]*K_THETA -  AdaptedOutput[YAW]);
            quadrotor.sv.motorOutput[3] = (   control[ALTITUDE]*K_Z + control[ROLL]*K_PHI - control[PITCH]*K_THETA +  AdaptedOutput[YAW]);


                 quadrotor.sv.motorOutput[0] = (   AdaptedOutput[ALTITUDE] - AdaptedOutput[ROLL] - AdaptedOutput[PITCH] - AdaptedOutput[YAW]);
                 quadrotor.sv.motorOutput[1] = (   AdaptedOutput[ALTITUDE] - AdaptedOutput[ROLL] + AdaptedOutput[PITCH] + AdaptedOutput[YAW]);
                 quadrotor.sv.motorOutput[2] = (   AdaptedOutput[ALTITUDE] + AdaptedOutput[ROLL] + AdaptedOutput[PITCH] - AdaptedOutput[YAW]);
                 quadrotor.sv.motorOutput[3] = (   AdaptedOutput[ALTITUDE] + AdaptedOutput[ROLL] - AdaptedOutput[PITCH] + AdaptedOutput[YAW]);

                               }
        else{
      quadrotor.sv.motorOutput[0] = (   control[ALTITUDE]*K_Z - control[ROLL]*K_PHI - control[PITCH]*K_THETA - control[YAW]*K_PSI);
      quadrotor.sv.motorOutput[1] = (   control[ALTITUDE]*K_Z - control[ROLL]*K_PHI + control[PITCH]*K_THETA + control[YAW]*K_PSI);
      quadrotor.sv.motorOutput[2] = (   control[ALTITUDE]*K_Z + control[ROLL]*K_PHI + control[PITCH]*K_THETA - control[YAW]*K_PSI);
      quadrotor.sv.motorOutput[3] = (   control[ALTITUDE]*K_Z + control[ROLL]*K_PHI - control[PITCH]*K_THETA + control[YAW]*K_PSI);

           }

      if (quadrotor.mode == ESC_STANDBY){
         qESC_SetOutput(MOTOR1,0);
         qESC_SetOutput(MOTOR2,0);
         qESC_SetOutput(MOTOR3,0);
         qESC_SetOutput(MOTOR4,0);

         if ((fabsf(quadrotor.sv.attitude[ROLL])<=ATTI_THRESHOLD) && (fabsf(quadrotor.sv.attitude[PITCH])<=ATTI_THRESHOLD) && (fabsf(quadrotor.sv.attitude[YAW])<=ATTI_THRESHOLD)){
            for (i=1;i<5;i++) qLed_TurnOn(leds[i]);
         }else{
            for (i=1;i<5;i++) qLed_TurnOff(leds[i]);
         }

      }
      else{

         // Motor command
         qESC_SetOutput(MOTOR1,quadrotor.sv.motorOutput[0]+68);//(variador 1)+68 a cada uno
         qESC_SetOutput(MOTOR2,quadrotor.sv.motorOutput[1]+68);//variador 2
         qESC_SetOutput(MOTOR3,quadrotor.sv.motorOutput[2]+68);//variador 3
         qESC_SetOutput(MOTOR4,quadrotor.sv.motorOutput[3]+68);//varidor 4 azul

      }
   }

}
