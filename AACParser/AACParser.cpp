// AACParser.cpp: 定义控制台应用程序的入口点。
//

#include "AACParse.h"
#include <cstdlib>
#include <cstring>

/**
 * AAC码流数据解析测试
 *
*/

int main(){
	const char *url = "../files/aac/yangzi.aac";
	if (!simplest_aac_parser(url))
		printf("解析成功\n");
	else
		printf("解析失败\n");
    return 0;
}

