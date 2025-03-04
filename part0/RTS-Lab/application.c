/*
 * User's Guide:
 *
 * 1. Numeric Input:
 *    - You can input an integer between -5 and 5 (e.g., 3 or -2).
 *    - After entering the number, press the 'e' key to finish the input and confirm the number.
 *    - The system will output the corresponding tone information based on the input.
 *
 * 2. Volume Control:
 *    - Press 'u': Increase the volume.
 *    - Press 'd': Decrease the volume.
 *    - Press 'm': Toggle mute/unmute.
 *
 * 3. Background Task Load Adjustment:
 *    - Press '+': Increase the background task load.
 *    - Press '_': Decrease the background task load.
 *
 * 4. Deadline Mode Toggle:
 *    - Press 't': Toggle the deadline mode on or off.
 *
 * 5. CAN and Serial Communication:
 *    - Upon startup, the system sends a CAN message (with the content "Hello").
 *    - Received CAN messages will be displayed via the serial interface (SCI).
 *
 * 6. Additional Notes:
 *    - While entering a number, each character is stored in a buffer until you press 'e' to submit.
 *    - Ensure that the entered number is within the valid range (-5 to 5).
 *    - All operations are executed asynchronously.
 *
 * Follow the above guidelines to enjoy the system experience!
 */

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "system_stm32f4xx.h"

#define min_index -10
#define max_index 14
#define period_size (max_index - min_index + 1)
#define DAC_Address (*(volatile uint8_t*) 0x4000741C)
#define NUM_RUN 500
#define CPU_FREQ_HZ 168000000


typedef struct
{
    Object super;
    int history[3];
    int history_count;
    char buffer[20];
    int buf_index;
    int freq_index[32];
    int period[period_size];
} App;

typedef struct
{
    Object super;
    int volume;
    int muted;
    int state;
} ToneGenerator;

typedef struct {
    Object super;
    int background_loop_range;
    int deadline;
} BackgroundTask;

App app = {initObject(), {0, 0, 0}, 0, "", 0};

ToneGenerator toneGen = {initObject(), 15, 0, 0};

BackgroundTask bgTask = {initObject(), 1000, 1};

void reader(App *, int);
void receiver(App *, int);
void process_input(App *, int);
void increase_volume(ToneGenerator*, int);
void decrease_volume(ToneGenerator*, int);
void toggle_mute(ToneGenerator*, int);
void increase_load(BackgroundTask*, int);
void decrease_load(BackgroundTask*, int);
void toggle_deadline(BackgroundTask*, int);
void measure_background_wcet(BackgroundTask* self, int loop_range);
void measure_generator_wcet(ToneGenerator* self);

void init_dwt(void) {
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

uint32_t get_time(void) {
    return DWT->CYCCNT;
}


//create original melody tone
void freq_index(App *self)
{
    int melody[32] = {
        0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0,
    };

    for (int i = 0; i < 32; i++)
    {
        self->freq_index[i] = melody[i];
    };
}

//create original melody frequency matched with original melody tone
void init_period(App *self)
{
    int precompute_period[period_size] = {
        2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136,
        1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

    for (int i = 0; i < period_size; i++)
    {
        self->period[i] = precompute_period[i];
    }
}


Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

void receiver(App *self, int unused)
{
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c)
{
    switch (c) {
        case 'e':
            SCI_WRITE(&sci0, "Rcv: 'e'\n");
            self->buffer[self->buf_index] = '\0';
            int num = atoi(self->buffer); // convert parameter from char to integer
            self->buf_index = 0;          // after pressing 'e', reset the index to 0
            process_input(self, num);     // transfer the num to function process_input
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
            
        case '_':
            ASYNC(&bgTask, decrease_load, 0);
            break;
        case 't':  
            ASYNC(&bgTask, toggle_deadline, 0);
            break;
            
        case 'w':    
            SCI_WRITE(&sci0, "Measuring Background WCET...\n");
            ASYNC(&bgTask, measure_background_wcet, bgTask.background_loop_range);
            break;
            
        case 'q':    
            SCI_WRITE(&sci0, "Measuring Background WCET...\n");
            ASYNC(&bgTask, measure_background_wcet, 1000);
            break;
            
        case 'a':    
            SCI_WRITE(&sci0, "Measuring Tone Generator WCET...\n");
            ASYNC(&bgTask, measure_generator_wcet, 0);
            break;
            
        default:
            if (self->buf_index < sizeof(self->buffer) - 1)
            {
                self->buffer[self->buf_index++] = c;
            }

            SCI_WRITE(&sci0, "Rcv: '");
            SCI_WRITECHAR(&sci0, c);
            SCI_WRITE(&sci0, "'\n");
            break;
    }
}


    
void background_task_core(int loop_range) {
    for (int i = 0; i < loop_range; i++) {
        
        }
    }

int get_period_index(App *self, int k) {
    
    if ( k < min_index || k > max_index) {
        return -1;
        }
        
    else {
        return k - min_index;
        }
    
    }
    
    
void get_period_key(App *self, int key) {
    char keyBuffer[10];
    snprintf(keyBuffer, sizeof(keyBuffer), "Key: %d\n", key); //
    SCI_WRITE(&sci0, keyBuffer);
    
    for (int i = 0; i < 32; i++) {
        int k = self->freq_index[i] + key;
        int period_index = get_period_index(self, k);
        
        char buffer[10];
    if (period_index != -1) {
            snprintf(buffer, sizeof(buffer), "%d ", self->period[period_index]);
        } else {
            snprintf(buffer, sizeof(buffer), "N/A ");
        }        SCI_WRITE(&sci0, buffer);

        }
        SCI_WRITE(&sci0, "\n");
    
    }

void process_input(App *self, int num)
{
    if(num <= 5 && num >= -5) {
        get_period_key(self, num);
        
        } else {
            SCI_WRITE(&sci0, "Invalid input. Please enter a number between -5 to 5.\n");
            }
    }


void increase_volume(ToneGenerator *self, int unused) {
    if (self->volume < 20) {
        self->volume += 1;
        SCI_WRITE(&sci0, "Increased Volume\n");
    } else {
        SCI_WRITE(&sci0, "Max Volume Already!\n");
            }
    }

void decrease_volume(ToneGenerator *self, int unused) {
    if (self->volume > 1) {
        self->volume -= 1;
        SCI_WRITE(&sci0, "Decreased Volume\n");
    } else {
        SCI_WRITE(&sci0, "Min Volume Already!\n");
            }
    }

void toggle_mute(ToneGenerator *self, int unused) {
    self->muted = !self->muted;
    if (self->muted) {
        SCI_WRITE(&sci0, "Muted!\n");
    } else {
        SCI_WRITE(&sci0, "Unmuted!\n");
    }}

void increase_load(BackgroundTask *self, int unused) {
    if (self->background_loop_range + 500 <= 20000) {
        self->background_loop_range += 500;
        char msg[50];
        snprintf(msg, sizeof(msg), "Increased load: %d\n", self->background_loop_range);
        SCI_WRITE(&sci0, msg);
        } else {
        SCI_WRITE(&sci0, "Max load Already!\n");
        }
    }
    
    void decrease_load(BackgroundTask *self, int unused) {
    if (self->background_loop_range - 500 >= 1000) {
        self->background_loop_range -= 500;
        char msg[50];
        snprintf(msg, sizeof(msg), "Decreased load: %d\n", self->background_loop_range);
        SCI_WRITE(&sci0, msg);
        } else {
        SCI_WRITE(&sci0, "Min load Already!\n");
        }
    }
    
    void toggle_deadline(BackgroundTask *self, int unused) {
        self->deadline = !self->deadline;
        if (self->deadline) {
            SCI_WRITE(&sci0, "Deadline Enabled\n");
            }else {
            SCI_WRITE(&sci0, "Deadline Disabled\n");
            }
        
        }

void load_task(BackgroundTask *self, int unused) {
    for (volatile int i = 0; i < self->background_loop_range; i++) {
        
        }
        if (self->deadline) {
        SEND(USEC(1300), USEC(1300), self, load_task, 0);
            }else{
        AFTER(USEC(1300), self, load_task, 0);
    }
}


void generate_tone(ToneGenerator *self, int unused) {
    switch (self->muted) {
        case 1:
            DAC_Address = 0;
            break;
        case 0:
            switch (self->state) {
                case 0:
                    DAC_Address = 0;
                    break;
                case 1:
                    DAC_Address = self->volume;
                    break;
                }
                self->state = !self->state;
                break;
            }
            if (bgTask.deadline) {
            SEND(USEC(931), USEC(100), self, generate_tone, 0);
                }else{
            AFTER(USEC(931), self, generate_tone, 0); 
                }
}

void measure_background_wcet(BackgroundTask *self, int loop_range) {
    uint32_t max_bg1 = 0;
    uint32_t total_bg1 = 0;
    uint32_t start, end, diff;
    
    int runs = NUM_RUN;
    
    for(int i = 0; i < runs; i++) {
        start = get_time();
        for (volatile int j = 0; j < loop_range; j++) {
            
            }
            end = get_time();
            diff = end - start;
            if (diff > max_bg1) {
                max_bg1 = diff;
            }
            total_bg1 += diff;

            }
        
        int avg = total_bg1 / runs;
        
        
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Current_loop=%d : Max = %lu ns, Avg = %d ns\n", loop_range, max_bg1, avg);
        SCI_WRITE(&sci0, buffer);
    }


void measure_generator_wcet(ToneGenerator *self) {
    uint32_t max = 0;
    uint32_t total = 0;
    uint32_t start, end, diff;
    
    int runs = NUM_RUN;
    
    for(int i = 0; i < runs; i++) {
        start = get_time();
        for (volatile int j = 0; j < 2000; j++) {
            
            }
            end = get_time();
            diff = end - start;
            if (diff > max) {
                max = diff;
            }
            total += diff;

            }
        
        int avg = total / runs;
        
        
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Max = %lu ns, Avg = %d ns\n", max, avg);
        SCI_WRITE(&sci0, buffer);
    }


void startApp(App *self, int arg)
{
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");


    init_period(self);
    freq_index(self);
    get_period_key(self, 0);
    
    
    init_dwt();
    // using SYNC to start task with deadline
    ASYNC(&toneGen, generate_tone, 0); 
    ASYNC(&bgTask, load_task, 0);      
    
    
    
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

