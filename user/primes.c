#include "kernel/types.h"
#include "user/user.h"


// 递归处理函数：每个进程负责筛选一个质数
void 
process(int *pfd) {
    close(pfd[1]); // 关闭写端，只读
    int prime;
    // 读取第一个数，作为本进程要筛选的质数
    if (read(pfd[0], &prime, sizeof(prime)) == 0) {
        close(pfd[0]);
        exit(0);
    }
    printf("prime %d\n", prime);
    
    int n;
    int cfd[2];
    pipe(cfd); // 创建下一个进程的管道
    int has_child = 0;
    // 读取剩余数字，筛选后传递给下一个进程
    while (read(pfd[0], &n, sizeof(n)) != 0) {
        if (n % prime != 0) {
            // 第一次需要fork子进程
            if (!has_child) {
                if (fork() == 0) {
                    // 子进程递归处理
                    close(pfd[0]);
                    process(cfd);
                    exit(0);
                }
                has_child = 1;
            }
            write(cfd[1], &n, sizeof(n));
        }
    }
    // 关闭所有用过的文件描述符
    close(pfd[0]);
    close(cfd[1]);
    if (has_child) {
        wait(0); // 等待子进程结束
    }
    exit(0);
}


int 
main() {
    int pfd[2];
    pipe(pfd);
    // 父进程写入2~35
    if (fork() == 0) {
        // 子进程递归处理
        process(pfd);
        exit(0);
    } else {
        close(pfd[0]); // 只写
        for (int i = 2; i <= 35; i++) {
            write(pfd[1], &i, sizeof(i));
        }
        close(pfd[1]); // 写完关闭
        wait(0); // 等待子进程结束
    }
    exit(0);
}


