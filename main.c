#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

// 保存原始终端设置
static struct termios orig_termios;

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

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
    raw.c_cc[VTIME] = 1;     // 100ms超时
    
    // 应用修改后的终端设置
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

// 特殊按键定义
#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_RIGHT 1002
#define KEY_LEFT 1003
#define KEY_PAGE_UP 1004
#define KEY_PAGE_DOWN 1005
#define KEY_HOME 1006
#define KEY_END 1007
#define KEY_DEL 1008
#define KEY_ESC 0x1b

// 编辑器状态
struct editorState {
    int screenrows;
    int screencols;
    int cursor_x;
    int cursor_y;
};

struct editorState E;

// 追加缓冲区结构
struct appendBuffer {
    char *b;
    int len;
};

#define AB_INIT {NULL, 0}

// 向追加缓冲区添加内容
void abAppend(struct appendBuffer *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

// 释放追加缓冲区
void abFree(struct appendBuffer *ab) {
    free(ab->b);
}

// 获取终端大小
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // 如果ioctl失败，尝试通过移动光标到右下角来获取尺寸
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    // 请求光标位置报告
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    // 读取响应
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    
    // 解析响应
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}

// 清屏
void clearScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

// 读取按键
int readKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    // 处理转义序列
    if (c == '\x1b') {
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DEL;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_UP;
                    case 'B': return KEY_DOWN;
                    case 'C': return KEY_RIGHT;
                    case 'D': return KEY_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }
        
        return '\x1b';
    } else {
        return c;
    }
}

// 移动光标
void moveCursor(int key) {
    switch (key) {
        case KEY_UP:
            if (E.cursor_y > 0) E.cursor_y--;
            break;
        case KEY_DOWN:
            if (E.cursor_y < E.screenrows - 1) E.cursor_y++;
            break;
        case KEY_LEFT:
            if (E.cursor_x > 0) E.cursor_x--;
            break;
        case KEY_RIGHT:
            if (E.cursor_x < E.screencols - 1) E.cursor_x++;
            break;
        case KEY_HOME:
            E.cursor_x = 0;
            break;
        case KEY_END:
            E.cursor_x = E.screencols - 1;
            break;
        case KEY_PAGE_UP:
            E.cursor_y = 0;
            break;
        case KEY_PAGE_DOWN:
            E.cursor_y = E.screenrows - 1;
            break;
    }
}

// 绘制欢迎信息
void drawWelcome(struct appendBuffer *ab) {
    char welcome[80];
    int welcomelen = snprintf(welcome, sizeof(welcome), 
        "Tiny Editor -- version 0.0.1");
    if (welcomelen > E.screencols) welcomelen = E.screencols;
    
    // 居中显示欢迎信息
    int padding = (E.screencols - welcomelen) / 2;
    if (padding) {
        abAppend(ab, "~", 1);
        padding--;
    }
    while (padding--) abAppend(ab, " ", 1);
    
    abAppend(ab, welcome, welcomelen);
}

// 刷新屏幕
void refreshScreen() {
    struct appendBuffer ab = AB_INIT;
    
    // 隐藏光标
    abAppend(&ab, "\x1b[?25l", 6);
    
    // 移动光标到左上角
    abAppend(&ab, "\x1b[H", 3);
    
    // 绘制屏幕内容
    for (int y = 0; y < E.screenrows; y++) {
        // 绘制行号
        char line[32];
        int linelen = snprintf(line, sizeof(line), "%d ", y + 1);
        if (linelen > 0) {
            abAppend(&ab, line, linelen);
        }
        
        // 绘制内容
        if (y == 0) {
            drawWelcome(&ab);
        } else {
            abAppend(&ab, "~", 1);
        }
        
        // 清除行尾
        abAppend(&ab, "\x1b[K", 3);
        
        // 如果不是最后一行，添加换行符
        if (y < E.screenrows - 1) {
            abAppend(&ab, "\r\n", 2);
        }
    }
    
    // 显示光标位置
    char status[80];
    int statuslen = snprintf(status, sizeof(status), 
        "[Cursor: %d,%d] [Size: %d×%d]",
        E.cursor_y + 1, E.cursor_x + 1, E.screencols, E.screenrows);
    if (statuslen > E.screencols) statuslen = E.screencols;
    
    // 移动光标到状态行
    abAppend(&ab, "\x1b[999C", 6);  // 移动到行尾
    abAppend(&ab, "\x1b[1A", 4);    // 上移一行
    
    // 显示状态信息
    abAppend(&ab, "\x1b[7m", 4);    
    abAppend(&ab, status, statuslen);
    abAppend(&ab, "\x1b[m", 3);     
    // 移动光标到实际位置
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y + 1, E.cursor_x + 1);
    abAppend(&ab, buf, strlen(buf));
    
    // 显示光标
    abAppend(&ab, "\x1b[?25h", 6);
    
    // 写入屏幕
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// 初始化编辑器
void initEditor() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    
    if (getWindowSize(&E.screenrows, &E.screencols)) die("getWindowSize");
}

int main() {
    // 启用原始模式
    enableRawMode();
    
    // 初始化编辑器
    initEditor();
    
    while (1) {
        refreshScreen();
        
        int c = readKey();
        
        // 退出条件
        if (c == 'q' || c == KEY_ESC) {
            clearScreen();
            break;
        }
        
        // 处理光标移动
        switch (c) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
            case KEY_HOME:
            case KEY_END:
            case KEY_PAGE_UP:
            case KEY_PAGE_DOWN:
                moveCursor(c);
                break;
        }
    }

    printf("终端原始模式已禁用，恢复标准设置。\r\n");
    return 0;
}

