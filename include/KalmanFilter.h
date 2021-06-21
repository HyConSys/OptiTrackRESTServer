#ifndef _KALMAN_CLASS_H
#define _KALMAN_CLASS_H

class KalmanFilter{
public:
    KalmanFilter(bool _useExtendedFilter);
    float insertElement(float dataPoint);
private:
    void kalman1_init(kalman1_state *state, float init_x, float init_p);
    float kalman1_filter(kalman1_state *state, float z_measure);
    void kalman2_init(kalman2_state *state, float *init_x, float (*init_p)[2]);
    float kalman2_filter(kalman2_state *state, float z_measure);
    int data_index;
    bool useExtendedFilter;
    kalman1_state state1;
    kalman2_state state2;
    float init_x[2];
    float init_p[2][2] = {{10e-6,0}, {0,10e-6}};

};

#endif