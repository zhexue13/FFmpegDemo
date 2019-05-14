// H264Parser.cpp: 定义控制台应用程序的入口点。
//
/**
 * H.264码流数据解析测试
 *
*/
#include "H264Parse.h"


int main(){

	const char *url= "../files/h264/sintel.h264";
	if (!simplest_h264_parser(url))
		printf("解析成功\n");
	else
		printf("解析失败\n");

    return 0;
}

