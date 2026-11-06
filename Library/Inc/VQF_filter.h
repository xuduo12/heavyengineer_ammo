/**牛逼！
 * @file vqf_filter.h
 * @brief VQF姿态滤波算法头文件
 */

#ifndef HORIZON_VQF_FILTER_H
#define HORIZON_VQF_FILTER_H

#include <stdbool.h>
#include <stdint.h>

typedef float vqf_real_t;
typedef double vqf_double_t;

/**
 * @brief VQF滤波器结构体
 */
struct VQF_FILTER_t {
    /* ================= 参数 (Params) =================
     * 这些在 vqf_init() 里设置默认值，可按需调整 */
    vqf_real_t tauAcc;                      /* 加速度计低通滤波时间常数(s)，越大对加速度噪声抑制越强，但动态响应变慢 */
    bool motionBiasEstEnabled;              /* 是否开启"运动中的零偏估计"，开启可在运动时也逐步修正陀螺仪零偏 */
    bool restBiasEstEnabled;                /* 是否开启"静止时零偏估计"，静止时用加速度计校准零偏，精度更高 */
    vqf_real_t biasSigmaInit;               /* 零偏估计的初始方差(°/s)，初始不确定度 */
    vqf_real_t biasForgettingTime;          /* 零偏忘记时间常数(s)，控制零偏估计收敛速度，越小收敛越快 */
    vqf_real_t biasClip;                    /* 零偏限幅(°/s)，防止零偏估计发散到离谱值 */
    vqf_real_t biasSigmaMotion;             /* 运动时零偏过程噪声方差(°/s) */
    vqf_real_t biasVerticalForgettingFactor;/* 竖直方向(重力方向)零偏遗忘因子，越小重力方向零偏收敛越慢 */
    vqf_real_t biasSigmaRest;               /* 静止时零偏过程噪声方差(°/s) */
    vqf_real_t restMinT;                    /* 判定"静止"的最短持续时间(s)，静止不足该时长不启用静止零偏估计 */
    vqf_real_t restFilterTau;               /* 静止检测用的低通滤波时间常数(s) */
    vqf_real_t restThGyr;                   /* 静止检测的陀螺仪角速度阈值(°/s)，低于此值算静止 */
    vqf_real_t restThAcc;                   /* 静止检测的加速度波动阈值(m/s²)，加速度变化小于此值算静止 */

    /* ================= 系数 (Coeffs) =================
     * 内部滤波器系数，由 vqf_init() 根据上述参数计算，无需手动改 */
    vqf_real_t gyrTs;                       /* 陀螺仪采样周期(s)，即传入 vqf_update 的 dt */
    vqf_real_t accTs;                       /* 加速度计采样周期(s)，默认与 gyrTs 相同 */
    vqf_double_t accLpB[3];                 /* 加速度低通滤波器分子系数 b0/b1/b2（二阶巴特沃斯） */
    vqf_double_t accLpA[2];                 /* 加速度低通滤波器分母系数 a1/a2 */
    vqf_real_t biasP0;                      /* 零偏卡尔曼协方差初值 */
    vqf_real_t biasV;                       /* 零偏卡尔曼过程噪声 */
    vqf_real_t biasMotionW;                 /* 运动零偏估计的测量权重 */
    vqf_real_t biasVerticalW;               /* 重力方向零偏估计的测量权重 */
    vqf_real_t biasRestW;                   /* 静止零偏估计的测量权重 */
    vqf_double_t restGyrLpB[3];             /* 静止检测陀螺仪低通滤波器分子系数 */
    vqf_double_t restGyrLpA[2];             /* 静止检测陀螺仪低通滤波器分母系数 */
    vqf_double_t restAccLpB[3];             /* 静止检测加速度低通滤波器分子系数 */
    vqf_double_t restAccLpA[2];             /* 静止检测加速度低通滤波器分母系数 */

    /* ================= 状态 (State) =================
     * 滤波器内部运行状态，一般不需要读，给懂算法的人看 */
    vqf_real_t gyrQuat[4];                  /* 纯陀螺仪积分出的四元数 [w,x,y,z]（只用角速度） */
    vqf_real_t accQuat[4];                  /* 纯加速度计校正出的四元数 [w,x,y,z]（只用重力方向） */
    bool restDetected;                      /* 当前是否判定为静止（静置时置 true） */
    vqf_real_t lastAccLp[3];                /* 加速度低通滤波后的加速度，用于校正 accQuat */
    vqf_double_t accLpState[6];             /* 加速度低通滤波器的内部状态（3通道 × 2阶） */
    vqf_real_t lastAccCorrAngularRate;      /* 上一次加速度校正产生的角速度(rad/s) */
    vqf_real_t bias[3];                     /* 当前估计的陀螺仪零偏(rad/s)，vqf_update 内部会减去它 */
    vqf_real_t biasP[9];                    /* 零偏卡尔曼协方差矩阵（3×3） */
    vqf_double_t motionBiasEstRLpState[18]; /* 运动零偏估计中 R 矩阵滤波状态（9通道 × 2阶） */
    vqf_double_t motionBiasEstBiasLpState[4];/* 运动零偏估计中 bias 滤波状态（2通道 × 2阶） */
    vqf_real_t restLastSquaredDeviations[2];/* 静止检测的累计方差：[0]陀螺仪 [1]加速度 */
    vqf_real_t restT;                       /* 当前已持续"疑似静止"的累计时间(s) */
    vqf_real_t restLastGyrLp[3];            /* 静止检测陀螺仪滤波输出 */
    vqf_double_t restGyrLpState[6];         /* 静止检测陀螺仪滤波器内部状态 */
    vqf_real_t restLastAccLp[3];            /* 静止检测加速度滤波输出 */
    vqf_double_t restAccLpState[6];         /* 静止检测加速度滤波器内部状态 */

    /* ================= 输出 (Outputs) =================
     * 业务代码主要用这里的字段，由 vqf_output() 每次更新 */
    float q[4];                             /* 【最终四元数】[w,x,y,z]，机体姿态的完整表示 */
    float pitch, roll, yaw;                 /* 【欧拉角】俯仰角/横滚角/偏航角（度，yaw 范围 ±180） */
    float yaw_laps;                         /* 偏航角跨过 ±180 的次数（绕圈计数，每转一圈 ±1） */
    float last_yaw;                         /* 上一次的 yaw，内部用于计算累计偏航 */
    float YawTotalAngle;                    /* 【累计偏航角】(度)，可跨过 ±180 连续累加，适合做控制 */
    float rMat[3][3];                       /* 【旋转矩阵】机体坐标系转地理坐标系（当前未更新，备用） */
};

extern struct VQF_FILTER_t vqf_filter;

void vqf_init(struct VQF_FILTER_t *f, float dt);
void vqf_update(struct VQF_FILTER_t *f, float gx, float gy, float gz, float ax, float ay, float az);
void vqf_output(struct VQF_FILTER_t *f);

#endif //HORIZON_VQF_FILTER_H
