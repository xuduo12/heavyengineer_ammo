//适配2027通讯协议V1.0.0
#ifndef REFEREE_H
#define REFEREE_H
#include "main.h"
#include "crc_ref.h"
#include "string.h"
#include "fifo.h"
#include "bsp_uart.h"

#define USART6_RX_BUF_LENGHT     256
#define UART_REFEREE_FIFO_BUF_LENGTH 512
#define HEADER_SOF 0xA5
#define REF_PROTOCOL_FRAME_MAX_SIZE         320//0x0310有300字节
#define REF_PROTOCOL_HEADER_SIZE            sizeof(frame_header_struct_t)  // 5?????
#define REF_PROTOCOL_CMD_SIZE               2
#define REF_PROTOCOL_CRC16_SIZE             2
#define REF_HEADER_CRC_LEN                  (REF_PROTOCOL_HEADER_SIZE + REF_PROTOCOL_CRC16_SIZE)
#define REF_HEADER_CRC_CMDID_LEN            (REF_PROTOCOL_HEADER_SIZE + REF_PROTOCOL_CRC16_SIZE + sizeof(uint16_t))
#define REF_HEADER_CMDID_LEN                (REF_PROTOCOL_HEADER_SIZE + sizeof(uint16_t))

typedef  struct __attribute__((packed)) 
{
  uint8_t SOF;
  uint16_t data_length;
  uint8_t seq;
  uint8_t CRC8;
} frame_header_struct_t;


typedef enum 
{
  STEP_HEADER_SOF  = 0,
  STEP_LENGTH_LOW  = 1,
  STEP_LENGTH_HIGH = 2,
  STEP_FRAME_SEQ   = 3,
  STEP_HEADER_CRC8 = 4,
  STEP_DATA_CRC16  = 5,
} referee_unpack_step_e;


typedef struct __attribute__((packed)) 
{
  frame_header_struct_t *p_header;
  uint16_t       data_len;
  uint8_t        protocol_packet[REF_PROTOCOL_FRAME_MAX_SIZE];
  referee_unpack_step_e  unpack_step;
  uint16_t       index;
} referee_unpack_data_t;


/*裁判系统回包结构体*/

/* ID: 0x0001  Byte: 11    比赛状态数据(1Hz)    服务器->全体机器人 */
typedef  struct __attribute__((packed))
{
  uint8_t  game_type : 4;      // bit0-3 比赛类型: 1超级对抗赛 2高校单项赛 3ICRA 4高校联盟赛3V3 5高校联盟赛步兵对抗
  uint8_t  game_progress : 4;  // bit4-7 比赛阶段: 0未开始 1准备阶段 2十五秒自检 3五秒倒计时 4比赛中 5结算中
  uint16_t stage_remain_time;  // 当前阶段剩余时间, 单位: s
  uint64_t SyncTimeStamp;      // UNIX时间, 连上NTP服务器后生效
} game_state_t;

/* ID: 0x0002  Byte:  1    比赛结果数据(比赛结束)    服务器->全体机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  winner;             // 0平局 1红方胜利 2蓝方胜利
} game_result_t;

/* ID: 0x0003  Byte: 24    机器人血量数据(3Hz)    服务器->全体机器人 */
typedef struct __attribute__((packed))
{
  uint16_t ally_1_robot_HP;     // 己方1号重装机器人血量, 未上场/被罚下为0
  uint16_t reserved;            // 保留位(己方2号)
  uint16_t ally_3_robot_HP;     // 己方3号步兵机器人血量
  uint16_t ally_4_robot_HP;     // 己方4号步兵机器人血量
  int16_t  damage_difference;   // 己方全队总伤害与对方全队总伤害之差
  uint16_t ally_7_robot_HP;     // 己方7号哨兵机器人血量
  uint16_t outpost_R_HP;        // R前哨血量
  uint16_t ally_base_HP;        // 己方基地血量
  uint16_t outpost_B_HP;        // B前哨血量
  uint16_t enemy_base_HP;       // 对方基地血量
  uint16_t ally_control_degree; // 己方累计控制度
  uint16_t enemy_control_degree;// 对方累计控制度
} game_robot_HP_t;

/* ID: 0x0101  Byte:  8    场地事件数据(1Hz)    服务器->己方全体机器人 */
typedef struct __attribute__((packed))
{
  uint32_t event_data;    // bit0己方补给区激活 bit1-2己方小能量机关(0未激活 1已激活 2正在激活)
                          // bit3-4己方大能量机关(同上) bit5-7 R堡垒增益点(0未激活 1己方 2对方 3双方)
                          // bit8-10 B堡垒增益点 bit11-13 R基地增益点 bit14-16 B基地增益点
                          // bit17己方制高点 bit18-19 R中央高地(0未激活 1己方 2对方)
                          // bit20-21 B中央高地 bit22-24 R前哨增益点 bit25-27 B前哨增益点
                          // bit28-29 R前哨阵地控制(0己方完全控制 1己方临时控制 2己方未控制)
                          // bit30-31 B前哨阵地控制
  uint32_t event_data_2;  // bit0-1 R基地阵地控制(0己方完全控制 1己方临时控制 2己方未控制)
                          // bit2-3 B基地阵地控制
                          // bit4-12 对方飞镖最后一次击中己方前哨/基地的时间, 单位s, 0-420
                          // bit13-15 对方飞镖最后击中的具体目标(0开局默认 1R前哨 2基地固定
                          //         3基地随机固定 4基地随机移动 5基地末端移动 6B前哨)
                          // bit16-17 R高地控制区激活(仅RMUL) bit18-19 R高地控制区控制状态(仅RMUL)
                          // bit20-23 B高地控制区激活/控制状态(仅RMUL)
                          // bit24-27 中央控制区激活/控制状态(仅RMUL)
} event_data_t;

/* ID: 0x0104  Byte:  3    裁判警告数据(触发/1Hz)    服务器->被判罚方全体机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  level;               // 己方最后受罚等级: 1双方黄牌 2黄牌 3红牌 4判负
  uint8_t  offending_robot_id;  // 违规机器人ID(红1为1, 蓝1为101), 判负/双方黄牌时为0
  uint8_t  count;               // 该等级违规次数, 开局为0
} referee_warning_t;

/* ID: 0x0105  Byte:  3    飞镖发射相关数据(1Hz)    服务器->己方全体机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  dart_remaining_time;  // 己方飞镖发射剩余时间, 单位: s
  uint16_t dart_info;            // bit0-2 最近一次己方飞镖击中目标(0默认 1R前哨 2基地固定 3基地随机固定
                                 //       4基地随机移动 5基地末端移动 6B前哨)
                                 // bit3-5 对方最近被击中目标累计次数(最多4)
                                 // bit6-8 飞镖当前选定的击打目标(0默认/前哨站 1基地固定 2基地随机固定
                                 //       3基地随机移动 4基地末端移动)
                                 // bit9-15 保留
} dart_info_t;

/* ID: 0x0201  Byte: 17    机器人性能体系数据(10Hz)    主控模块->对应机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  robot_id;                       // 本机器人ID
  uint8_t  robot_level;                    // 机器人等级
  uint16_t current_HP;                     // 机器人当前血量
  uint16_t maximum_HP;                     // 机器人血量上限
  uint16_t shooter_barrel_cooling_value;   // 射击热量每秒冷却值
  uint16_t shooter_barrel_heat_limit;      // 射击热量上限
  uint16_t chassis_power_limit;            // 底盘功率上限
  float    bullet_speed_limit;             // 射击初速度上限, 单位: m/s
  uint8_t  power_management_gimbal_output : 1;   // 电源管理gimbal口输出: 0无输出 1有24V输出
  uint8_t  power_management_chassis_output : 1;  // 电源管理chassis口输出: 0无输出 1有24V输出
  uint8_t  power_management_shooter_output : 1;  // 电源管理shooter口输出: 0无输出 1有24V输出
} game_robot_state_t;

/* ID: 0X0202  Byte: 14    实时底盘缓冲能量和射击热量数据(10Hz)    主控模块->对应机器人 */
typedef struct __attribute__((packed))
{
  uint16_t reserved0;                 // 保留位(原底盘电压)
  uint16_t reserved1;                 // 保留位(原底盘电流)
  float    reserved2;                 // 保留位(原底盘功率)
  uint16_t buffer_energy;             // 缓冲能量, 单位: J
  uint16_t shooter_17mm_barrel_heat;  // 17mm发射机构的射击热量
  uint16_t shooter_42mm_barrel_heat;  // 42mm发射机构的射击热量
} power_heat_data_t;

/* ID: 0x0203  Byte: 12    机器人位置数据(1Hz)    主控模块->对应机器人 */
typedef struct __attribute__((packed))
{
  float    x;      // 本机器人位置x坐标, 单位: m
  float    y;      // 本机器人位置y坐标, 单位: m
  float    angle;  // 测速模块朝向, 单位: 度, 正北为0度
} game_robot_pos_t;

/* ID: 0x0204  Byte:  7    机器人增益数据(3Hz)    服务器->对应机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  recovery_buff;       // 回血增益百分比, 10表示每秒恢复血量上限的10%
  uint16_t cooling_buff;        // 射击热量冷却增益具体值, x表示冷却增加x/s
  uint8_t  defence_buff;        // 防御增益百分比, 50表示50%防御
  uint8_t  vulnerability_buff;  // 负防御增益百分比, 30表示-30%防御
  uint16_t attack_buff;         // 攻击增益百分比, 50表示50%攻击
} buff_musk_t;

/* ID: 0x0206  Byte:  7    伤害状态数据(伤害发生后)    主控模块->对应机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  armor_id : 4;             // 被击/撞击/离线的装甲模块或测速模块ID编号, 其他原因扣血为0
  uint8_t  HP_deduction_reason : 4;  // 血量变化类型: 0装甲被弹丸攻击 1装甲模块离线
                                     //               2超级电容管理模块离线 5装甲受到撞击
  uint16_t hit_sequence;             // 命中事件序号, 每发命中后递增1
  int16_t  hit_position_x;           // 命中位置x坐标, 单位: mm, 以装甲检测面左下角为原点
  int16_t  hit_position_y;           // 命中位置y坐标, 单位: mm
} robot_hurt_t;

/* ID: 0x0207  Byte:  7    实时射击数据(弹丸发射后)    主控模块->对应机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  bullet_type;          // 弹丸类型: 1为17mm弹丸 2为42mm弹丸
  uint8_t  shooter_number;       // 发射机构ID: 1为17mm发射机构 2保留 3为42mm发射机构
  uint8_t  launching_frequency;  // 弹丸射速, 单位: Hz
  float    initial_speed;        // 弹丸初速度, 单位: m/s
} shoot_data_t;

/* ID: 0x0208  Byte:  8    允许发弹量与剩余金币数(10Hz)    服务器->己方重装/步兵/哨兵 */
typedef struct __attribute__((packed))
{
  uint16_t projectile_allowance_17mm;     // 自身拥有的17mm弹丸允许发弹量
  uint16_t projectile_allowance_42mm;     // 42mm弹丸允许发弹量
  uint16_t remaining_gold_coin;           // 剩余金币数量
  uint16_t projectile_allowance_fortress; // 堡垒增益点提供的储备允许发弹量(与是否激活堡垒无关)
} projectile_allowance_t;

/* ID: 0x0209  Byte:  6    机器人RFID模块状态(3Hz)    服务器->己方装有RFID模块的机器人 */
typedef struct __attribute__((packed))
{
  uint32_t rfid_status;    // bit0己方补给区 bit1己方装配区 bit2R基地 bit3B基地 bit4R堡垒
                           // bit5B堡垒 bit6R中央高地 bit7B中央高地 bit8R前哨 bit9B前哨
                           // bit10己方制高点 bit11R高地控制区(仅RMUL) bit12B高地控制区(仅RMUL)
                           // bit13中央控制区(仅RMUL) bit14己方地形跨越(中央高地下方)
                           // bit15己方地形跨越(中央高地上方) bit16对方地形跨越(中央高地下方)
                           // bit17对方地形跨越(中央高地上方) bit18己方地形跨越(公路下方)
                           // bit19己方地形跨越(公路上方) bit20对方地形跨越(公路下方)
                           // bit21对方地形跨越(公路上方) bit22-24己方隧道(近己方公路侧 下/中/上)
                           // bit25-27隧道(近中央高地且近己方梯形高地 低/中/高)
                           // bit28-30隧道(远中央高地且远己方梯形高地 远/中/近)
  uint16_t rfid_status_2;  // bit0-2对方隧道(近对方公路侧 下/中/上)
                           // bit3-5隧道(近中央高地且近对方梯形高地 低/中/高)
                           // bit6-8隧道(远中央高地且远对方梯形高地 远/中/近)
} rfid_status_t;

/* ID: 0x020A  Byte:  6    飞镖选手端指令数据(3Hz)    服务器->己方飞镖机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  dart_launch_opening_status;  // 飞镖发射站状态: 1关闭 2正在开启或关闭中 0已开启
  uint8_t  reserved;                    // 保留位
  uint16_t target_change_time;          // 切换击打目标时的比赛剩余时间, 单位: s, 未切换为0
  uint16_t latest_launch_cmd_time;      // 最后一次确定发射指令时的比赛剩余时间, 单位: s
} dart_client_cmd_t;

/* ID: 0x020B  Byte: 40    地面机器人位置数据(1Hz)    服务器->己方哨兵机器人 */
typedef struct __attribute__((packed))
{
  float    heavyengineer_x;  // 己方重装机器人位置x坐标, 单位: m
  float    heavyengineer_y;  // 己方重装机器人位置y坐标, 单位: m
  float    reserved0;        // 保留位
  float    reserved1;        // 保留位
  float    standard_3_x;     // 己方3号步兵机器人位置x坐标, 单位: m
  float    standard_3_y;     // 己方3号步兵机器人位置y坐标, 单位: m
  float    standard_4_x;     // 己方4号步兵机器人位置x坐标, 单位: m
  float    standard_4_y;     // 己方4号步兵机器人位置y坐标, 单位: m
  float    reserved2;        // 保留位
  float    reserved3;        // 保留位
} ground_robot_position_t;

/* ID: 0x020C  Byte:  2    雷达标记进度数据(1Hz)    服务器->己方雷达机器人 */
typedef struct __attribute__((packed))
{
  uint16_t mark_progress;  // bit0对方1号重装易伤 bit1保留 bit2对方3号步兵易伤 bit3对方4号步兵易伤
                           // bit4保留 bit5对方哨兵易伤 bit6己方1号重装特殊标识 bit7保留
                           // bit8己方3号步兵特殊标识 bit9己方4号步兵特殊标识 bit10保留
                           // bit11己方哨兵特殊标识 bit12-15保留
                           // 对方: 标记进度>=100发1; 己方: 标记进度>=50发1
} radar_mark_data_t;

/* ID: 0x020D  Byte: 14    哨兵自主决策信息同步(1Hz)    服务器->己方哨兵机器人 */
typedef struct __attribute__((packed))
{
  uint32_t sentry_info;    // bit0-10 除远程兑换外成功兑换的允许发弹量
                           // bit11-14 成功远程兑换允许发弹量的次数
                           // bit15-18 成功远程兑换血量的次数
                           // bit19 当前是否可以确认免费复活(1可以)
                           // bit20 当前是否可以兑换立即复活(1可以)
                           // bit21-30 兑换立即复活需要花费的金币数
                           // bit31 保留
  uint16_t sentry_info_2;  // bit0 是否处于脱战状态(1脱战)
                           // bit1-11 队伍17mm允许发弹量剩余可兑换数
                           // bit12-13 哨兵姿态(1进攻 2防御 3移动)
                           // bit14 己方能量机关当前是否可进入正在激活状态
                           // bit15 当前姿态是否为强化姿态
  uint64_t sentry_info_3;  // bit0-7 进攻姿态弱化前剩余时长 bit8-15 防御姿态弱化前剩余时长
                           // bit16-23 移动姿态弱化前剩余时长 bit24-31 保留
                           // bit32-39 强化进攻姿态剩余时长 bit40-47 强化防御姿态剩余时长
                           // bit48-55 强化移动姿态剩余时长 bit56-63 保留
} sentry_info_t;

/* ID: 0x020E  Byte:  8    雷达自主决策信息同步(1Hz)    服务器->己方雷达机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  radar_info;        // bit0-1 雷达拥有触发双倍易伤的机会(最多2)
                              // bit2 对方是否正在被触发双倍易伤(1是)
                              // bit3-4 己方加密等级(即对方干扰波难度), 开局1, 最高3
                              // bit5 当前是否可以修改密钥(1可以)
  uint8_t  radar_info_2;      // bit0 当前是否存在生效中的装甲加固区域(1存在)
                              // bit1-3 雷达当前剩余装甲加固机会数
  uint16_t fortify_center_x;  // 当前生效的圆形加固区域圆心x坐标, 单位: mm
  uint16_t fortify_center_y;  // 当前生效的圆形加固区域圆心y坐标, 单位: mm
  uint16_t fortify_radius;    // 当前生效的圆形加固区域半径, 单位: mm
} radar_info_t;

/* ID: 0x0301  Byte: 118(max)    机器人交互数据(30Hz上限)    己方机器人之间互发 */
#define ROBOT_INTERACTION_MAX_LEN  112
typedef struct __attribute__((packed))
{
  uint16_t data_cmd_id;  // 子内容ID: 0x0200-0x02FF机器人之间通信 0x0100选手端删除图层
                         // 0x0101绘制1个图形 0x0102绘制2个图形 0x0103绘制5个图形
                         // 0x0104绘制7个图形 0x0110绘制字符 0x0120哨兵自主决策指令
                         // 0x0121雷达自主决策指令
  uint16_t sender_id;    // 发送者ID
  uint16_t receiver_id;  // 接收者ID
  uint8_t  user_data[ROBOT_INTERACTION_MAX_LEN];  // 内容数据段, 最大112字节
} robot_interaction_data_t;

/* ID: 0x0303  Byte: 12    选手端小地图交互数据(触发/1Hz)    选手端点击->服务器->发送方选择的己方机器人 */
typedef struct __attribute__((packed))
{
  float    target_position_x;  // 目标位置x轴坐标, 单位: m; 发送目标机器人ID时该值为0
  float    target_position_y;  // 目标位置y轴坐标, 单位: m; 发送目标机器人ID时该值为0
  uint8_t  cmd_keyboard;       // 云台手按下的键盘按键通用键值, 无按键为0
  uint8_t  target_robot_id;    // 对方机器人ID; 发送坐标数据时该值为0
  uint16_t cmd_source;         // 信息来源ID(选手端)
} map_command_t;

/* ID: 0x0310  Byte: 300    机器人发送给自定义终端的数据(50Hz上限)    机器人->图传链路->自定义终端 */
typedef struct __attribute__((packed))
{
  uint8_t  data[300];    // 自定义数据
} custom_robot_data_2_t;

/* ID: 0x0311  Byte: 60    自定义终端发送给机器人的自定义指令(75Hz上限)    自定义终端->图传链路->机器人 */
typedef struct __attribute__((packed))
{
  uint8_t  data[60];     // 自定义指令数据
} custom_robot_data_t;


typedef struct __attribute__((packed))
{
  frame_header_struct_t    FrameHeader;
  game_state_t             GameState;                // 0x0001
  game_result_t            GameResult;               // 0x0002
  game_robot_HP_t          GameRobotHP;              // 0x0003
  event_data_t             EventData;                // 0x0101
  referee_warning_t        RefereeWarning;           // 0x0104
  dart_info_t              DartInfo;                 // 0x0105
  game_robot_state_t       GameRobotState;           // 0x0201
  power_heat_data_t        PowerHeatData;            // 0x0202
  game_robot_pos_t         GameRobotPos;             // 0x0203
  buff_musk_t              BuffMusk;                 // 0x0204
  robot_hurt_t             RobotHurt;                // 0x0206
  shoot_data_t             ShootData;                // 0x0207
  projectile_allowance_t   ProjectileAllowance;      // 0x0208
  rfid_status_t            RfidStatus;               // 0x0209
  dart_client_cmd_t        DartClientCmd;            // 0x020A
  ground_robot_position_t  GroundRobotPosition;      // 0x020B
  radar_mark_data_t        RadarMarkData;            // 0x020C
  sentry_info_t            SentryInfo;               // 0x020D
  radar_info_t             RadarInfo;                // 0x020E
  robot_interaction_data_t InteractionData;          // 0x0301
  map_command_t            MapCommand;               // 0x0303
  custom_robot_data_2_t    robot_to_custom;          // 0x0310
  custom_robot_data_t      custom_to_robot;          // 0x0311

  int init_flag;

} data_referee_t;


/*裁判系统回包结构体*/
void referee_unpack_fifo_data(void);
void uart6_dma_Init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num);
void referee_Init();
void referee_read_data(uint8_t *frame);
#endif
