#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    Object super;
    int history[3];      
    int history_count;   
    char buffer[20];     
    int buf_index;       
} App;

App app = { initObject(), {0, 0, 0}, 0, "", 0 };

void reader(App*, int);
void receiver(App*, int);
void process_input(App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    if (c == 'F') {
        SCI_WRITE(&sci0, "Rcv: 'F'\n");
        self->history_count = 0;
        SCI_WRITE(&sci0, "The 3-history has been erased\n");
        return;
    }

    if (c == 'e') {
        SCI_WRITE(&sci0, "Rcv: 'e'\n"); 
        self->buffer[self->buf_index] = '\0';  
        int num = atoi(self->buffer);          
        self->buf_index = 0;                   

        process_input(self, num);
                     
        return;
    }

    
    if (self->buf_index < sizeof(self->buffer) - 1) {
        self->buffer[self->buf_index++] = (char)c;
    }

    
    SCI_WRITE(&sci0, "Rcv: '");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "'\n");
}

void process_input(App *self, int num) {
    
    if (self->history_count < 3) {
        self->history[self->history_count++] = num;
    } else {
        
        self->history[0] = self->history[1];
        self->history[1] = self->history[2];
        self->history[2] = num;
    }

    
    int sum = 0;
    for (int i = 0; i < self->history_count; i++) {
        sum += self->history[i];
    }

    int median;
    if (self->history_count == 1) {
        median = self->history[0];
    } else if (self->history_count == 2) {
        median = (self->history[0] + self->history[1]) / 2;
    } else {
        
        int order[3];
        for (int i = 0; i < 3; i++) order[i] = self->history[i];
        for (int i = 0; i < 2; i++) {
            for (int j = i + 1; j < 3; j++) {
                if (order[i] > order[j]) {
                    int temp = order[i];
                    order[i] = order[j];
                    order[j] = temp;
                }
            }
        }
        median = order[1];
    }

    
    char output[100];
    snprintf(output, sizeof(output), "Entered integer %d: sum = %d, median = %d\n", num, sum, median);
    SCI_WRITE(&sci0, output);
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

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

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
