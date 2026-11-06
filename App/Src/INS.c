#include "INS.h"
#include "cmsis_os.h"
#include "PID.h"
#include "alg_filter.h"
data_ins_t data_ins = {0};
// void INS_Task(void const * argument)
// {
// 	while(1)
// 	{
//         osDelay(4);
// 	}
// }