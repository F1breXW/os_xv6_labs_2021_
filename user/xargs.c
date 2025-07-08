#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    char buf[512]; // 用于存储一行输入
    char *args[MAXARG]; // exec参数数组
    int i;
    // 预先将命令和参数填入args
    for(i = 1; i < argc; i++){
        args[i-1] = argv[i];
    }
    int base = argc - 1; // 命令参数个数
    int n;
    // 逐字符读取，直到遇到'\n'为止
    while(1){
        int pos = 0;
        // 读取一行
        while((n = read(0, buf+pos, 1)) == 1 && buf[pos] != '\n' && buf[pos] != '\0'){
            pos++;
            if(pos >= sizeof(buf)-1) break;
        }
        if(n == 0 && pos == 0) // 文件结尾
            break;
        buf[pos] = 0; // 字符串结尾
        if(pos == 0) // 空行
            continue;
        // 构造参数数组
        args[base] = buf;
        args[base+1] = 0;
        int pid = fork();
        if(pid == 0){
            exec(args[0], args);
            // exec失败
            fprintf(2, "xargs: exec failed\n");
            exit(1);
        } else if(pid > 0){
            wait(0);
        } else {
            fprintf(2, "xargs: fork failed\n");
            exit(1);
        }
    }
    exit(0);
}



