#ifndef INDICATORS_H
#define INDICATORS_H

#include <Arduino.h>
#include "LockerState.h"

enum Feedback { UI_NONE, UI_SUCCESS, UI_ERROR, UI_NOTICE, UI_CANCEL };

void setupIndicators();
void showFeedback(Feedback next);
bool feedbackActive();
void updateMotorFeedback(State previous, State current);
// idle means the locker is eligible to sleep, apart from pending feedback.
void updateIndicators(uint32_t now, State state, bool idle);

#endif
