//parar.cpp

#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

int main(){
	wiringPiSetup();
	if(softPwmCreate(0,0,100))exit(1);
	if(softPwmCreate(4,0,100))exit(1);
	if(softPwmCreate(2,0,100))exit(1);
	if(softPwmCreate(3,0,100))exit(1);

	
	while(true){
			
		
		//stop
			softPwmStop(0); softPwmStop(4);
	softPwmStop(2); softPwmStop(3);
	}
	softPwmStop(0); softPwmStop(4);
	softPwmStop(2); softPwmStop(3);
	return 0;
}

