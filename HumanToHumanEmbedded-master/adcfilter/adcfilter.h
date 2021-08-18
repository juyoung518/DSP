#ifndef ADCFILTER_H
#define ADCFILTER_H

#define RMSWindow   50


class adcFilter
{
public:
    void init(float numerator[5], float denominator[4], float filterGain, short int filterSelect);
    void padding(short int filterSelect);
    float notch(int newVal);
    float lpf(float newVal);
    int envelope(float newVal);
private:
    float filterNum[5];
    float filterDen[4];
    float gain;
    short int filterSel;
    float xv[5];
    float yv[5];
    float RMSEnv[RMSWindow];
    float RMSSum = 0;
};

#endif