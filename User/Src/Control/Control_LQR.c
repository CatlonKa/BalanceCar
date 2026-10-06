#include "Control_LQR.h"

void Control_LQR_Reset(void)
{
}

void Control_LQR_Step(const Control_Input *in, Control_Output *out)
{
    float thrust;
    float steer;

    thrust = -(LQR_K_THRUST_PITCH * in->pitch +
               LQR_K_THRUST_RATE * in->pitch_rate +
               LQR_K_THRUST_SPEED * in->speed +
               LQR_K_THRUST_TURN * in->turn_rate);

    steer = -(LQR_K_STEER_PITCH * in->pitch +
              LQR_K_STEER_RATE * in->pitch_rate +
              LQR_K_STEER_SPEED * in->speed +
              LQR_K_STEER_TURN * in->turn_rate);

    out->left = thrust + steer;
    out->right = thrust - steer;
}

const Control_Algo Control_AlgoLQR =
{
    "LQR",
    Control_LQR_Reset,
    Control_LQR_Step
};
