Thermocouple code uses Adafruit MCP9600 Library. Install via library manager -> search "Adafruit MCP9600"

Device #  | Register Byte | Reg. Val. in Kiloohms | Side
1         | 1100 000x     | GND                   | 
2  U2     | 1100 001x     | R2A = 10, R2B = 2.2   | LHS
3  U3     | 1100 010x     | R3A = 10, R3B = 4.3   | RHS
4  U4     | 1100 011x     | R4A = 10, R4B = 7.5   | LHS
5  U5     | 1100 100x     | R5A = 10, R5B = 13    | RHS
6  U6     | 1100 101x     | R6A = 10, R6B = 22    | LHS
7  U7     | 1100 110x     | R7A = 10, R7B = 43    | RHS
8         | 1100 111x     | VDD                   | 
