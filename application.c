#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "core_cm4.h"  // 用于访问 DWT 寄存器（针对 Cortex-M4）

#define min_index -10
#define max_index 14
#define period_size (max_index - min_index + 1)
#define DAC_Address (*(volatile uint8_t*) 0x4000741C)

#define NUM_MEASUREMENTS 500
#define F_CPU 168000000UL  // 假定 CPU 频率为 168MHz

// 对象和任务结构定义
typedef struct {
    Object super;
    int history[3];
    int history_count;
    char buffer[20];
    int buf_index;
    int freq_index[32];
    int period[period_size];
	int current_key;
} App;

// ToneGenerator 增加了 current_period 和 playing 字段，用于旋律播放
typedef struct {
    Object super;
    int volume;
    int muted;
    int state;
    int current_period;  // 当前音符的周期（微秒）
    int playing;         // 1：播放，0：静音
} ToneGenerator;

typedef struct {
    Object super;
    int background_loop_range;  // 模拟负载的循环次数
    int deadline;
} BackgroundTask;

// 全局对象实例（注意：这里将背景负载初始值设为 1000，便于 load_task 调整）
App app = { initObject(), {0, 0, 0}, 0, "", 0 };
ToneGenerator toneGen = { initObject(), 15, 0, 0, 0, 0 };
BackgroundTask bgTask = { initObject(), 1000, 1 };

// 函数原型声明
void reader(App *, int);
void receiver(App *, int);
void process_input(App *, int);
void increase_volume(ToneGenerator*, int);
void decrease_volume(ToneGenerator*, int);
void toggle_mute(ToneGenerator*, int);
void increase_load(BackgroundTask*, int);
void decrease_load(BackgroundTask*, int);
void toggle_deadline(BackgroundTask*, int);
void freq_index(App *);
void init_period(App *);
int get_period_index(App *, int);
void get_period_key(App *, int);
void init_dwt(void);
void measure_background(BackgroundTask *, int);
void measure_tone(ToneGenerator *, int);
void load_task(BackgroundTask *, int);
void generate_tone(ToneGenerator *, int);
void play_melody(App *, int);
void note_gap(App *, int);
void startApp(App *, int);

// 根据预定义的旋律数组初始化频率索引
void freq_index(App *self)
{
    int melody[32] = {
        0, 2, 4, 0, 0, 2, 4, 0,
        4, 5, 7, 4, 5, 7, 7, 9,
        7, 5, 4, 0, 7, 9, 7, 5,
        4, 0, 0, -5, 0, 0, -5, 0
    };
    for (int i = 0; i < 32; i++) {
        self->freq_index[i] = melody[i];
    }
}

// 初始化预先计算好的周期数组
void init_period(App *self)
{
    int precompute_period[period_size] = {
        506, 536, 568, 601, 637, 675, 715, 758,
        803, 851, 901, 955, 1012, 1072, 1136, 1203,
        1275, 1351, 1431, 1516, 1607, 1702, 1803, 1911, 2024
    };
    for (int i = 0; i < period_size; i++) {
        self->period[i] = precompute_period[i];
    }
}

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

// 接收 CAN 消息的回调函数
void receiver(App *self, int unused)
{
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

// 处理 SCI 输入
void reader(App *self, int c)
{
    switch (c) {
        case 'e':
            SCI_WRITE(&sci0, "Rcv: 'e'\n");
            self->buffer[self->buf_index] = '\0';
            {
                int num = atoi(self->buffer);
                self->buf_index = 0;
                process_input(self, num);
            }
            break;
        case 'u':
            ASYNC(&toneGen, increase_volume, 0);
            SCI_WRITE(&sci0, "Volume Up\n");
            break;
        case 'd':
            ASYNC(&toneGen, decrease_volume, 0);
            SCI_WRITE(&sci0, "Volume Down\n");
            break;
        case 'm':
            ASYNC(&toneGen, toggle_mute, 0);
            SCI_WRITE(&sci0, "Mute\n");
            break;
        case '+':
            ASYNC(&bgTask, increase_load, 0);
            break;
        case '-':
            ASYNC(&bgTask, decrease_load, 0);
            break;
        case 't':
            ASYNC(&bgTask, toggle_deadline, 0);
            break;
        default:
            if (self->buf_index < sizeof(self->buffer) - 1) {
                self->buffer[self->buf_index++] = c;
            }
            SCI_WRITE(&sci0, "Rcv: '");
            SCI_WRITECHAR(&sci0, c);
            SCI_WRITE(&sci0, "'\n");
            break;
    }
}

// 根据键值（key）和频率索引确定对应的周期数组下标
int get_period_index(App *self, int k)
{
    if (k < min_index || k > max_index)
        return -1;
    else
        return k - min_index;
}

// 根据键值（key）打印当前音调的周期序列
void get_period_key(App *self, int key)
{
    char keyBuffer[10];
    snprintf(keyBuffer, sizeof(keyBuffer), "Key: %d\n", key);
    SCI_WRITE(&sci0, keyBuffer);
    
    for (int i = 0; i < 32; i++) {
        int k = self->freq_index[i] + key;
        int period_index = get_period_index(self, k);
        char buffer[10];
        if (period_index != -1)
            snprintf(buffer, sizeof(buffer), "%d ", self->period[period_index]);
        else
            snprintf(buffer, sizeof(buffer), "N/A ");
        SCI_WRITE(&sci0, buffer);
    }
    SCI_WRITE(&sci0, "\n");
}

// 根据输入数字处理键盘命令
void process_input(App *self, int num)
{
    if (num <= 5 && num >= -5) {
		self->current_key = num;
        get_period_key(self, num);
    } else {
        SCI_WRITE(&sci0, "Invalid input. Please enter a number between -5 to 5.\n");
    }
}

// ToneGenerator 控制函数
void increase_volume(ToneGenerator *self, int unused)
{
    if (self->volume < 20) {
        self->volume += 1;
        SCI_WRITE(&sci0, "Increased Volume\n");
    } else {
        SCI_WRITE(&sci0, "Max Volume Already!\n");
    }
}

void decrease_volume(ToneGenerator *self, int unused)
{
    if (self->volume > 1) {
        self->volume -= 1;
        SCI_WRITE(&sci0, "Decreased Volume\n");
    } else {
        SCI_WRITE(&sci0, "Min Volume Already!\n");
    }
}

void toggle_mute(ToneGenerator *self, int unused)
{
    self->muted = !self->muted;
    if (self->muted)
        SCI_WRITE(&sci0, "Muted!\n");
    else
        SCI_WRITE(&sci0, "Unmuted!\n");
}

// BackgroundTask 控制函数
void increase_load(BackgroundTask *self, int unused)
{
    if (self->background_loop_range + 500 <= 8000) {
        self->background_loop_range += 500;
        char msg[50];
        snprintf(msg, sizeof(msg), "Increased load: %d\n", self->background_loop_range);
        SCI_WRITE(&sci0, msg);
    } else {
        SCI_WRITE(&sci0, "Max load Already!\n");
    }
}

void decrease_load(BackgroundTask *self, int unused)
{
    if (self->background_loop_range - 500 >= 1000) {
        self->background_loop_range -= 500;
        char msg[50];
        snprintf(msg, sizeof(msg), "Decreased load: %d\n", self->background_loop_range);
        SCI_WRITE(&sci0, msg);
    } else {
        SCI_WRITE(&sci0, "Min load Already!\n");
    }
}

void toggle_deadline(BackgroundTask *self, int unused)
{
    self->deadline = !self->deadline;
    if (self->deadline)
        SCI_WRITE(&sci0, "Deadline Enabled\n");
    else
        SCI_WRITE(&sci0, "Deadline Disabled\n");
}

// 初始化 DWT 计数器，用于高精度计时
void init_dwt(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// 测量背景任务空循环的 WCET
// 测量背景任务空循环的 WCET（单位：ns）
// 测量背景任务空循环的 WCET（单位：ns）
void measure_background(BackgroundTask *self, int unused)
{
    uint32_t max_cycles = 0;
    uint64_t total_cycles = 0;
    for (int j = 0; j < NUM_MEASUREMENTS; j++) {
        uint32_t start = DWT->CYCCNT;
        for (volatile int i = 0; i < self->background_loop_range; i++) {
            // 空循环模拟负载
        }
        uint32_t end = DWT->CYCCNT;
        uint32_t cycles = end - start;
        if (cycles > max_cycles)
            max_cycles = cycles;
        total_cycles += cycles;
    }
    // 将 cycles 转换为纳秒: ns = (cycles * 1e9) / F_CPU
    uint32_t max_ns = (uint32_t)(((uint64_t)max_cycles * 1000000000UL) / F_CPU);
    uint32_t avg_ns = (uint32_t)((((uint64_t)total_cycles / NUM_MEASUREMENTS) * 1000000000UL) / F_CPU);
    char msg[100];
    snprintf(msg, sizeof(msg), "Background Task WCET: max = %lu ns, avg = %lu ns\n",
             (unsigned long)max_ns, (unsigned long)avg_ns);
    SCI_WRITE(&sci0, msg);
}

// 测量 ToneGenerator 写 DAC 部分的 WCET
void measure_tone(ToneGenerator *self, int unused)
{
    uint32_t max_cycles = 0;
    uint64_t total_cycles = 0;
    for (int j = 0; j < NUM_MEASUREMENTS; j++) {
        uint32_t start = DWT->CYCCNT;
        if (self->muted) {
            DAC_Address = 0;
        } else {
            if (self->state == 0)
                DAC_Address = self->volume;
            else
                DAC_Address = 0;
            self->state = !self->state;
        }
        uint32_t end = DWT->CYCCNT;
        uint32_t cycles = end - start;
        if (cycles > max_cycles)
            max_cycles = cycles;
        total_cycles += cycles;
    }
    // 将 cycles 转换为纳秒: 每个周期占用的时间 = (1e9 / F_CPU) ns
    // 为避免乘法溢出，这里先将 max_cycles 转为 64 位计算
    uint32_t max_ns = (uint32_t)(((uint64_t)max_cycles * 1000000000UL) / F_CPU);
    uint32_t avg_ns = (uint32_t)((((uint64_t)total_cycles / NUM_MEASUREMENTS) * 1000000000UL) / F_CPU);
    char msg[100];
    snprintf(msg, sizeof(msg), "Tone Generator WCET: max = %lu ns, avg = %lu ns\n",
             (unsigned long)max_ns, (unsigned long)avg_ns);
    SCI_WRITE(&sci0, msg);
}

// 背景负载任务：模拟工作负载并根据 deadline 模式调度自身
void load_task(BackgroundTask *self, int unused)
{
    for (volatile int i = 0; i < self->background_loop_range; i++) {
        // 模拟负载循环
    }
    if (self->deadline) {
        SEND(USEC(1300), USEC(1300), self, load_task, 0);
    } else {
        AFTER(USEC(1300), self, load_task, 0);
    }
}

// 背景负载任务：模拟工作负载并根据 deadline 模式调度自身


// ToneGenerator 任务：根据 playing 标志和当前音符周期控制 DAC 输出
void generate_tone(ToneGenerator *self, int unused)
{
    if (!self->playing) {
        DAC_Address = 0;
    } else {
        if (self->muted) {
            DAC_Address = 0;
        } else {
            if (self->state == 0)
                DAC_Address = self->volume;
            else
                DAC_Address = 0;
            self->state = !self->state;
        }
    }
    // 使用当前音符周期作为延时，否则使用默认 500 微秒
    unsigned int delay = self->playing ? self->current_period : 500;
    if (bgTask.deadline) {
        SEND(delay, delay, self, generate_tone, 0);
    } else {
        AFTER(delay, self, generate_tone, 0);
    }
}

// 播放旋律：按顺序依次播放 32 个音符
void play_melody(App *self, int note_index)
{
    // 旋律中每个音符的时长模式（单位：拍）
    static float note_pattern[32] = {
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 2, 1, 1, 2, 0.5, 0.5,
        0.5, 0.5, 1, 1, 0.5, 0.5, 0.5, 0.5,
        1, 1, 1, 1, 2, 1, 1, 2
    };
    int tempo = 120; // 默认 120 bpm
    // 计算一拍的时长（微秒）
    unsigned int beat_duration = 60000000 / tempo;
    unsigned int gap = 50000; // 50 ms 静音间隔
    float beat_factor = note_pattern[note_index];
    unsigned int note_duration = (unsigned int)(beat_duration * beat_factor);
    // 播放时实际发声时长为 note_duration 减去 gap
    unsigned int tone_duration = (note_duration > gap) ? note_duration - gap : note_duration;
    
    // 根据当前音符计算频率（默认 key=0）
    int freq_offset = self->freq_index[note_index];
    int key = self->current_key;
    int computed_index = freq_offset + key;
    int period_index = get_period_index(self, computed_index);
    if (period_index != -1)
        toneGen.current_period = self->period[period_index];
    else
        toneGen.current_period = 0;
    
    toneGen.playing = 1;  // 开始播放当前音符
    {
        char msg[80];
        snprintf(msg, sizeof(msg), "Playing note %d: period = %d, tone duration = %u us\n",
                 note_index, toneGen.current_period, tone_duration);
        SCI_WRITE(&sci0, msg);
    }
    // 安排在 tone_duration 后关闭当前音符（产生静音间隔）
    AFTER(tone_duration, self, note_gap, note_index);
}

// 关闭当前音符后产生静音，并调度下一音符播放
void note_gap(App *self, int note_index)
{
    toneGen.playing = 0;
    SCI_WRITE(&sci0, "Note gap\n");
    int gap = 50000;  // 50 ms gap
    int next_index = (note_index + 1) % 32;
    AFTER(gap, self, play_melody, next_index);
}

// 应用程序入口，完成初始化、测量和任务调度
void startApp(App *self, int arg)
{
    CANMsg msg;
    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    init_period(self);
    freq_index(self);
    get_period_key(self, 0);
    
    // 初始化 DWT 计数器，用于高精度测量
    init_dwt();
    
    SCI_WRITE(&sci0, "Measuring Background Task WCET...\n");
    measure_background(&bgTask, 0);
//	SCI_WRITE(&sci0, "Measuring Tone Generator WCET...\n");
//    measure_tone(&toneGen, 0);
    // 根据 deadline 模式安排 toneGen 与背景负载任务
    
        //ASYNC(&toneGen, generate_tone, 0);
        //ASYNC(&bgTask, load_task, 0);
    
    
    // 启动旋律播放（循环 32 个音符）
   // ASYNC(self, play_melody, 0);
    
    // 发送一条简单的 CAN 消息
    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
}

int main()
{
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
