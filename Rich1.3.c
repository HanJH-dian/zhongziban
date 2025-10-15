#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// 定义常量
#define MAP_ROWS 8
#define MAP_COLS 30
#define MAX_PLAYERS 4
#define MAX_PROPERTIES 10
#define MAX_ITEMS 10

// 玩家结构体
typedef struct {
    char name[20];
    char symbol;
    int money;
    int points;
    int position;
    int properties[MAX_PROPERTIES];
    int property_count;
    int items[MAX_ITEMS]; // 0:无 1:路障 2:机器娃娃 3:炸弹
    int item_count;
    int hospitalized;
    int imprisoned;
    int god_mode; // 财神附身
} Player;

// 地图格子类型
typedef struct {
    char type; // S:起点 O:空地 T:道具屋 G:礼品屋 M:魔法屋 H:医院 P:监狱 $:矿地
    int owner; // -1:无主, 0-3:玩家索引
    int level; // 0:空地, 1:茅屋, 2:洋房, 3:摩天楼
    int price;
    int toll; // 过路费
    int has_item; // 是否有道具
    int item_type; // 道具类型
} Cell;

// 游戏状态
Cell map[MAP_ROWS][MAP_COLS];
Player players[MAX_PLAYERS];
int player_count;
int current_player;
int game_over;

// 初始化地图为方形边界
void init_map() {
    // 初始化所有格子为'-'
    for (int i = 0; i < MAP_ROWS; i++) {
        for (int j = 0; j < MAP_COLS; j++) {
            map[i][j].type = ' ';
            map[i][j].owner = -1;
            map[i][j].level = 0;
            map[i][j].has_item = 0;
            map[i][j].price = 0;
            map[i][j].toll = 0;
        }
    }
    
    // 设置边界路径
    // 顶部行
    for (int j = 0; j < MAP_COLS; j++) {
        map[0][j].type = 'O';
        map[0][j].price = 200;
        map[0][j].toll = 100;
    }
    
    // 底部行
    for (int j = 0; j < MAP_COLS; j++) {
        map[MAP_ROWS-1][j].type = 'O';
        map[MAP_ROWS-1][j].price = 300;
        map[MAP_ROWS-1][j].toll = 150;
    }
    
    // 左侧列
    for (int i = 1; i < MAP_ROWS-1; i++) {
        map[i][0].type = 'O';
        map[i][0].price = 200;
        map[i][0].toll = 100;
    }
    
    // 右侧列
    for (int i = 1; i < MAP_ROWS-1; i++) {
        map[i][MAP_COLS-1].type = 'O';
        map[i][MAP_COLS-1].price = 500;
        map[i][MAP_COLS-1].toll = 250;
    }
    
    // 设置特殊地点
    map[0][0].type = 'S'; // 起点
    map[0][15].type = 'H'; // 医院
    map[0][MAP_COLS-1].type = 'T'; // 道具屋
    
    map[MAP_ROWS-1][0].type = 'M'; // 魔法屋
    map[MAP_ROWS-1][15].type = 'P'; // 监狱
    map[MAP_ROWS-1][MAP_COLS-1].type = 'G'; // 礼品屋
    
    // 设置矿地
    map[1][0].type = '$';
    map[2][0].type = '$';
    map[3][0].type = '$';
    map[4][0].type = '$';
    map[5][0].type = '$';
    map[6][0].type = '$';
}

// 初始化玩家
void init_players(int count, int initial_money) {
    player_count = count;
    
    // 初始化玩家属性
    for (int i = 0; i < count; i++) {
        players[i].money = initial_money;
        players[i].points = 0;
        players[i].position = 0;
        players[i].property_count = 0;
        players[i].item_count = 0;
        players[i].hospitalized = 0;
        players[i].imprisoned = 0;
        players[i].god_mode = 0;
        
        for (int j = 0; j < MAX_PROPERTIES; j++) {
            players[i].properties[j] = -1;
        }
        
        for (int j = 0; j < MAX_ITEMS; j++) {
            players[i].items[j] = 0;
        }
    }
    
    // 设置玩家名称和符号
    strcpy(players[0].name, "钱夫人");
    players[0].symbol = 'Q';
    
    strcpy(players[1].name, "阿土伯");
    players[1].symbol = 'A';
    
    strcpy(players[2].name, "孙小美");
    players[2].symbol = 'X';
    
    strcpy(players[3].name, "金贝贝");
    players[3].symbol = 'J';
    
    current_player = 0;
    game_over = 0;
}

// 将一维位置转换为二维坐标,确保了玩家沿着矩形边界顺时针移动，符合大富翁游戏的传统玩法
void position_to_coord(int position, int *row, int *col) {
    int total_cells = 2 * (MAP_ROWS + MAP_COLS - 2); // 边界上的总格子数
    
    position = position % total_cells;
    
    if (position < MAP_COLS) {
        // 顶部行
        *row = 0;
        *col = position;
    } else if (position < MAP_COLS + MAP_ROWS - 2) {
        // 右侧列
        *row = position - MAP_COLS + 1;
        *col = MAP_COLS - 1;
    } else if (position < 2 * MAP_COLS + MAP_ROWS - 2) {
        // 底部行（从右到左）
        *row = MAP_ROWS - 1;
        *col = MAP_COLS - 1 - (position - (MAP_COLS + MAP_ROWS - 2));
    } else {
        // 左侧列（从下到上）
        *row = MAP_ROWS - 1 - (position - (2 * MAP_COLS + MAP_ROWS - 2)) - 1;
        *col = 0;
    }
}

// 显示地图
void display_map() {
    printf("\n当前地图状态:\n");
    printf("------------------------------------------------------------\n");
    
    for (int i = 0; i < MAP_ROWS; i++) {
        for (int j = 0; j < MAP_COLS; j++) {
            // 检查是否有玩家在此位置
            int player_here = -1;
            for (int k = 0; k < player_count; k++) {
                int row, col;
                position_to_coord(players[k].position, &row, &col);
                if (row == i && col == j) {
                    player_here = k;
                    break;
                }
            }
            
            if (player_here != -1) {
                switch (player_here)
                {
                case 0:
                    printf("\033[1;31m%c\033[0m", players[player_here].symbol);
                    break;
                case 1:
                    printf("\033[1;32m%c\033[0m", players[player_here].symbol);
                    break;
                case 2:
                    printf("\033[1;34m%c\033[0m", players[player_here].symbol); 
                    break;
                case 3:
                    printf("\033[1;33m%c\033[0m", players[player_here].symbol); 
                    break;
                default:
                    break;
                }
                
            } else if (map[i][j].has_item) {
                switch (map[i][j].item_type) {
                    case 1: printf("#"); break; // 路障
                    case 2: printf("@"); break; // 机器娃娃
                    case 3: printf("@"); break; // 炸弹
                    default: printf("?");
                }
            } else {
                switch (map[i][j].type) {
                    case 'S': printf("S"); break;
                    case 'O': 
                        if (map[i][j].owner == -1) {
                            printf("O");
                        } else {
                            printf("%d", map[i][j].level);
                        }
                        break;
                    case 'T': printf("T"); break;
                    case 'G': printf("G"); break;
                    case 'M': printf("M"); break;
                    case 'H': printf("H"); break;
                    case 'P': printf("P"); break;
                    default: printf("%c", map[i][j].type);
                }
            }
        }
        printf("\n");
    }
    
    printf("------------------------------------------------------------\n");
    printf("图例: S-起点 O-空地 T-道具屋 G-礼品屋 M-魔法屋/矿地 H-医院 P-监狱\n");
    printf("      数字-地产等级(0-3) Q-钱夫人 A-阿土伯 S-孙小美 J-金贝贝\n");
    printf("      #-路障 @-炸弹/机器娃娃\n");
}

// 显示玩家状态
void display_player_status(int player_index) {
    printf("\n%s 的状态:\n", players[player_index].name);
    printf("资金: %d元\n", players[player_index].money);
    printf("点数: %d点\n", players[player_index].points);
    
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    printf("位置: (%d, %d)\n", row, col);
    
    printf("地产: %d处\n", players[player_index].property_count);
    printf("道具: %d个\n", players[player_index].item_count);
    
    if (players[player_index].hospitalized > 0) {
        printf("状态: 住院中 (%d回合后出院)\n", players[player_index].hospitalized);
    } else if (players[player_index].imprisoned > 0) {
        printf("状态: 监禁中 (%d回合后释放)\n", players[player_index].imprisoned);
    } else if (players[player_index].god_mode > 0) {
        printf("状态: 财神附身 (%d回合有效)\n", players[player_index].god_mode);
    } else {
        printf("状态: 正常\n");
    }
}

// 掷骰子
int roll_dice() {
    return rand() % 6 + 1;
}

// 移动玩家
void move_player(int player_index, int steps) {
    players[player_index].position = (players[player_index].position + steps) % (2 * (MAP_ROWS + MAP_COLS - 2));
    
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    printf("%s 移动了 %d 步，到达位置 (%d, %d)\n", 
           players[player_index].name, steps, row, col);
}

// 购买地产
void buy_property(int player_index) {
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    
    if (map[row][col].type != 'O') {
        printf("此处不能购买地产\n");
        return;
    }
    
    if (map[row][col].owner != -1) {
        printf("此地已有主人\n");
        return;
    }
    
    if (players[player_index].money >= map[row][col].price) {
        players[player_index].money -= map[row][col].price;
        map[row][col].owner = player_index;
        players[player_index].properties[players[player_index].property_count++] = players[player_index].position;
        printf("%s 购买了位置 (%d, %d) 的地产，花费 %d元\n", 
               players[player_index].name, row, col, map[row][col].price);
    } else {
        printf("资金不足，无法购买此地产\n");
    }
}

// 升级地产
void upgrade_property(int player_index) {
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    
    if (map[row][col].owner != player_index) {
        printf("这不是你的地产\n");
        return;
    }
    
    if (map[row][col].level < 3 && players[player_index].money >= map[row][col].price) {
        players[player_index].money -= map[row][col].price;
        map[row][col].level++;
        printf("%s 升级了位置 (%d, %d) 的地产，现在是 %d 级，花费 %d元\n", 
               players[player_index].name, row, col, map[row][col].level, map[row][col].price);
    } else {
        printf("无法升级此地产\n");
    }
}

// 支付过路费
void pay_toll(int player_index) {
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    
    int owner = map[row][col].owner;
    int toll = map[row][col].toll * (map[row][col].level + 1);
    
    if (players[player_index].god_mode > 0) {
        printf("财神附身，免付过路费\n");
        return;
    }
    
    if (players[owner].hospitalized > 0 || players[owner].imprisoned > 0) {
        printf("地主正在医院或监狱中，免付过路费\n");
        return;
    }
    
    if (players[player_index].money >= toll) {
        players[player_index].money -= toll;
        players[owner].money += toll;
        printf("%s 向 %s 支付了过路费 %d元\n", 
               players[player_index].name, players[owner].name, toll);
    } else {
        printf("%s 资金不足，无法支付过路费，破产了！\n", players[player_index].name);
        game_over = 1;
    }
}

// 处理玩家到达的位置
void handle_position(int player_index) {
    int row, col;
    position_to_coord(players[player_index].position, &row, &col);
    Cell *cell = &map[row][col];
    
    printf("%s 到达了位置 (%d, %d): ", players[player_index].name, row, col);
    
    switch (cell->type) {
        case 'S':
            printf("起点\n");
            break;
            
        case 'O':
            if (cell->owner == -1) {
                printf("空地，可以购买\n");
                printf("购买价格: %d元\n", cell->price);
                printf("是否购买? (y/n): ");
                char choice;
                scanf(" %c", &choice);
                if (tolower(choice) == 'y') {
                    buy_property(player_index);
                }
            } else if (cell->owner == player_index) {
                printf("自己的地产，可以升级\n");
                printf("升级价格: %d元\n", cell->price);
                printf("是否升级? (y/n): ");
                char choice;
                scanf(" %c", &choice);
                if (tolower(choice) == 'y') {
                    upgrade_property(player_index);
                }
            } else {
                printf("%s 的地产，需要支付过路费\n", players[cell->owner].name);
                printf("过路费: %d元\n", cell->toll * (cell->level + 1));
                pay_toll(player_index);
            }
            break;
            
        case 'T':
            printf("道具屋\n");
            // 简化版：随机获得一个道具
            int item = rand() % 3 + 1;
            if (players[player_index].item_count < MAX_ITEMS) {
                players[player_index].items[players[player_index].item_count++] = item;
                const char *item_name = "";
                switch (item) {
                    case 1: item_name = "路障"; break;
                    case 2: item_name = "机器娃娃"; break;
                    case 3: item_name = "炸弹"; break;
                }
                printf("获得了 %s\n", item_name);
            } else {
                printf("道具栏已满，无法获得新道具\n");
            }
            break;
            
        case 'G':
            printf("礼品屋\n");
            // 简化版：随机获得一个礼品
            int gift = rand() % 3 + 1;
            switch (gift) {
                case 1:
                    players[player_index].money += 2000;
                    printf("获得了 2000元奖金\n");
                    break;
                case 2:
                    players[player_index].points += 200;
                    printf("获得了 200点\n");
                    break;
                case 3:
                    players[player_index].god_mode = 5;
                    printf("获得了财神附身，5回合有效\n");
                    break;
            }
            break;
            
        case 'M':
            if (row == 0 || row == MAP_ROWS-1 || col == 0 || col == MAP_COLS-1) {
                printf("魔法屋\n");
                // 简化版：随机获得或失去一些资源
                int effect = rand() % 3;
                switch (effect) {
                    case 0:
                        players[player_index].money += 1000;
                        printf("获得了 1000元\n");
                        break;
                    case 1:
                        players[player_index].points += 100;
                        printf("获得了 100点\n");
                        break;
                    case 2:
                        players[player_index].money -= 500;
                        printf("失去了 500元\n");
                        break;
                }
            } else {
                printf("矿地\n");
                // 获得点数
                int points = 20 + rand() % 80;
                players[player_index].points += points;
                printf("获得了 %d 点\n", points);
            }
            break;
            
        case 'H':
            printf("医院\n");
            if (players[player_index].hospitalized == 0) {
                printf("只是路过医院\n");
            } else {
                printf("正在医院接受治疗\n");
            }
            break;
            
        case 'P':
            printf("监狱\n");
            if (players[player_index].imprisoned == 0) {
                printf("只是路过监狱\n");
            } else {
                printf("正在监狱服刑\n");
            }
            break;
            
        default:
            printf("未知地点\n");
            break;
    }
    
    // 检查是否有道具效果
    if (cell->has_item) {
        printf("触发了道具效果: ");
        switch (cell->item_type) {
            case 1: // 路障
                printf("被路障拦截，停止一回合\n");
                // 简化版：跳过下一回合
                break;
            case 3: // 炸弹
                printf("被炸弹炸伤，住院3天\n");
                players[player_index].hospitalized = 3;
                break;
        }
        cell->has_item = 0; // 移除道具
    }
}

// 使用路障
void use_block(int player_index, int distance) {
    if (players[player_index].item_count > 0) {
        int has_block = 0;
        for (int i = 0; i < players[player_index].item_count; i++) {
            if (players[player_index].items[i] == 1) {
                has_block = 1;
                // 移除道具
                for (int j = i; j < players[player_index].item_count - 1; j++) {
                    players[player_index].items[j] = players[player_index].items[j+1];
                }
                players[player_index].item_count--;
                break;
            }
        }
        
        if (has_block) {
            int target_pos = (players[player_index].position + distance + (2 * (MAP_ROWS + MAP_COLS - 2))) % (2 * (MAP_ROWS + MAP_COLS - 2));
            int row, col;
            position_to_coord(target_pos, &row, &col);
            map[row][col].has_item = 1;
            map[row][col].item_type = 1;
            printf("在位置 (%d, %d) 放置了路障\n", row, col);
        } else {
            printf("没有路障道具\n");
        }
    } else {
        printf("没有可用道具\n");
    }
}

// 使用炸弹
void use_bomb(int player_index, int distance) {
    if (players[player_index].item_count > 0) {
        int has_bomb = 0;
        for (int i = 0; i < players[player_index].item_count; i++) {
            if (players[player_index].items[i] == 3) {
                has_bomb = 1;
                // 移除道具
                for (int j = i; j < players[player_index].item_count - 1; j++) {
                    players[player_index].items[j] = players[player_index].items[j+1];
                }
                players[player_index].item_count--;
                break;
            }
        }
        
        if (has_bomb) {
            int target_pos = (players[player_index].position + distance + (2 * (MAP_ROWS + MAP_COLS - 2))) % (2 * (MAP_ROWS + MAP_COLS - 2));
            int row, col;
            position_to_coord(target_pos, &row, &col);
            map[row][col].has_item = 1;
            map[row][col].item_type = 3;
            printf("在位置 (%d, %d) 放置了炸弹\n", row, col);
        } else {
            printf("没有炸弹道具\n");
        }
    } else {
        printf("没有可用道具\n");
    }
}

// 使用机器娃娃
void use_robot(int player_index) {
    if (players[player_index].item_count > 0) {
        int has_robot = 0;
        for (int i = 0; i < players[player_index].item_count; i++) {
            if (players[player_index].items[i] == 2) {
                has_robot = 1;
                // 移除道具
                for (int j = i; j < players[player_index].item_count - 1; j++) {
                    players[player_index].items[j] = players[player_index].items[j+1];
                }
                players[player_index].item_count--;
                break;
            }
        }
        
        if (has_robot) {
            printf("清除了前方10格内的道具\n");
            for (int i = 1; i <= 10; i++) {
                int target_pos = (players[player_index].position + i) % (2 * (MAP_ROWS + MAP_COLS - 2));
                int row, col;
                position_to_coord(target_pos, &row, &col);
                map[row][col].has_item = 0;
            }
        } else {
            printf("没有机器娃娃道具\n");
        }
    } else {
        printf("没有可用道具\n");
    }
}

// 显示帮助信息
void show_help() {
    printf("\n可用命令:\n");
    printf("roll        - 掷骰子移动\n");
    printf("block n     - 在前后n格放置路障\n");
    printf("bomb n      - 在前后n格放置炸弹\n");
    printf("robot       - 使用机器娃娃清除前方道路\n");
    printf("query       - 查看自己的资产\n");
    printf("map         - 显示地图\n");
    printf("help        - 显示帮助信息\n");
    printf("quit        - 退出游戏\n");
}

// 主游戏循环
void game_loop() {
    srand(time(NULL));
    
    printf("欢迎来到大富翁简化版游戏!\n");
    
    // 设置玩家数量和初始资金
    int initial_money = 10000;
    printf("请输入玩家数量 (2-4): ");
    scanf("%d", &player_count);
    
    if (player_count < 2 || player_count > 4) {
        printf("玩家数量必须在2-4之间，已设置为2\n");
        player_count = 2;
    }
    
    printf("请输入初始资金 (默认10000): ");
    scanf("%d", &initial_money);
    
    if (initial_money < 1000 || initial_money > 50000) {
        printf("初始资金必须在1000-50000之间，已设置为10000\n");
        initial_money = 10000;
    }
    
    // 初始化游戏
    init_map();
    init_players(player_count, initial_money);
    
    printf("游戏开始! 初始资金: %d元\n", initial_money);
    
    // 游戏主循环
    while (!game_over) {
        Player *current = &players[current_player];
        
        // 跳过住院或监禁的玩家
        if (current->hospitalized > 0) {
            printf("\n%s 正在住院，跳过本回合 (%d回合后出院)\n", 
                   current->name, current->hospitalized);
            current->hospitalized--;
            current_player = (current_player + 1) % player_count;
            continue;
        }
        
        if (current->imprisoned > 0) {
            printf("\n%s 正在监禁中，跳过本回合 (%d回合后释放)\n", 
                   current->name, current->imprisoned);
            current->imprisoned--;
            current_player = (current_player + 1) % player_count;
            continue;
        }
        
        // 财神模式递减
        if (current->god_mode > 0) {
            current->god_mode--;
        }
        
        printf("\n轮到 %s 的回合\n", current->name);
        display_player_status(current_player);
        
        char command[20];
        int steps;
        
        while (1) {
            display_map();
            printf("\n请输入命令 (输入help查看帮助): ");
            scanf("%s", command);
            
            if (strcasecmp(command, "step") == 0){
                scanf("%d", &steps);
                printf("移动 %d 步", steps);
                move_player(current_player, steps);
                handle_position(current_player);
                break;
            }
            else if (strcasecmp(command, "roll") == 0) {
                steps = roll_dice();
                printf("掷出了 %d 点\n", steps);
                move_player(current_player, steps);
                handle_position(current_player);
                display_map();
                break;
            } else if (strcasecmp(command, "block") == 0) {
                int distance;
                scanf("%d", &distance);
                if (distance >= -10 && distance <= 10 && distance != 0) {
                    use_block(current_player, distance);
                    display_map();
                } else {
                    printf("距离必须在-10到10之间且不能为0\n");
                }
            } else if (strcasecmp(command, "bomb") == 0) {
                int distance;
                scanf("%d", &distance);
                if (distance >= -10 && distance <= 10 && distance != 0) {
                    use_bomb(current_player, distance);
                    display_map();
                } else {
                    printf("距离必须在-10到10之间且不能为0\n");
                }
            } else if (strcasecmp(command, "robot") == 0) {
                use_robot(current_player);
                display_map();
            } else if (strcasecmp(command, "query") == 0) {
                display_player_status(current_player);
            } else if (strcasecmp(command, "help") == 0) {
                show_help();
            } else if (strcasecmp(command, "quit") == 0) {
                game_over = 1;
                break;
            } else {
                printf("未知命令，请输入help查看帮助\n");
            }
            display_map();
        }
        
        // 检查游戏是否结束
        if (current->money < 0) {
            printf("%s 破产了！游戏结束\n", current->name);
            game_over = 1;
        }
        
        // 切换到下一个玩家
        current_player = (current_player + 1) % player_count;
    }
    
    printf("游戏结束!\n");
}

int main() {
    game_loop();
    return 0;
}