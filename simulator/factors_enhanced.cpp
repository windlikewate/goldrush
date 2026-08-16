// factors_enhanced.cpp — 增强因子评价实现
#include "factors_enhanced.h"
#include <cmath>
#include <algorithm>

// 定义增强权重 (前8项与 DEFAULT_WEIGHTS 一致，保证可退化优化)
const double ENHANCED_WEIGHTS[10] = {
    1.0,   // F1 淘金(目标变量)
    -0.08, // F2 避雷
    0.19,  // F3 探索
    -0.08, // F4 敌人距离
    0.08,  // F5 NPC
    0.08,  // F6 路径效率
    0.08,  // F7 位置优势
    0.55,  // F8 中心靠近
    // --- 以下为新增机制权重 ---
    0.35,  // F9 视野利用率因子 (正向：买的准得分高)
    0.25   // F10 行动力均衡因子 (正向：步数分配且有效利用度高)
};

EnhancedFactorValues computeEnhancedFactors(const GameInput* input, 
                                            const int actions[S], 
                                            int k, 
                                            int vp) {
    EnhancedFactorValues ef = {};

    // 1. 复用底层代码，计算原有的 F1 ~ F8 因子
    FactorValues base_f = computeFactors(input, actions, k);
    *static_cast<FactorValues*>(&ef) = base_f; // 向上转型拷贝基础数据

    // 2. ─── F9: 视野利用率因子 (Vision Efficiency Factor) ───
    // 评估当前视野内金币被采集的饱和度
    double empty_count = 0;
    double gold_count = 0;
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (input->grid[r][c] == 0) {
                empty_count++;
            } else if (input->grid[r][c] >= 1) {
                gold_count++;
            }
        }
    }
    
    // 饱和度定义：空地在(空地+已知金币)中的占比。饱和度越高，代表可视范围内的金币已经被采空
    double saturation = empty_count / (empty_count + gold_count + 1e-5);
    
    if (vp > 0) {
        // 如果决定买视野：饱和度越高（被采空了），买视野收益越大 -> 给正分
        ef.vision_efficiency = saturation; 
    } else {
        // 如果决定不买视野：饱和度越高（被采空了却不买），压制该动作 -> 给负分惩罚
        // 反之，如果饱和度低（视野里全是金币），-saturation 惩罚就很小
        ef.vision_efficiency = -saturation; 
    }

    // 3. ─── F10: 行动力均衡因子 (Action Balance Factor) ───
    // 评估当前 k 分配下，两个角色是否都处于有效移动状态
    UnitSimulation sim0 = simulateUnit(input, input->my_units[0], actions, 0, k);
    UnitSimulation sim1 = simulateUnit(input, input->my_units[1], actions, k, S - k);

    // 计算有效利用率 (实际移动次数 / 分配的步数)。避免分配了步数全用来撞墙(actual_moves不变)或原地发呆
    double util0 = (k == 0) ? 1.0 : (double)sim0.actual_moves / k;
    double util1 = ((S - k) == 0) ? 1.0 : (double)sim1.actual_moves / (S - k);

    // 均衡因子：取两者利用率的乘积。如果分配了步数却不移动（利用率为0），会大幅拖累整体得分
    ef.action_balance = util0 * util1;

    return ef;
}

double scoreEnhancedFactors(const EnhancedFactorValues& f, const double weights[10]) {
    // 基础因子得分
    double base_score = scoreFactors(f, weights); 
    
    // 追加新因子得分
    return base_score 
         + weights[8] * f.vision_efficiency
         + weights[9] * f.action_balance;
}