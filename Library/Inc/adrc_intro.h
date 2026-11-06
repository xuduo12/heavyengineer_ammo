#ifndef __ADRC_INTRO_H__
#define __ADRC_INTRO_H__

/*=============================================================================
 * 文件名称：adrc_intro.h
 * 适用对象：自抗扰控制器（ADRC）——学习/教学版接口定义
 *
 * 【这个文件是什么】
 *   这是与工程版 ADRC.h 完全等价的接口，但注释是"逐行讲解式"的：
 *   不只看懂"函数签名"，还能看懂"为什么要这个参数、算法每一步怎么算"。
 *   算法实现、每一步的推导与逐行注释，全部在 adrc_intro.c 里。
 *
 * 【ADRC 一句话理解】
 *   被控对象用"标称二阶模型"来描述：
 *              y'' = b0·u + f(t)
 *    其中 b0·u 是"已知/可控"的部分（我下多大指令，就会按 b0 比例产生加速度），
 *    而 f(t) 是"总扰动"：外力、摩擦、未建模动态、参数误差、甚至模型的
 *    非线性…… 把一切不确定的东西都塞进这一个 f(t) 里。
 *    ADRC 的思路：用一个"扩张状态观测器 ESO"实时观测出 f(t)（记为 z3），
 *    然后在控制端直接把它抵消：
 *              u = (u0 − z3) / b0
 *    抵消之后，对象就退化成 y'' ≈ b0·u0 的"纯积分串联"结构，
 *    剩下的 u0 用一个简单的 PD 就足够控制了。
 *    —— 这就是"先估计扰动、再抵消扰动"的自抗扰思想。
 *
 * 【四大部件】（对应 adrc_intro.c 里的 4 个函数）
 *   1) TD   跟踪微分器     : 对给定信号安排平滑的过渡过程，输出 v1≈给定位置、
 *                            v2≈给定速度，避免给定量突变时输出猛冲。
 *   2) ESO  扩张状态观测器 : 用输入 u 和反馈 y 实时观测
 *                            z1≈真实被控量、z2≈其一阶导、z3≈总扰动。
 *   3) LSEF 线性误差反馈   : 对 (TD输出 − ESO观测) 做 PD，得到虚拟控制量 u0。
 *   4) 扰动补偿           : u = (u0 − z3)/b0，把观测到的扰动直接抵消。
 *
 * 【核心调参思想：带宽法（高志强）】
 *   所有增益都由两个带宽唯一确定，无需手算：
 *     w0 观测器带宽：ESO 收敛/抗扰有多快（β1~β3 由 w0 推出）
 *     wc 控制器带宽：闭环响应有多快（kp/kd 由 wc 推出）
 *
 * 作者：根据韩京清《自抗扰控制技术》及高志强线性ADRC整理
 * 平台：任意 C99 编译器（嵌入式 / PC 均可）
 *=====================================================================*/

#include "stdint.h"

/*--------------------- 参数默认值（可自行修改） ---------------------*/
#define ADRC_TD_DEFAULT_R    5000.0f   /* TD 跟踪速度因子，越大跟踪越快 */
#define ADRC_TD_DEFAULT_H0    0.01f    /* TD 滤波/步长因子，一般 = 控制周期 */
#define ADRC_ESO_DEFAULT_W0   30.0f    /* ESO 观测器带宽，rad/s */
#define ADRC_LSEF_DEFAULT_WC  15.0f    /* 控制器带宽，rad/s */
#define ADRC_OUT_MAX_DEFAULT 16384.0f  /* 输出限幅（例如 CAN 电流指令 ±16384） */
#define ADRC_B0_MIN           1e-4f    /* b0 下限，防止除零/除极小值 */

/*--------------------- 阶数定义 ---------------------*/
/* ADRC_VEL : 一阶 ADRC，用于速度环（被控量是速度，只需观测 2 个状态：
 *             z1≈速度，z2≈总扰动）
 * ADRC_POS : 二阶 ADRC，用于位置环（被控量是位置，观测 3 个状态：
 *             z1≈位置，z2≈速度，z3≈总扰动） */
typedef enum
{
    ADRC_VEL = 1,   /* 速度环 */
    ADRC_POS = 2,   /* 位置环 */
} ADRC_order_e;

/*--------------------- ADRC 实例结构体 ---------------------*/
/* 一个结构体 = 一个独立的 ADRC 控制器。要用多个环（位置环外环 + 速度环内环）
 * 就定义多个实例，各自独立初始化、独立计算。 */
typedef struct
{
    /* 阶数：ADRC_VEL 或 ADRC_POS */
    ADRC_order_e order;

    /* —— TD 跟踪微分器状态 —— */
    float td_v1;    /* v1：平滑后的"位置给定"（过渡过程输出） */
    float td_v2;    /* v2：给定"速度"（v1 的微分，带滤波） */
    float td_r;     /* r ：跟踪速度因子：越大过渡越快，但给定突变越猛 */
    float td_h;     /* h ：积分步长，一般 = 控制周期(秒) */

    /* —— ESO 扩张状态观测器状态 —— */
    float eso_z1;   /* z1：对反馈量的观测（≈ 真实被控量） */
    float eso_z2;   /* z2：对反馈量一阶导的观测（≈ 真实速度） */
    float eso_z3;   /* z3：对总扰动 f(t) 的观测（位置环用；速度环此处=0） */
    float eso_b0;   /* b0：被控对象增益估计（"控制量→被控量二阶导"的比例） */
    float eso_beta1;/* β1：ESO 观测器增益（带宽法整定，≈ 3·w0） */
    float eso_beta2;/* β2：同上（≈ 3·w0²） */
    float eso_beta3;/* β3：同上（≈ w0³，位置环用） */
    float eso_w0;   /* w0：观测器带宽，调它即可（β1/β2/β3 自动算好） */

    /* —— LSEF 线性误差反馈系数 —— */
    float lsef_kp;  /* kp：PD 比例系数（位置环 = wc²，速度环 = wc） */
    float lsef_kd;  /* kd：PD 微分系数（位置环 = 2·wc，速度环不用 = 0） */
    float lsef_wc;  /* wc：控制器带宽，调它即可（kp/kd 自动算好） */

    /* —— 记录量（便于上位机查看/调参） —— */
    float set;      /* 目标给定（本周期传入） */
    float fdb;      /* 反馈量   （本周期传入） */
    float out;      /* 控制器输出（已限幅，可直接下发执行机构） */
    float out_max;  /* 输出限幅，|u| 不会超过它 */
    float error;    /* 误差 set - ref（已做角度环绕处理） */
    float disturbance; /* 观测到的总扰动 z3（位置环）/ z2（速度环），
                          方便调参时观察扰动补偿是否跟上 */

} ADRC_type_def;

/*--------------------- 函数接口 ---------------------*/
/* 初始化：使用前必须调用一次 */
void ADRC_Init(ADRC_type_def *ADRC,
                 ADRC_order_e order,   /* ADRC_VEL 速度环 / ADRC_POS 位置环 */
                 float w0,               /* ESO 观测器带宽(rad/s)，先按 3~10 倍 wc 取 */
                 float wc,               /* 控制器带宽(rad/s)，从 10~15 起步 */
                 float b0,               /* 被控对象增益估计 */
                 float r,                /* TD 跟踪速度因子 */
                 float h,                /* 控制周期(秒)，调用周期必须等于 h */
                 float out_max);         /* 输出限幅 */

/* 核心计算：每个固定周期调用一次，返回控制器输出 u */
/*   ref = 反馈量（编码器位置 / 速度实测值）
 *   set = 目标量（目标位置 / 目标速度）
 *   max = 角度量程（环绕处理用）：
 *            0    = 线性量（速度、普通位置），不做环绕
 *          非 0   = 循环量（如编码器一圈 8192 就传 8192），自动把
 *                   误差/状态都搬到 [-max/2, +max/2) 区间，做最短路径跟随 */
float ADRC_Calc(ADRC_type_def *ADRC, float ref, float set, int max);

/* 在线调整参数（运行时随时可调，下一周期即生效） */
void ADRC_SetESOBandwidth(ADRC_type_def *ADRC, float w0);   /* 只改 w0，β1/β2/β3 自动重算 */
void ADRC_SetControllerBandwidth(ADRC_type_def *ADRC, float wc); /* 只改 wc，kp/kd 自动重算 */
void ADRC_SetB0(ADRC_type_def *ADRC, float b0);             /* 重估被控对象增益后更新 */
void ADRC_SetOutMax(ADRC_type_def *ADRC, float out_max);    /* 改输出限幅 */
void ADRC_SetControlPeriod(ADRC_type_def *ADRC, float h); /* 改控制周期 */
/* 复位：把 TD / ESO 的状态清零（换被控对象、重新起机时调用） */
void ADRC_Reset(ADRC_type_def *ADRC);

#endif /* __ADRC_INTRO_H__ */
