/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Simple interface for SDL's timer utility.
 *
 *  */

typedef struct {
    float prev;
    float now;
    float diff;
} Timer;

void timerstart(Timer *);
void timerupdate(Timer *);

void timerglobalstart();
void timerglobalupdate();
float timerglobalcurrent();
float timerglobaldiff();
