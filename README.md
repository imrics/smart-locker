# Personal Smart Locker Project
## Milestone 2
- Logs to serial monitor at 9600 baud
- Starts locked with the saved password (`1234` by default)
- Password followed by `#` to unlock; `*` to lock
- While unlocked, hold `#` for 5 seconds to change password
    - Enter password followed by `#` to save or `*` to cancel
- Password persists in EEPROM across reboots
