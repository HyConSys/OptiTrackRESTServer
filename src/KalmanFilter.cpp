/*
 * FileName : kalman_filter.c
 * Author   : xiahouzuoxin @163.com
 * Version  : v1.0
 * Date     : 2014/9/24 20:36:51
 * Brief    : 
 * 
 * Copyright (C) MICL,USTB
 */
#include <stdio.h>
#include "KalmanFilter.h"

KalmanFilter::KalmanFilter(bool _useExtendedFilter) {
    data_index = 0;
    useExtendedFilter = _useExtendedFilter;
}

void KalmanFilter::kalman1_init(kalman1_state *state, float init_x, float init_p) {
    state->x = init_x;
    state->p = init_p;
    state->A = 1;
    state->H = 1;
    state->q = 10e-6f;  /* predict noise convariance */
    state->r = 10e-5f;  /* measure error convariance */
}

float KalmanFilter::kalman1_filter(kalman1_state *state, float z_measure) {

    /* Predict */
    state->x = state->A * state->x;
    state->p = state->A * state->A * state->p + state->q;  /* p(n|n-1)=A^2*p(n-1|n-1)+q */

    /* Measurement */
    state->gain = state->p * state->H / (state->p * state->H * state->H + state->r);
    state->x = state->x + state->gain * (z_measure - state->H * state->x);
    state->p = (1 - state->gain * state->H) * state->p;

    return state->x;
}

void KalmanFilter::kalman2_init(kalman2_state *state, float *init_x, float (*init_p)[2]) {

    state->x[0]    = init_x[0];
    state->x[1]    = init_x[1];
    state->p[0][0] = init_p[0][0];
    state->p[0][1] = init_p[0][1];
    state->p[1][0] = init_p[1][0];
    state->p[1][1] = init_p[1][1];
    //state->A       = {{1, 0.1}, {0, 1}};
    state->A[0][0] = 1.0f;
    state->A[0][1] = 0.1f;
    state->A[1][0] = 0.0f;
    state->A[1][1] = 1.0f;
    //state->H       = {1,0};
    state->H[0]    = 1.0f;
    state->H[1]    = 0.0f;
    //state->q       = {{10e-6,0}, {0,10e-6}};  /* measure noise convariance */
    state->q[0]    = 10e-7f;
    state->q[1]    = 10e-7f;
    state->r       = 10e-7f;  /* estimated error convariance */
}

float KalmanFilter::kalman2_filter(kalman2_state *state, float z_measure) {

    float temp0 = 0.0f;
    float temp1 = 0.0f;
    float temp = 0.0f;

    /* Step1: Predict */
    state->x[0] = state->A[0][0] * state->x[0] + state->A[0][1] * state->x[1];
    state->x[1] = state->A[1][0] * state->x[0] + state->A[1][1] * state->x[1];
    /* p(n|n-1)=A^2*p(n-1|n-1)+q */
    state->p[0][0] = state->A[0][0] * state->p[0][0] + state->A[0][1] * state->p[1][0] + state->q[0];
    state->p[0][1] = state->A[0][0] * state->p[0][1] + state->A[1][1] * state->p[1][1];
    state->p[1][0] = state->A[1][0] * state->p[0][0] + state->A[0][1] * state->p[1][0];
    state->p[1][1] = state->A[1][0] * state->p[0][1] + state->A[1][1] * state->p[1][1] + state->q[1];

    /* Step2: Measurement */
    /* gain = p * H^T * [r + H * p * H^T]^(-1), H^T means transpose. */
    temp0 = state->p[0][0] * state->H[0] + state->p[0][1] * state->H[1];
    temp1 = state->p[1][0] * state->H[0] + state->p[1][1] * state->H[1];
    temp  = state->r + state->H[0] * temp0 + state->H[1] * temp1;
    state->gain[0] = temp0 / temp;
    state->gain[1] = temp1 / temp;
    /* x(n|n) = x(n|n-1) + gain(n) * [z_measure - H(n)*x(n|n-1)]*/
    temp = state->H[0] * state->x[0] + state->H[1] * state->x[1];
    state->x[0] = state->x[0] + state->gain[0] * (z_measure - temp); 
    state->x[1] = state->x[1] + state->gain[1] * (z_measure - temp);

    /* Update @p: p(n|n) = [I - gain * H] * p(n|n-1) */
    state->p[0][0] = (1 - state->gain[0] * state->H[0]) * state->p[0][0];
    state->p[0][1] = (1 - state->gain[0] * state->H[1]) * state->p[0][1];
    state->p[1][0] = (1 - state->gain[1] * state->H[0]) * state->p[1][0];
    state->p[1][1] = (1 - state->gain[1] * state->H[1]) * state->p[1][1];

    return state->x[0];
}

float KalmanFilter::insertElement(float dataPoint) {

    static float data0;

    if (data_index == 0)
        data0  = dataPoint;


    if (data_index == 0 && !useExtendedFilter) {
        kalman1_init(&state1, dataPoint, 5e2);
        data_index++;
        return dataPoint;
    }

    if (data_index == 1 && useExtendedFilter) {
        init_x[0] = data0;
        init_x[1] = dataPoint - data0;
        kalman2_init(&state2, init_x, init_p);
        data_index++;
        return dataPoint;
    }

    if (useExtendedFilter == false)
        return kalman1_filter(&state1, dataPoint);
    else
        return kalman2_filter(&state2, dataPoint);

}
