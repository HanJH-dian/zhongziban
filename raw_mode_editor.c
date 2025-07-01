#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// 保存原始终端设置
static struct termios orig_termios;

// 错误处理
void die(const char *s) {
    perror(s);
    exit(1);
}

// 恢复终端原始设置
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

// 原始模式函数
void enableRawMode() {
    // 获取当前终端设置
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    
    // 退出时恢复终端
    atexit(disableRawMode);
    
    // 创建修改后的终端设置
    struct termios raw = orig_termios;
    
    // 输入模式标志 (c_iflag)
    raw.c_iflag &= ~(BRKINT  // 禁用BREAK中断处理
                  | ICRNL   // 禁用CR转NL
                  | INPCK   // 禁用奇偶校验
                  | ISTRIP  // 禁用第8位剥离
                  | IXON);  // 禁用软件流控制输出
    
    // 输出模式标志 (c_oflag)
    raw.c_oflag &= ~(OPOST); // 禁用输出处理
    
    // 控制模式标志 (c_cflag)
    raw.c_cflag |= (CS8);    // 设置字符大小为8位
    
    // 本地模式标志 (c_lflag)
    raw.c_lflag &= ~(ECHO    // 禁用回显
                   | ICANON  // 禁用规范模式
                   | IEXTEN  // 禁用扩展输入处理
                   | ISIG);  // 禁用信号处理
    
    // 控制字符设置 (c_cc)
    raw.c_cc[VMIN] = 0;      // 最小读取字符数
    raw.c_cc[VTIME] = 1;     // 100ms超时 (VTIME以十分之一秒为单位)
    
    // 应用修改后的终端设置
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

// 打印按键信息
void printKeyInfo(unsigned char c) {
    if (iscntrl(c)) {
        // 控制字符显示为 ^X 格式
        printf("%d (^%c)\r\n", c, c + 64);
    } else {
        // 可打印字符直接显示
        printf("%d ('%c')\r\n", c, c);
    }
}

int main() {
    // 启用原始模式
    enableRawMode();
    
    printf("终端原始模式已启用。按下 'q' 退出。\r\n");
    printf("按键信息将显示为: ASCII值 (字符表示)\r\n");
    printf("------------------------------------\r\n");
    
    while (1) {
        unsigned char c = '\0';
        
        // 读取用户输入
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
            die("read");
        }
        
        // 显示按键信息
        if (c != '\0') {
            printKeyInfo(c);
        }
        
        // 检测退出键 'q'
        if (c == 'q') break;
    }
     
    // 注意：disableRawMode() 会在程序退出时通过 atexit 自动调用
    printf("\r\n终端原始模式已禁用，恢复标准设置。\r\n");
    return 0;
}