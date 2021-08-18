#include <adcfilter.h>
#include <chrono>
#include <thread>
#include <math.h>
#include </Users/juyoung/Library/Arduino15/packages/esp32/hardware/esp32/1.0.4/tools/sdk/include/driver/driver/adc.h>
#include <stdio.h>
#include <string.h>



void adcFilter::init(float numerator[5], float denominator[4], float filterGain, short int filterSelect)
{
    // Save to filterNum
    memcpy(filterNum, numerator, sizeof(float) * 5);
    memcpy(filterDen, denominator, sizeof(float) * 4);
    gain = filterGain;
    filterSel = filterSelect;
    padding(filterSel);
    for (int i=0; i < RMSWindow; i++){
      RMSEnv[i] = 0;
    }
}

void adcFilter::padding(short int filterSelect)
{
    int paddingLen = 0;
    if (filterSelect == 0) {
        // Notch Filter
        paddingLen = 5;
    } else {
        // LPF Filter
        paddingLen = 3;
    }

    for (int i = 0; i < paddingLen; i++){
        float adcVal = adc1_get_raw(ADC1_CHANNEL_4) * 1.0;
        xv[i] = adcVal;
        yv[i] = adcVal;
        using namespace std::this_thread;
        using namespace std::chrono;
        sleep_for(milliseconds(2));
    }
}

float adcFilter::notch(int newVal)
{
    for (int i=0; i < 4; i++){
        xv[i] = xv[i+1];
        yv[i] = yv[i+1];
    }
    xv[4] = round(newVal / gain * 10000.0) / 10000.0;
    yv[4] = round((xv[0] + yv[0] * filterDen[0] + \
                      xv[1] * filterNum[1] + yv[1] * filterDen[1] + \
                      xv[2] * filterNum[2] + yv[2] * filterDen[2] + \
                      xv[3] * filterNum[3] + yv[3] * filterDen[3] + \
                      xv[4]) * 10000.0) / 10000.0;
    return yv[4];
}

float adcFilter::lpf(float newVal)
{
    for (int i=0; i < 2; i++){
        xv[i] = xv[i+1];
        yv[i] = yv[i+1];
    }
    xv[2] = round(newVal * 10000.0 / gain) / 10000.0;
    yv[2] = round((xv[0] + yv[0] * (-0.8371816513) + \
                      xv[1] * (-2) + yv[1] * (1.8226949252) + \
                      xv[2]) * 10000.0) / 10000.0;
    return yv[2];
}

int adcFilter::envelope(float newVal)
{
    RMSSum -= RMSEnv[0];
    for (int i=0; i < RMSWindow-1; i++){
        RMSEnv[i] = RMSEnv[i+1];
    }
    RMSEnv[RMSWindow-1] = pow(newVal, 2) / RMSWindow;
    RMSSum += RMSEnv[RMSWindow-1];
    return (int) round(sqrt(RMSSum));
}