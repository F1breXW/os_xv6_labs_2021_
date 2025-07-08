#include "kernel/types.h"
#include "user/user.h"

int 
main(int argc, char** argv ){
    int pid;
    int parent_fd[2]; // 父进程用于接收子进程消息的管道
    int child_fd[2];  // 子进程用于接收父进程消息的管道
    char buf[20];
    // 为父子进程分别建立管道
    pipe(child_fd); 
    pipe(parent_fd);

    // 子进程部分
    if((pid = fork()) == 0){
        close(parent_fd[1]); // 关闭父进程写端
        read(parent_fd[0],buf, 4); // 读取父进程发来的"ping"
        printf("%d: received %s\n",getpid(), buf); // 打印收到的消息
        close(child_fd[0]); // 关闭子进程读端
        write(child_fd[1], "pong", sizeof(buf)); // 回复父进程"pong"
        exit(0);
    }
    // 父进程部分
    else{
        close(parent_fd[0]); // 关闭父进程读端
        write(parent_fd[1], "ping",4); // 向子进程发送"ping"
        close(child_fd[1]); // 关闭子进程写端
        read(child_fd[0], buf, sizeof(buf)); // 读取子进程回复的"pong"
        printf("%d: received %s\n", getpid(), buf); // 打印收到的消息
        exit(0);
    }
    
}
