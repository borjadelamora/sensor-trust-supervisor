# What one monitor makes of the sensor it watches

uint8 SENSOR_GNSS=0
uint8 SENSOR_FLOW=1
uint8 SENSOR_CAMERA=2

uint8 LEVEL_TRUSTED=0
uint8 LEVEL_SUSPECT=1

# px4 microseconds for gnss and flow, gazebo sim time for the camera
builtin_interfaces/Time stamp
uint8 sensor
uint8 level

# Metres per second for gnss and flow, a mean pixel level for the camera
float64 residual
