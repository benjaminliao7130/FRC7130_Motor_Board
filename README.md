# FRC 7130 DIY Motor Control Board

A standalone, custom motor control box designed by FRC 7130 to streamline mechanism prototyping. Powered by an ATmega328P (or Arduino UNO), this board allows sub-teams to test mechanisms independently without needing a full FRC control system (RoboRIO, Radio, etc.).

It takes a 12V input from a standard FRC PDP/PDH (via an internal LM2596 buck converter to 5V) and outputs dual PWM signals to drive motor controllers like Spark MAX or Victor SPX.

## Key Features
*   **Dual Motor Control:** Independent speed control via two potentiometers.
*   **Boot Safety Check:** Requires pots to be zeroed on startup before arming.
*   **3 Operation Modes:** 
    *   **IDLE:** Safe mode (motors disabled).
    *   **FULL:** 100% speed range (5% increments).
    *   **FINE:** Capped at 50% max speed with 1% resolution for precision tuning.
*   **Software Ramping:** Slew rate limits prevent sudden current spikes and mechanical damage.
*   **UI Feedback:** 1.3" OLED display (speed/mode) and dedicated WS2812B status LEDs.

