// /*
//  * ============================
//  *        USER'S GUIDE
//  * ============================
//  *
//  * This software interacts via the Serial (SCI) interface and CAN bus.
//  * You can control the software by sending specific key commands through the Serial interface.
//  *
//  * 1. **Entering Numbers**
//  *    - Type numeric characters (e.g., '1', '2', '3') followed by the letter 'e' to process the input.
//  *    - Example:
//  *         Input: 123e
//  *         Output: Entered integer 123: sum = 123, median = 123
//  *
//  * 2. **Resetting History**
//  *    - Press the letter 'F' to clear the last three recorded numbers (history).
//  *    - Example:
//  *         Input: F
//  *         Output: The 3-history has been erased
//  *
//  * 3. **Receiving CAN Messages**
//  *    - CAN messages received will automatically be printed to the Serial interface.
//  *    - Example:
//  *         Output: Can msg received: <message_content>
//  *
//  * 4. **Displaying Input Feedback**
//  *    - Every character you type will be echoed back to the Serial interface.
//  *    - Example:
//  *         Input: 4
//  *         Output: Rcv: '4'
//  *
//  * 5. **Calculation of Sum and Median**
//  *    - After entering up to 3 numbers, the program will maintain a history and calculate:
//  *         - **Sum**: Total of the last 3 numbers.
//  *         - **Median**: Middle value when numbers are sorted.
//  *    - Example:
//  *         Input: 10e
//  *         Input: 20e
//  *         Input: 30e
//  *         Output: Entered integer 30: sum = 60, median = 20
//  *
//  * 6. **Buffer Limit**
//  *    - The maximum length for a single number input is 19 characters. Additional characters will be ignored.
//  */

// #include "TinyTimber.h"
// #include "sciTinyTimber.h"
// #include "canTinyTimber.h"
// #include <stdlib.h>
// #include <stdio.h>

// #define min_index -10
// #define max_index 14
// #define period_size (max_index - min_index + 1)

// typedef struct
// {
//     Object super;
//     int history[3];
//     int history_count;
//     char buffer[20];
//     int buf_index;
//     int freq_index[32];
//     int period[8];
// } App;

// App app = {initObject(), {0, 0, 0}, 0, "", 0};

// void reader(App *, int);
// void receiver(App *, int);
// void process_input(App *, int);

// void freq_index(App *self)
// {
//     int melody[32] = {
//         0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0,
//     };

//     for (int i = 0; i < 32; i++)
//     {
//         self->freq_index[i] = melody[i];
//     };
// }

// void period(App *self)
// {
//     int precompute_period[period_size] = {
//         2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136,
//         1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

//     for (int i = 0; i < period_size; i++)
//     {
//         self->period[i] = precompute_period[i];
//     }
// }

// Serial sci0 = initSerial(SCI_PORT0, &app, reader);
// Can can0 = initCan(CAN_PORT0, &app, receiver);

// void receiver(App *self, int unused)
// {
//     CANMsg msg;
//     CAN_RECEIVE(&can0, &msg);
//     SCI_WRITE(&sci0, "Can msg received: ");
//     SCI_WRITE(&sci0, msg.buff);
// }

// void reader(App *self, int c)
// {
//     if (c == 'f')
//     {
//         SCI_WRITE(&sci0, "Rcv: 'f'\n");
//         self->history_count = 0;
//         SCI_WRITE(&sci0, "The 3-history has been erased\n");
//         return;
//     }

//     if (c == 'e')
//     {
//         SCI_WRITE(&sci0, "Rcv: 'e'\n");
//         self->buffer[self->buf_index] = '\0';
//         int num = atoi(self->buffer); // convert string into int
//         self->buf_index = 0;          // when conversation finish, reset the index to 0

//         process_input(self, num);

//         return;
//     }

//     if (self->buf_index < sizeof(self->buffer) - 1)
//     {
//         self->buffer[self->buf_index++] = (char)c;
//     }

//     SCI_WRITE(&sci0, "Rcv: '");
//     SCI_WRITECHAR(&sci0, c);
//     SCI_WRITE(&sci0, "'\n");
// }

// void process_input(App *self, int num)
// {

//     if (self->history_count < 3)
//     {
//         self->history[self->history_count++] = num;
//     }
//     else
//     {

//         self->history[0] = self->history[1];
//         self->history[1] = self->history[2];
//         self->history[2] = num;
//     }

//     int sum = 0;
//     for (int i = 0; i < self->history_count; i++)
//     {
//         sum += self->history[i];
//     }

//     int median;
//     if (self->history_count == 1)
//     {
//         median = self->history[0];
//     }
//     else if (self->history_count == 2)
//     {
//         median = (self->history[0] + self->history[1]) / 2;
//     }
//     else
//     {

//         // bubble sort
//         int order[3];
//         for (int i = 0; i < 3; i++)
//             order[i] = self->history[i];
//         for (int i = 0; i < 2; i++)
//         {
//             for (int j = i + 1; j < 3; j++)
//             {
//                 if (order[i] > order[j])
//                 {
//                     int temp = order[i];
//                     order[i] = order[j];
//                     order[j] = temp;
//                 }
//             }
//         }
//         median = order[1];
//     }

//     char output[100];
//     snprintf(output, sizeof(output), "Entered integer %d: sum = %d, median = %d\n", num, sum, median);
//     SCI_WRITE(&sci0, output);
// }

// void startApp(App *self, int arg)
// {
//     CANMsg msg;

//     CAN_INIT(&can0);
//     SCI_INIT(&sci0);
//     SCI_WRITE(&sci0, "Hello, hello...\n");

//     msg.msgId = 1;
//     msg.nodeId = 1;
//     msg.length = 6;
//     msg.buff[0] = 'H';
//     msg.buff[1] = 'e';
//     msg.buff[2] = 'l';
//     msg.buff[3] = 'l';
//     msg.buff[4] = 'o';
//     msg.buff[5] = 0;
//     CAN_SEND(&can0, &msg);
// }

// void output_period(App *self)
// {
//     for (int i = 0; i < period_size; i++)
//     {
//         printf("%d ", self->period[i]);
//     }
// }

// int main()
// {
//     INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
//     INSTALL(&can0, can_interrupt, CAN_IRQ0);
//     TINYTIMBER(&app, startApp, 0);
//     period(&app);
//     output_period(&app);
//     return 0;
// }

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include <stdio.h>
#include <math.h>

#define MIN_INDEX -5
#define MAX_INDEX 5

// 示例的频率索引数据（假设这是给定的频率）
int fre_index[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0,
                     7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

// period 数组：每个 key 对应一组 32 个 period 值
double periods[MAX_INDEX - MIN_INDEX + 1][32];

// 串口对象
Serial sci0;

// 预计算所有 period 值
void pre_compute()
{
    for (int k = MIN_INDEX; k <= MAX_INDEX; k++)
    {
        int key_index = k - MIN_INDEX; // 计算当前key的索引
        for (int i = 0; i < 32; i++)
        {
            // 计算每个频率对应的周期值
            periods[key_index][i] = 1.0 / (2 * 440.0 * pow(2, fre_index[i] / 12.0));

            // 输出调试信息，查看计算结果
            char output[50]; // 缩小输出缓冲区
            snprintf(output, sizeof(output), "Debug: key_index = %d, Index %d: Period = %.6f\n", key_index, i, periods[key_index][i]);
            SCI_WRITE(&sci0, output); // 将计算结果通过串口输出

            // 限制输出频率，避免过多输出导致缓冲区溢出
            if (i % 8 == 0)
            {
                SCI_WRITE(&sci0, "\n");
            }
        }
    }
}

// main 函数，初始化串口并调用 pre_compute
int main()
{
    // 初始化串口
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Starting period calculations...\n");

    // 调用预计算函数
    pre_compute();

    SCI_WRITE(&sci0, "Period calculations completed.\n");

    return 0;
}