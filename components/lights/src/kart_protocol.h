// Auto-generated kart protocol definitions
// DO NOT EDIT MANUALLY

// Component Types
#define KART_TYPE_LIGHTS      0x01
#define KART_TYPE_MOTORS      0x02
#define KART_TYPE_BATTERIES   0x03
#define KART_TYPE_CONTROLS    0x04
#define KART_TYPE_SENSORS     0x05

// Light Components
#define KART_LIGHT_FRONT        0x01
#define KART_LIGHT_REAR         0x02
#define KART_LIGHT_LEFT_SIGNAL  0x03
#define KART_LIGHT_RIGHT_SIGNAL 0x04
#define KART_LIGHT_BRAKE        0x05

// Motor Components
#define KART_MOTOR_REAR_LEFT   0x01
#define KART_MOTOR_REAR_RIGHT  0x02

// Battery Components
#define KART_BATTERY_REAR_LEFT  0x01
#define KART_BATTERY_REAR_RIGHT 0x02
#define KART_BATTERY_CENTER     0x03

// Control Components
#define KART_CONTROL_STEERING  0x01
#define KART_CONTROL_THROTTLE  0x02
#define KART_CONTROL_BRAKE     0x03

// Sensor Components
#define KART_SENSOR_SPEED        0x01
#define KART_SENSOR_TEMPERATURE  0x02
#define KART_SENSOR_CURRENT      0x03

// Light Commands
#define KART_CMD_LIGHT_MODE    0x01
#define KART_CMD_LIGHT_SIGNAL  0x02
#define KART_CMD_LIGHT_BRAKE   0x03
#define KART_CMD_LIGHT_TEST    0x04

// Motor Commands
#define KART_CMD_MOTOR_POWER   0x01
#define KART_CMD_MOTOR_SPEED   0x02

// Control Commands
#define KART_CMD_CONTROL_CALIBRATE 0x01

// Light Mode Values
#define KART_LIGHT_MODE_OFF    0x00
#define KART_LIGHT_MODE_LOW    0x01
#define KART_LIGHT_MODE_HIGH   0x02
#define KART_LIGHT_MODE_HAZARD 0x03

// Light Signal Values
#define KART_LIGHT_SIGNAL_OFF   0x00
#define KART_LIGHT_SIGNAL_LEFT  0x01
#define KART_LIGHT_SIGNAL_RIGHT 0x02

// Light Binary Values
#define KART_LIGHT_OFF 0x00
#define KART_LIGHT_ON  0x01

// Motor Power Values
#define KART_MOTOR_POWER_OFF     0x00
#define KART_MOTOR_POWER_ON      0x01
#define KART_MOTOR_POWER_REVERSE 0x02

// Calibration Values
#define KART_CALIBRATE_BEGIN  0x01
#define KART_CALIBRATE_END    0x02
#define KART_CALIBRATE_CANCEL 0x03

// System Status Values
#define KART_STATUS_STANDBY  0x00
#define KART_STATUS_RUNNING  0x01
#define KART_STATUS_ERROR    0x02
#define KART_STATUS_CHARGING 0x03

// State IDs
#define KART_STATE_LIGHT_MODE    0x01
#define KART_STATE_LIGHT_SIGNAL  0x02
#define KART_STATE_LIGHT_BRAKE   0x03
#define KART_STATE_LIGHT_TEST    0x04

#define KART_STATE_CONTROL_ANGLE       0x01
#define KART_STATE_CONTROL_THROTTLE    0x02
#define KART_STATE_CONTROL_BRAKE       0x03

#define KART_STATE_MOTOR_RPM           0x01
#define KART_STATE_MOTOR_TEMPERATURE   0x02

#define KART_STATE_BATTERY_VOLTAGE     0x01
#define KART_STATE_BATTERY_CURRENT     0x02
#define KART_STATE_BATTERY_TEMPERATURE 0x03
#define KART_STATE_BATTERY_CHARGE      0x04

#define KART_STATE_SYSTEM_STATUS       0x01
#define KART_STATE_SYSTEM_ERRORS       0x02

// Message Format
// byte0: [4 bits component_type][4 bits command/state]
// byte1: component_id
// byte2: value_id or command_id
// bytes3-7: data