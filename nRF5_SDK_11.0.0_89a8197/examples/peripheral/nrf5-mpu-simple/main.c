 /* 
  * This example is not extensively tested and only 
  * meant as a simple explanation and for inspiration. 
  * NO WARRANTY of ANY KIND is provided. 
  */

#include <stdio.h>
#include "boards.h"
#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "app_mpu.h"
#include <math.h>
#define PI 3.14159
/*UART buffer size. */
#define UART_TX_BUF_SIZE 256
#define UART_RX_BUF_SIZE 1
#define TAU 0.1
#define TS 0.03
#define G 15800
#define LOW 5;
 double dt=0.005;
 double gyroXangle, gyroYangle; // Angle calculate using the gyro only
double compAngleX, compAngleY; // Calculated angle using a complementary filter
double kalAngleX, kalAngleY; // Calculated angle using a Kalman filter
double gyro_x,gyro_y,gyro_z;
double real_acc_x,real_acc_y,real_acc_z;
double g_acc_x,g_acc_y,g_acc_z;
double pre_real_acc_x=100,pre_real_acc_y=100,pre_real_acc_z=100;
double LP_real_acc_x,LP_real_acc_y,LP_real_acc_z;
double move_x=0,move_y=0,move_z=0;
	struct kaldata kalx;
	struct kaldata kaly;
struct kaldata{
	double bias;
	double angle;
	double rate;
	double P[2][2];
	double Q_angle;
	double Q_gyro;
	double R_measure;
	double Q_bias;
	
};
static void uart_events_handler(app_uart_evt_t * p_event)
{
    switch (p_event->evt_type)
    {
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
double abs(double number){
	if(number<0)
		return -number;
	else
		return number;
}

static void uart_config(void)
{
    uint32_t                     err_code;
    const app_uart_comm_params_t comm_params =
    {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_DISABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_events_handler,
                       APP_IRQ_PRIORITY_LOW,
                       err_code);

    APP_ERROR_CHECK(err_code);
}


void mpu_setup(void)
{
    ret_code_t ret_code;
    // Initiate MPU driver
    ret_code = mpu_init();
    APP_ERROR_CHECK(ret_code); // Check for errors in return value
    
    // Setup and configure the MPU with intial values
    mpu_config_t p_mpu_config = MPU_DEFAULT_CONFIG(); // Load default values
    p_mpu_config.smplrt_div = 19;   // Change sampelrate. Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV). 19 gives a sample rate of 50Hz
    p_mpu_config.accel_config.afs_sel = AFS_2G; // Set accelerometer full scale range to 2G
    ret_code = mpu_config(&p_mpu_config); // Configure the MPU with above values
    APP_ERROR_CHECK(ret_code); // Check for errors in return value 
}


double kalgetAngle(struct kaldata * kal,double newAngle, double newRate, double dt){
	double K[2];
	kal->rate=newRate-kal->bias;
	kal->angle+=dt*kal->rate;
	kal->P[0][0]+=dt*(dt*kal->P[1][1]-kal->P[0][1]-kal->P[1][0]+kal->Q_angle);
	kal->P[0][1]-=dt*kal->P[1][1];
	kal->P[1][0]-=dt*kal->P[1][1];
	kal->P[1][1]+=kal->Q_bias*dt;
	double S=kal->P[0][0]+kal->R_measure;
	K[0] = kal->P[0][0] / S;
  K[1] = kal->P[1][0] / S;
	double y=newAngle - kal->angle;
	kal->angle+=K[0]*y;
	kal->bias+=K[1]*y;
	double P00_temp=kal->P[0][0];
	double P01_temp=kal->P[0][1];
	kal->P[0][0] -= K[0] * P00_temp;
  kal->P[0][1] -= K[0] * P01_temp;
  kal->P[1][0] -= K[1] * P00_temp;
  kal->P[1][1] -= K[1] * P01_temp;
	return kal->angle;
}
void kalinit(struct kaldata * kal){
	kal->Q_angle = 0.001;
	kal->Q_bias = 0.003;
	kal->R_measure=0.03;
	kal->angle=0;
	kal->bias=0;
	kal->P[0][0]=0;
	kal->P[0][1]=0;
	kal->P[1][0]=0;
	kal->P[1][1]=0;
}
	
	
int main(void)
{    
	int i=0;
	int j=0;
	double acc_x,acc_y,acc_z;
	double law_acc_x=0,law_acc_y=0,law_acc_z=0;
	double law_gyro_x=0,law_gyro_y=0,law_gyro_z=0;
	double low_pass_acc_x[5],low_pass_acc_y[5],low_pass_acc_z[5];
	double low_pass_gyro_x[5],low_pass_gyro_y[5],low_pass_gyro_z[5];
	
	
	
	
  accel_values_t acc_values;
	gyro_values_t gyro_values;
	kalinit(&kalx);
	kalinit(&kaly);	
  uint32_t err_code;
    uart_config();
    mpu_setup();

			err_code = mpu_read_accel(&acc_values);
			APP_ERROR_CHECK(err_code);
			err_code = mpu_read_gyro(&gyro_values);
			APP_ERROR_CHECK(err_code);
	
			acc_x=acc_values.x;
			acc_z=acc_values.z;
			acc_y=acc_values.y;
			
	double roll=atan2(acc_y, acc_z) * (45.0/atan(1.0));
	double pitch=atan(-acc_x / sqrt(acc_y * acc_y + acc_z * acc_z)) *  (45.0/atan(1.0));
	kalx.angle=roll;
	kaly.angle=pitch;
	  compAngleX = roll;
  compAngleY = pitch;
	
	//필터 초기화
	for(i=0;i<5;i++){
		low_pass_acc_x[i]=0;
		low_pass_acc_y[i]=0;
		low_pass_acc_z[i]=0;
		
		low_pass_gyro_x[i]=0;
		low_pass_gyro_y[i]=0;
		low_pass_gyro_z[i]=0;
	}
		
  while(1)
    {
			// Read accelerometer sensor values
			err_code = mpu_read_accel(&acc_values);
			APP_ERROR_CHECK(err_code);
			err_code = mpu_read_gyro(&gyro_values);
			APP_ERROR_CHECK(err_code);
			
			
			law_acc_x=acc_values.x;
			law_acc_z=acc_values.z;
			law_acc_y=acc_values.y;
			
			law_gyro_x=gyro_values.x;
			law_gyro_y=gyro_values.y;
			law_gyro_z=gyro_values.z;
			
			
			//로우패스
			acc_x=0;
			acc_y=0;
			acc_z=0;
			gyro_x=0;
			gyro_y=0;
			gyro_z=0;
			
			low_pass_acc_x[j]=law_acc_x;
			low_pass_acc_y[j]=law_acc_y;
			low_pass_acc_z[j]=law_acc_z;
			
			low_pass_gyro_x[j]=law_gyro_x;
			low_pass_gyro_y[j]=law_gyro_y;
			low_pass_gyro_z[j]=law_gyro_z;
			
			
			
			for(i=0;i<5;i++){
					acc_x+=low_pass_acc_x[j];
					acc_y+=low_pass_acc_y[j];
					acc_z+=low_pass_acc_z[j];
					gyro_x+=low_pass_gyro_x[j];
					gyro_y+=low_pass_gyro_y[j];
					gyro_z+=low_pass_gyro_z[j];
				
			}
			acc_x=acc_x/5;
			acc_y=acc_y/5;
			acc_z=acc_z/5;
					
			gyro_x=gyro_x/5;
			gyro_y=gyro_y/5;
			gyro_z=gyro_z/5;
			
			if(j==4)
				j=0;
			else
				j++;
			
			//로우패스끝
			
			
			double roll=atan2(acc_y, acc_z) * (45.0/atan(1.0));
	    double pitch=atan(-acc_x / sqrt(acc_y * acc_y + acc_z * acc_z)) *  (45.0/atan(1.0));
	
			double gyroXrate=gyro_x/131.0;
			double gyroYrate=gyro_y/131.0;
			
		if ((roll < -90 && kalAngleX > 90) || (roll > 90 && kalAngleX < -90)) {
    kalx.angle=roll;
    compAngleX = roll;
    kalAngleX = roll;
    gyroXangle = roll;
  } else
    kalAngleX = kalgetAngle(&kalx,roll, gyroXrate, dt); // Calculate the angle using a Kalman filter

	if (abs(kalAngleX) > 90)
    gyroYrate = -gyroYrate; // Invert rate, so it fits the restriced accelerometer reading
  kalAngleY = kalgetAngle(&kaly,pitch, gyroYrate, dt);
			
	
  gyroXangle += gyroXrate * dt; // Calculate gyro angle without any filter
  gyroYangle += gyroYrate * dt;

		  compAngleX = 0.93 * (compAngleX + gyroXrate * dt) + 0.07 * roll; // Calculate the angle using a Complimentary filter
  compAngleY = 0.93 * (compAngleY + gyroYrate * dt) + 0.07 * pitch;

	  if (gyroXangle < -180 || gyroXangle > 180)
    gyroXangle = kalAngleX;
  if (gyroYangle < -180 || gyroYangle > 180)
    gyroYangle = kalAngleY;
	
	g_acc_x=G*sin(((kalAngleY+180)/180*PI))*cos(((kalAngleX)/180*PI));
	
	g_acc_y=G*sin((kalAngleX/180*PI))*cos(((kalAngleY)/180*PI));

	g_acc_z=G*cos((kalAngleX/180*PI))*cos((kalAngleY/180*PI));

	real_acc_x=acc_x-g_acc_x;
	real_acc_y=acc_y-g_acc_y;
	real_acc_z=acc_z-g_acc_z;
	
	
	
			if(i==300){
				
				
				printf(" kalAngleX:%f\r\n",kalAngleX);
				printf(" kalAngleY:%f\r\n",kalAngleY);
				
				printf(" real_x:%f\r\n",real_acc_x);
				printf(" real_y:%f\r\n",real_acc_y);
				printf(" real_z:%f\r\n",real_acc_z);
				
				
				
				
			i=0;
			}
			i++;
			nrf_delay_ms(1);
			
			
			
    }
}

/** @} */

