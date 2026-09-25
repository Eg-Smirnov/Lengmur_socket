#pragma once

/** Initializes the power output control in the disabled state. */
void outputControlBegin();

/** Reports whether the power output is enabled. */
bool outputControlIsEnabled();

/** Toggles the power output and returns its new enabled state. */
bool outputControlToggle();
