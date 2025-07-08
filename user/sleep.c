#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc,char *argv[])
{
    // 如果调用时在sleep后没加参数，则提示请输入参数
    if(argc == 1)
    {
        // 参数不足时，提示用户输入参数
        printf("Please enter the paremeters!");
    }
    else
    {
        // 将字符串参数转换为整数，作为sleep的时间（tick数）
        int duration = atoi(argv[1]);
        // 调用sleep系统调用，挂起当前进程duration个tick
        sleep(duration);
    }
    // 程序正常退出
    exit(0);
}