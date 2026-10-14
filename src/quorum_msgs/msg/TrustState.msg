# reasoning from arbiter for each sensor

uint8 SENSOR_GNSS=0
uint8 SENSOR_FLOW=1
uint8 SENSOR_CAMERA=2

uint8 LEVEL_TRUSTED=0
uint8 LEVEL_SUSPECT=1
uint8 LEVEL_BANNED=2

uint8 EVIDENCE_ACCUSED=0
uint8 EVIDENCE_CLEARED=1
uint8 EVIDENCE_SILENT=2

uint8 OUTCOME_AGREEMENT=0
uint8 OUTCOME_UNATTRIBUTED=1
uint8 OUTCOME_ACCUSED=2

# px4 time of the imu sample the vote ran on
builtin_interfaces/Time stamp

# Indexed by the SENSOR_ constants
uint8[3] level
uint8[3] evidence
bool[3] accused

uint8 outcome
float64 margin_mps

# NaN when empty
float64 residual_gnss_flow_mps
float64 residual_gnss_ref_mps
float64 residual_flow_ref_mps
