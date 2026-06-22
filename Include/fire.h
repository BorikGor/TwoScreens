#ifndef FIRE_H
#define FIRE_H

#include <stdint.h>

/*
 * Function: fire_init
 * -------------------
 * Initialize the fire simulation state.
 *
 * Method:
 * - Clears internal heat buffers.
 * - Resets timing state.
 * - Renders one initial frame.
 *
 * Variables:
 * - none.
 */
void fire_init(void);

/*
 * Function: fire_tick
 * -------------------
 * Advance and render the fire effect when its frame time expires.
 *
 * Method:
 * - Uses the shared system millisecond counter from the application.
 * - Runs one simulation step only when enough time has elapsed.
 *
 * Variables:
 * - now_ms: current application time in milliseconds.
 */
void fire_tick(uint32_t now_ms);

#endif