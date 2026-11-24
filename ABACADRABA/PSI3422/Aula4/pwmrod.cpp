//pwmroda4.cpp
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

int main(){
	wiringPiSetup();
	if(softPwmCreate(0,0,100))exit(1);
	if(softPwmCreate(1,0,100))exit(1);
	if(softPwmCreate(2,0,100))exit(1);
	if(softPwmCreate(3,0,100))exit(1);
	for(int i=0; i<2; i++){
		softPwmWrite(0,90); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,90); delay(2000);	
		
		softPwmWrite(0,0); softPwmWrite(1,90);
		softPwmWrite(2,90); softPwmWrite(3,0); delay(2000);	
		
		softPwmWrite(0,60); softPwmWrite(1,0);
		softPwmWrite(2,0); softPwmWrite(3,60); delay(2000);	
		
		softPwmWrite(0,0); softPwmWrite(1,60);
		softPwmWrite(2,60); softPwmWrite(3,0); delay(2000);	
	}
	softPwmStop(0); softPwmStop(1);
	softPwmStop(2); softPwmStop(3);
}

