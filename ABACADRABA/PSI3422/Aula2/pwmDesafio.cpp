//pwmDesafio.cpp

#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

int main(){
	wiringPiSetup();
	if(softPwmCreate(0,0,100))exit(1);
	if(softPwmCreate(1,0,100))exit(1);
	if(softPwmCreate(2,0,100))exit(1);
	if(softPwmCreate(3,0,100))exit(1);
	//delay(10000);
	int vel1 = 80; //motors'base speed
	int vel2 = 90; //motors'base speed
	int oneMeter = 7000;
	while(true){
	
		//moving forward for 1 meter
		softPwmWrite(0,vel1); softPwmWrite(1,0);
		softPwmWrite(2,vel2); softPwmWrite(3,0); delay(oneMeter);	
		
		//stop
		softPwmWrite(0,0); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,0); delay(500);	
		
		//turn around
		softPwmWrite(0,vel1); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,vel2); delay(3000);	
		
		//stop
		softPwmWrite(0,0); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,0); delay(500);	
		
		//moving forward for 1 meter
		softPwmWrite(0,vel1); softPwmWrite(1,0);
		softPwmWrite(2,vel2); softPwmWrite(3,0); delay(oneMeter);	
		
		//stop
		softPwmWrite(0,0); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,0); delay(500);	
	softPwmStop(0); softPwmStop(4);
	softPwmStop(2); softPwmStop(3);
}
	return 0;
}

