//閫傞厤2027閫氳鍗忚V1.0.0
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
#define REF_PROTOCOL_FRAME_MAX_SIZE         320//0x0310鏈?300瀛楄妭
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


/*瑁佸垽绯荤粺鍥炲寘缁撴瀯浣?*/

/* ID: 0x0001  Byte: 11    姣旇禌鐘舵?佹暟鎹?(1Hz)    鏈嶅姟鍣?->鍏ㄤ綋鏈哄櫒浜? */
typedef  struct __attribute__((packed))
{
  uint8_t  game_type : 4;      // bit0-3 姣旇禌绫诲瀷: 1瓒呯骇瀵规姉璧? 2楂樻牎鍗曢」璧? 3ICRA 4楂樻牎鑱旂洘璧?3V3 5楂樻牎鑱旂洘璧涙鍏靛鎶?
  uint8_t  game_progress : 4;  // bit4-7 姣旇禌闃舵: 0鏈紑濮? 1鍑嗗闃舵 2鍗佷簲绉掕嚜妫? 3浜旂鍊掕鏃? 4姣旇禌涓? 5缁撶畻涓?
  uint16_t stage_remain_time;  // 褰撳墠闃舵鍓╀綑鏃堕棿, 鍗曚綅: s
  uint64_t SyncTimeStamp;      // UNIX鏃堕棿, 杩炰笂NTP鏈嶅姟鍣ㄥ悗鐢熸晥
} game_state_t;

/* ID: 0x0002  Byte:  1    姣旇禌缁撴灉鏁版嵁(姣旇禌缁撴潫)    鏈嶅姟鍣?->鍏ㄤ綋鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  winner;             // 0骞冲眬 1绾㈡柟鑳滃埄 2钃濇柟鑳滃埄
} game_result_t;

/* ID: 0x0003  Byte: 24    鏈哄櫒浜鸿閲忔暟鎹?(3Hz)    鏈嶅姟鍣?->鍏ㄤ綋鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint16_t ally_1_robot_HP;     // 宸辨柟1鍙烽噸瑁呮満鍣ㄤ汉琛?閲?, 鏈笂鍦?/琚綒涓嬩负0
  uint16_t reserved;            // 淇濈暀浣?(宸辨柟2鍙?)
  uint16_t ally_3_robot_HP;     // 宸辨柟3鍙锋鍏垫満鍣ㄤ汉琛?閲?
  uint16_t ally_4_robot_HP;     // 宸辨柟4鍙锋鍏垫満鍣ㄤ汉琛?閲?
  int16_t  damage_difference;   // 宸辨柟鍏ㄩ槦鎬讳激瀹充笌瀵规柟鍏ㄩ槦鎬讳激瀹充箣宸?
  uint16_t ally_7_robot_HP;     // 宸辨柟7鍙峰摠鍏垫満鍣ㄤ汉琛?閲?
  uint16_t outpost_R_HP;        // R鍓嶅摠琛?閲?
  uint16_t ally_base_HP;        // 宸辨柟鍩哄湴琛?閲?
  uint16_t outpost_B_HP;        // B鍓嶅摠琛?閲?
  uint16_t enemy_base_HP;       // 瀵规柟鍩哄湴琛?閲?
  uint16_t ally_control_degree; // 宸辨柟绱鎺у埗搴?
  uint16_t enemy_control_degree;// 瀵规柟绱鎺у埗搴?
} game_robot_HP_t;

/* ID: 0x0101  Byte:  8    鍦哄湴浜嬩欢鏁版嵁(1Hz)    鏈嶅姟鍣?->宸辨柟鍏ㄤ綋鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint32_t event_data;    // bit0宸辨柟琛ョ粰鍖烘縺娲? bit1-2宸辨柟灏忚兘閲忔満鍏?(0鏈縺娲? 1宸叉縺娲? 2姝ｅ湪婵?娲?)
                          // bit3-4宸辨柟澶ц兘閲忔満鍏?(鍚屼笂) bit5-7 R鍫″瀿澧炵泭鐐?(0鏈縺娲? 1宸辨柟 2瀵规柟 3鍙屾柟)
                          // bit8-10 B鍫″瀿澧炵泭鐐? bit11-13 R鍩哄湴澧炵泭鐐? bit14-16 B鍩哄湴澧炵泭鐐?
                          // bit17宸辨柟鍒堕珮鐐? bit18-19 R涓ぎ楂樺湴(0鏈縺娲? 1宸辨柟 2瀵规柟)
                          // bit20-21 B涓ぎ楂樺湴 bit22-24 R鍓嶅摠澧炵泭鐐? bit25-27 B鍓嶅摠澧炵泭鐐?
                          // bit28-29 R鍓嶅摠闃靛湴鎺у埗(0宸辨柟瀹屽叏鎺у埗 1宸辨柟涓存椂鎺у埗 2宸辨柟鏈帶鍒?)
                          // bit30-31 B鍓嶅摠闃靛湴鎺у埗
  uint32_t event_data_2;  // bit0-1 R鍩哄湴闃靛湴鎺у埗(0宸辨柟瀹屽叏鎺у埗 1宸辨柟涓存椂鎺у埗 2宸辨柟鏈帶鍒?)
                          // bit2-3 B鍩哄湴闃靛湴鎺у埗
                          // bit4-12 瀵规柟椋為晼鏈?鍚庝竴娆″嚮涓繁鏂瑰墠鍝?/鍩哄湴鐨勬椂闂?, 鍗曚綅s, 0-420
                          // bit13-15 瀵规柟椋為晼鏈?鍚庡嚮涓殑鍏蜂綋鐩爣(0寮?灞?榛樿 1R鍓嶅摠 2鍩哄湴鍥哄畾
                          //         3鍩哄湴闅忔満鍥哄畾 4鍩哄湴闅忔満绉诲姩 5鍩哄湴鏈绉诲姩 6B鍓嶅摠)
                          // bit16-17 R楂樺湴鎺у埗鍖烘縺娲?(浠匯MUL) bit18-19 R楂樺湴鎺у埗鍖烘帶鍒剁姸鎬?(浠匯MUL)
                          // bit20-23 B楂樺湴鎺у埗鍖烘縺娲?/鎺у埗鐘舵??(浠匯MUL)
                          // bit24-27 涓ぎ鎺у埗鍖烘縺娲?/鎺у埗鐘舵??(浠匯MUL)
} event_data_t;

/* ID: 0x0104  Byte:  3    瑁佸垽璀﹀憡鏁版嵁(瑙﹀彂/1Hz)    鏈嶅姟鍣?->琚垽缃氭柟鍏ㄤ綋鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  level;               // 宸辨柟鏈?鍚庡彈缃氱瓑绾?: 1鍙屾柟榛勭墝 2榛勭墝 3绾㈢墝 4鍒よ礋
  uint8_t  offending_robot_id;  // 杩濊鏈哄櫒浜篒D(绾?1涓?1, 钃?1涓?101), 鍒よ礋/鍙屾柟榛勭墝鏃朵负0
  uint8_t  count;               // 璇ョ瓑绾ц繚瑙勬鏁?, 寮?灞?涓?0
} referee_warning_t;

/* ID: 0x0105  Byte:  3    椋為晼鍙戝皠鐩稿叧鏁版嵁(1Hz)    鏈嶅姟鍣?->宸辨柟鍏ㄤ綋鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  dart_remaining_time;  // 宸辨柟椋為晼鍙戝皠鍓╀綑鏃堕棿, 鍗曚綅: s
  uint16_t dart_info;            // bit0-2 鏈?杩戜竴娆″繁鏂归闀栧嚮涓洰鏍?(0榛樿 1R鍓嶅摠 2鍩哄湴鍥哄畾 3鍩哄湴闅忔満鍥哄畾
                                 //       4鍩哄湴闅忔満绉诲姩 5鍩哄湴鏈绉诲姩 6B鍓嶅摠)
                                 // bit3-5 瀵规柟鏈?杩戣鍑讳腑鐩爣绱娆℃暟(鏈?澶?4)
                                 // bit6-8 椋為晼褰撳墠閫夊畾鐨勫嚮鎵撶洰鏍?(0榛樿/鍓嶅摠绔? 1鍩哄湴鍥哄畾 2鍩哄湴闅忔満鍥哄畾
                                 //       3鍩哄湴闅忔満绉诲姩 4鍩哄湴鏈绉诲姩)
                                 // bit9-15 淇濈暀
} dart_info_t;

/* ID: 0x0201  Byte: 17    鏈哄櫒浜烘?ц兘浣撶郴鏁版嵁(10Hz)    涓绘帶妯″潡->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  robot_id;                       // 鏈満鍣ㄤ汉ID
  uint8_t  robot_level;                    // 鏈哄櫒浜虹瓑绾?
  uint16_t current_HP;                     // 鏈哄櫒浜哄綋鍓嶈閲?
  uint16_t maximum_HP;                     // 鏈哄櫒浜鸿閲忎笂闄?
  uint16_t shooter_barrel_cooling_value;   // 灏勫嚮鐑噺姣忕鍐峰嵈鍊?
  uint16_t shooter_barrel_heat_limit;      // 灏勫嚮鐑噺涓婇檺
  uint16_t chassis_power_limit;            // 搴曠洏鍔熺巼涓婇檺
  float    bullet_speed_limit;             // 灏勫嚮鍒濋?熷害涓婇檺, 鍗曚綅: m/s
  uint8_t  power_management_gimbal_output : 1;   // 鐢垫簮绠＄悊gimbal鍙ｈ緭鍑?: 0鏃犺緭鍑? 1鏈?24V杈撳嚭
  uint8_t  power_management_chassis_output : 1;  // 鐢垫簮绠＄悊chassis鍙ｈ緭鍑?: 0鏃犺緭鍑? 1鏈?24V杈撳嚭
  uint8_t  power_management_shooter_output : 1;  // 鐢垫簮绠＄悊shooter鍙ｈ緭鍑?: 0鏃犺緭鍑? 1鏈?24V杈撳嚭
} game_robot_state_t;

/* ID: 0X0202  Byte: 14    瀹炴椂搴曠洏缂撳啿鑳介噺鍜屽皠鍑荤儹閲忔暟鎹?(10Hz)    涓绘帶妯″潡->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint16_t reserved0;                 // 淇濈暀浣?(鍘熷簳鐩樼數鍘?)
  uint16_t reserved1;                 // 淇濈暀浣?(鍘熷簳鐩樼數娴?)
  float    reserved2;                 // 淇濈暀浣?(鍘熷簳鐩樺姛鐜?)
  uint16_t buffer_energy;             // 缂撳啿鑳介噺, 鍗曚綅: J
  uint16_t shooter_17mm_barrel_heat;  // 17mm鍙戝皠鏈烘瀯鐨勫皠鍑荤儹閲?
  uint16_t shooter_42mm_barrel_heat;  // 42mm鍙戝皠鏈烘瀯鐨勫皠鍑荤儹閲?
} power_heat_data_t;

/* ID: 0x0203  Byte: 12    鏈哄櫒浜轰綅缃暟鎹?(1Hz)    涓绘帶妯″潡->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  float    x;      // 鏈満鍣ㄤ汉浣嶇疆x鍧愭爣, 鍗曚綅: m
  float    y;      // 鏈満鍣ㄤ汉浣嶇疆y鍧愭爣, 鍗曚綅: m
  float    angle;  // 娴嬮?熸ā鍧楁湞鍚?, 鍗曚綅: 搴?, 姝ｅ寳涓?0搴?
} game_robot_pos_t;

/* ID: 0x0204  Byte:  7    鏈哄櫒浜哄鐩婃暟鎹?(3Hz)    鏈嶅姟鍣?->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  recovery_buff;       // 鍥炶澧炵泭鐧惧垎姣?, 10琛ㄧず姣忕鎭㈠琛?閲忎笂闄愮殑10%
  uint16_t cooling_buff;        // 灏勫嚮鐑噺鍐峰嵈澧炵泭鍏蜂綋鍊?, x琛ㄧず鍐峰嵈澧炲姞x/s
  uint8_t  defence_buff;        // 闃插尽澧炵泭鐧惧垎姣?, 50琛ㄧず50%闃插尽
  uint8_t  vulnerability_buff;  // 璐熼槻寰″鐩婄櫨鍒嗘瘮, 30琛ㄧず-30%闃插尽
  uint16_t attack_buff;         // 鏀诲嚮澧炵泭鐧惧垎姣?, 50琛ㄧず50%鏀诲嚮
} buff_musk_t;

/* ID: 0x0206  Byte:  7    浼ゅ鐘舵?佹暟鎹?(浼ゅ鍙戠敓鍚?)    涓绘帶妯″潡->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  armor_id : 4;             // 琚嚮/鎾炲嚮/绂荤嚎鐨勮鐢叉ā鍧楁垨娴嬮?熸ā鍧桰D缂栧彿, 鍏朵粬鍘熷洜鎵ｈ涓?0
  uint8_t  HP_deduction_reason : 4;  // 琛?閲忓彉鍖栫被鍨?: 0瑁呯敳琚脊涓告敾鍑? 1瑁呯敳妯″潡绂荤嚎
                                     //               2瓒呯骇鐢靛绠＄悊妯″潡绂荤嚎 5瑁呯敳鍙楀埌鎾炲嚮
  uint16_t hit_sequence;             // 鍛戒腑浜嬩欢搴忓彿, 姣忓彂鍛戒腑鍚庨?掑1
  int16_t  hit_position_x;           // 鍛戒腑浣嶇疆x鍧愭爣, 鍗曚綅: mm, 浠ヨ鐢叉娴嬮潰宸︿笅瑙掍负鍘熺偣
  int16_t  hit_position_y;           // 鍛戒腑浣嶇疆y鍧愭爣, 鍗曚綅: mm
} robot_hurt_t;

/* ID: 0x0207  Byte:  7    瀹炴椂灏勫嚮鏁版嵁(寮逛父鍙戝皠鍚?)    涓绘帶妯″潡->瀵瑰簲鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  bullet_type;          // 寮逛父绫诲瀷: 1涓?17mm寮逛父 2涓?42mm寮逛父
  uint8_t  shooter_number;       // 鍙戝皠鏈烘瀯ID: 1涓?17mm鍙戝皠鏈烘瀯 2淇濈暀 3涓?42mm鍙戝皠鏈烘瀯
  uint8_t  launching_frequency;  // 寮逛父灏勯??, 鍗曚綅: Hz
  float    initial_speed;        // 寮逛父鍒濋?熷害, 鍗曚綅: m/s
} shoot_data_t;

/* ID: 0x0208  Byte:  8    鍏佽鍙戝脊閲忎笌鍓╀綑閲戝竵鏁?(10Hz)    鏈嶅姟鍣?->宸辨柟閲嶈/姝ュ叺/鍝ㄥ叺 */
typedef struct __attribute__((packed))
{
  uint16_t projectile_allowance_17mm;     // 鑷韩鎷ユ湁鐨?17mm寮逛父鍏佽鍙戝脊閲?
  uint16_t projectile_allowance_42mm;     // 42mm寮逛父鍏佽鍙戝脊閲?
  uint16_t remaining_gold_coin;           // 鍓╀綑閲戝竵鏁伴噺
  uint16_t projectile_allowance_fortress; // 鍫″瀿澧炵泭鐐规彁渚涚殑鍌ㄥ鍏佽鍙戝脊閲?(涓庢槸鍚︽縺娲诲牎鍨掓棤鍏?)
} projectile_allowance_t;

/* ID: 0x0209  Byte:  6    鏈哄櫒浜篟FID妯″潡鐘舵??(3Hz)    鏈嶅姟鍣?->宸辨柟瑁呮湁RFID妯″潡鐨勬満鍣ㄤ汉 */
typedef struct __attribute__((packed))
{
  uint32_t rfid_status;    // bit0宸辨柟琛ョ粰鍖? bit1宸辨柟瑁呴厤鍖? bit2R鍩哄湴 bit3B鍩哄湴 bit4R鍫″瀿
                           // bit5B鍫″瀿 bit6R涓ぎ楂樺湴 bit7B涓ぎ楂樺湴 bit8R鍓嶅摠 bit9B鍓嶅摠
                           // bit10宸辨柟鍒堕珮鐐? bit11R楂樺湴鎺у埗鍖?(浠匯MUL) bit12B楂樺湴鎺у埗鍖?(浠匯MUL)
                           // bit13涓ぎ鎺у埗鍖?(浠匯MUL) bit14宸辨柟鍦板舰璺ㄨ秺(涓ぎ楂樺湴涓嬫柟)
                           // bit15宸辨柟鍦板舰璺ㄨ秺(涓ぎ楂樺湴涓婃柟) bit16瀵规柟鍦板舰璺ㄨ秺(涓ぎ楂樺湴涓嬫柟)
                           // bit17瀵规柟鍦板舰璺ㄨ秺(涓ぎ楂樺湴涓婃柟) bit18宸辨柟鍦板舰璺ㄨ秺(鍏矾涓嬫柟)
                           // bit19宸辨柟鍦板舰璺ㄨ秺(鍏矾涓婃柟) bit20瀵规柟鍦板舰璺ㄨ秺(鍏矾涓嬫柟)
                           // bit21瀵规柟鍦板舰璺ㄨ秺(鍏矾涓婃柟) bit22-24宸辨柟闅ч亾(杩戝繁鏂瑰叕璺晶 涓?/涓?/涓?)
                           // bit25-27闅ч亾(杩戜腑澶珮鍦颁笖杩戝繁鏂规褰㈤珮鍦? 浣?/涓?/楂?)
                           // bit28-30闅ч亾(杩滀腑澶珮鍦颁笖杩滃繁鏂规褰㈤珮鍦? 杩?/涓?/杩?)
  uint16_t rfid_status_2;  // bit0-2瀵规柟闅ч亾(杩戝鏂瑰叕璺晶 涓?/涓?/涓?)
                           // bit3-5闅ч亾(杩戜腑澶珮鍦颁笖杩戝鏂规褰㈤珮鍦? 浣?/涓?/楂?)
                           // bit6-8闅ч亾(杩滀腑澶珮鍦颁笖杩滃鏂规褰㈤珮鍦? 杩?/涓?/杩?)
} rfid_status_t;

/* ID: 0x020A  Byte:  6    椋為晼閫夋墜绔寚浠ゆ暟鎹?(3Hz)    鏈嶅姟鍣?->宸辨柟椋為晼鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  dart_launch_opening_status;  // 椋為晼鍙戝皠绔欑姸鎬?: 1鍏抽棴 2姝ｅ湪寮?鍚垨鍏抽棴涓? 0宸插紑鍚?
  uint8_t  reserved;                    // 淇濈暀浣?
  uint16_t target_change_time;          // 鍒囨崲鍑绘墦鐩爣鏃剁殑姣旇禌鍓╀綑鏃堕棿, 鍗曚綅: s, 鏈垏鎹负0
  uint16_t latest_launch_cmd_time;      // 鏈?鍚庝竴娆＄‘瀹氬彂灏勬寚浠ゆ椂鐨勬瘮璧涘墿浣欐椂闂?, 鍗曚綅: s
} dart_client_cmd_t;

/* ID: 0x020B  Byte: 40    鍦伴潰鏈哄櫒浜轰綅缃暟鎹?(1Hz)    鏈嶅姟鍣?->宸辨柟鍝ㄥ叺鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  float    heavyengineer_x;  // 宸辨柟閲嶈鏈哄櫒浜轰綅缃畑鍧愭爣, 鍗曚綅: m
  float    heavyengineer_y;  // 宸辨柟閲嶈鏈哄櫒浜轰綅缃畒鍧愭爣, 鍗曚綅: m
  float    reserved0;        // 淇濈暀浣?
  float    reserved1;        // 淇濈暀浣?
  float    standard_3_x;     // 宸辨柟3鍙锋鍏垫満鍣ㄤ汉浣嶇疆x鍧愭爣, 鍗曚綅: m
  float    standard_3_y;     // 宸辨柟3鍙锋鍏垫満鍣ㄤ汉浣嶇疆y鍧愭爣, 鍗曚綅: m
  float    standard_4_x;     // 宸辨柟4鍙锋鍏垫満鍣ㄤ汉浣嶇疆x鍧愭爣, 鍗曚綅: m
  float    standard_4_y;     // 宸辨柟4鍙锋鍏垫満鍣ㄤ汉浣嶇疆y鍧愭爣, 鍗曚綅: m
  float    reserved2;        // 淇濈暀浣?
  float    reserved3;        // 淇濈暀浣?
} ground_robot_position_t;

/* ID: 0x020C  Byte:  2    闆疯揪鏍囪杩涘害鏁版嵁(1Hz)    鏈嶅姟鍣?->宸辨柟闆疯揪鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint16_t mark_progress;  // bit0瀵规柟1鍙烽噸瑁呮槗浼? bit1淇濈暀 bit2瀵规柟3鍙锋鍏垫槗浼? bit3瀵规柟4鍙锋鍏垫槗浼?
                           // bit4淇濈暀 bit5瀵规柟鍝ㄥ叺鏄撲激 bit6宸辨柟1鍙烽噸瑁呯壒娈婃爣璇? bit7淇濈暀
                           // bit8宸辨柟3鍙锋鍏电壒娈婃爣璇? bit9宸辨柟4鍙锋鍏电壒娈婃爣璇? bit10淇濈暀
                           // bit11宸辨柟鍝ㄥ叺鐗规畩鏍囪瘑 bit12-15淇濈暀
                           // 瀵规柟: 鏍囪杩涘害>=100鍙?1; 宸辨柟: 鏍囪杩涘害>=50鍙?1
} radar_mark_data_t;

/* ID: 0x020D  Byte: 14    鍝ㄥ叺鑷富鍐崇瓥淇℃伅鍚屾(1Hz)    鏈嶅姟鍣?->宸辨柟鍝ㄥ叺鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint32_t sentry_info;    // bit0-10 闄よ繙绋嬪厬鎹㈠鎴愬姛鍏戞崲鐨勫厑璁稿彂寮归噺
                           // bit11-14 鎴愬姛杩滅▼鍏戞崲鍏佽鍙戝脊閲忕殑娆℃暟
                           // bit15-18 鎴愬姛杩滅▼鍏戞崲琛?閲忕殑娆℃暟
                           // bit19 褰撳墠鏄惁鍙互纭鍏嶈垂澶嶆椿(1鍙互)
                           // bit20 褰撳墠鏄惁鍙互鍏戞崲绔嬪嵆澶嶆椿(1鍙互)
                           // bit21-30 鍏戞崲绔嬪嵆澶嶆椿闇?瑕佽姳璐圭殑閲戝竵鏁?
                           // bit31 淇濈暀
  uint16_t sentry_info_2;  // bit0 鏄惁澶勪簬鑴辨垬鐘舵??(1鑴辨垬)
                           // bit1-11 闃熶紞17mm鍏佽鍙戝脊閲忓墿浣欏彲鍏戞崲鏁?
                           // bit12-13 鍝ㄥ叺濮挎??(1杩涙敾 2闃插尽 3绉诲姩)
                           // bit14 宸辨柟鑳介噺鏈哄叧褰撳墠鏄惁鍙繘鍏ユ鍦ㄦ縺娲荤姸鎬?
                           // bit15 褰撳墠濮挎?佹槸鍚︿负寮哄寲濮挎??
  uint64_t sentry_info_3;  // bit0-7 杩涙敾濮挎?佸急鍖栧墠鍓╀綑鏃堕暱 bit8-15 闃插尽濮挎?佸急鍖栧墠鍓╀綑鏃堕暱
                           // bit16-23 绉诲姩濮挎?佸急鍖栧墠鍓╀綑鏃堕暱 bit24-31 淇濈暀
                           // bit32-39 寮哄寲杩涙敾濮挎?佸墿浣欐椂闀? bit40-47 寮哄寲闃插尽濮挎?佸墿浣欐椂闀?
                           // bit48-55 寮哄寲绉诲姩濮挎?佸墿浣欐椂闀? bit56-63 淇濈暀
} sentry_info_t;

/* ID: 0x020E  Byte:  8    闆疯揪鑷富鍐崇瓥淇℃伅鍚屾(1Hz)    鏈嶅姟鍣?->宸辨柟闆疯揪鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  radar_info;        // bit0-1 闆疯揪鎷ユ湁瑙﹀彂鍙屽?嶆槗浼ょ殑鏈轰細(鏈?澶?2)
                              // bit2 瀵规柟鏄惁姝ｅ湪琚Е鍙戝弻鍊嶆槗浼?(1鏄?)
                              // bit3-4 宸辨柟鍔犲瘑绛夌骇(鍗冲鏂瑰共鎵版尝闅惧害), 寮?灞?1, 鏈?楂?3
                              // bit5 褰撳墠鏄惁鍙互淇敼瀵嗛挜(1鍙互)
  uint8_t  radar_info_2;      // bit0 褰撳墠鏄惁瀛樺湪鐢熸晥涓殑瑁呯敳鍔犲浐鍖哄煙(1瀛樺湪)
                              // bit1-3 闆疯揪褰撳墠鍓╀綑瑁呯敳鍔犲浐鏈轰細鏁?
  uint16_t fortify_center_x;  // 褰撳墠鐢熸晥鐨勫渾褰㈠姞鍥哄尯鍩熷渾蹇儀鍧愭爣, 鍗曚綅: mm
  uint16_t fortify_center_y;  // 褰撳墠鐢熸晥鐨勫渾褰㈠姞鍥哄尯鍩熷渾蹇儁鍧愭爣, 鍗曚綅: mm
  uint16_t fortify_radius;    // 褰撳墠鐢熸晥鐨勫渾褰㈠姞鍥哄尯鍩熷崐寰?, 鍗曚綅: mm
} radar_info_t;

/* ID: 0x0301  Byte: 118(max)    鏈哄櫒浜轰氦浜掓暟鎹?(30Hz涓婇檺)    宸辨柟鏈哄櫒浜轰箣闂翠簰鍙? */
#define ROBOT_INTERACTION_MAX_LEN  112
typedef struct __attribute__((packed))
{
  uint16_t data_cmd_id;  // 瀛愬唴瀹笽D: 0x0200-0x02FF鏈哄櫒浜轰箣闂撮?氫俊 0x0100閫夋墜绔垹闄ゅ浘灞?
                         // 0x0101缁樺埗1涓浘褰? 0x0102缁樺埗2涓浘褰? 0x0103缁樺埗5涓浘褰?
                         // 0x0104缁樺埗7涓浘褰? 0x0110缁樺埗瀛楃 0x0120鍝ㄥ叺鑷富鍐崇瓥鎸囦护
                         // 0x0121闆疯揪鑷富鍐崇瓥鎸囦护
  uint16_t sender_id;    // 鍙戦?佽?匢D
  uint16_t receiver_id;  // 鎺ユ敹鑰匢D
  uint8_t  user_data[ROBOT_INTERACTION_MAX_LEN];  // 鍐呭鏁版嵁娈?, 鏈?澶?112瀛楄妭
} robot_interaction_data_t;

/* ID: 0x0303  Byte: 12    閫夋墜绔皬鍦板浘浜や簰鏁版嵁(瑙﹀彂/1Hz)    閫夋墜绔偣鍑?->鏈嶅姟鍣?->鍙戦?佹柟閫夋嫨鐨勫繁鏂规満鍣ㄤ汉 */
typedef struct __attribute__((packed))
{
  float    target_position_x;  // 鐩爣浣嶇疆x杞村潗鏍?, 鍗曚綅: m; 鍙戦?佺洰鏍囨満鍣ㄤ汉ID鏃惰鍊间负0
  float    target_position_y;  // 鐩爣浣嶇疆y杞村潗鏍?, 鍗曚綅: m; 鍙戦?佺洰鏍囨満鍣ㄤ汉ID鏃惰鍊间负0
  uint8_t  cmd_keyboard;       // 浜戝彴鎵嬫寜涓嬬殑閿洏鎸夐敭閫氱敤閿??, 鏃犳寜閿负0
  uint8_t  target_robot_id;    // 瀵规柟鏈哄櫒浜篒D; 鍙戦?佸潗鏍囨暟鎹椂璇ュ?间负0
  uint16_t cmd_source;         // 淇℃伅鏉ユ簮ID(閫夋墜绔?)
} map_command_t;

/* ID: 0x0310  Byte: 300    鏈哄櫒浜哄彂閫佺粰鑷畾涔夌粓绔殑鏁版嵁(50Hz涓婇檺)    鏈哄櫒浜?->鍥句紶閾捐矾->鑷畾涔夌粓绔? */
typedef struct __attribute__((packed))
{
  uint8_t  data[300];    // 鑷畾涔夋暟鎹?
} custom_robot_data_2_t;

/* ID: 0x0311  Byte: 60    鑷畾涔夌粓绔彂閫佺粰鏈哄櫒浜虹殑鑷畾涔夋寚浠?(75Hz涓婇檺)    鑷畾涔夌粓绔?->鍥句紶閾捐矾->鏈哄櫒浜? */
typedef struct __attribute__((packed))
{
  uint8_t  data[60];     // 鑷畾涔夋寚浠ゆ暟鎹?
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


/*瑁佸垽绯荤粺鍥炲寘缁撴瀯浣?*/
void referee_unpack_fifo_data(void);
void uart6_dma_Init(uint8_t *rx1_buf, uint8_t *rx2_buf, uint16_t dma_buf_num);
void referee_Init();
void referee_read_data(uint8_t *frame);
#endif
