#ifndef MY_DDS_TASK_H
#define MY_DDS_TASK_H

#include <stdint.h>
#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "math.h"
#include "arm_math.h"
#include "stdint.h"

#include "AD9954.h"

void StartDDSProcessingTask(void const * argument);

#endif /* MY_DDS_TASK_H */