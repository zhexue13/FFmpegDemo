// FLVParser.cpp: 定义控制台应用程序的入口点。
//

/**
 * FLV封装格式解析测试
 *
*/
#include "FLVParse.h"
#include <string>

using namespace std;

int main(){

    std::string path = "../files/flv/";
	std::string file_name = "cuc_ieschool.flv";
	simplest_flv_parser(path, file_name);

    return 0;
}

