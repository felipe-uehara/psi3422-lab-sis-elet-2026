#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

int main(){
	VideoCapture w(0);
	if(!w.isOpened()){
		fprintf(stderr, "Erro: Abertura de webcam 0.");
		exit(1);
	}
	w.set(CAP_PROP_FRAME_WIDTH,640);
	w.set(CAP_PROP_FRAME_HEIGHT,480);
	Mat_<Vec3b> a;
	namedWindow("janela");
	while(true){
	//w.grab(); 
	w >> a;
	imshow("janela", a);
	int ch=(signed char)(waitKey(30));
	if(ch>=0) break;
	}
}
