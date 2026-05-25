#pragma once

#include "game_types.hpp"

#include <array>
#include <string>
#include <vector>

inline std::vector<HeroMetaDef> BuildMetaHeroCatalog() {
    return {
        {u8"火焰法师", u8"主C", u8"法师", u8"火", u8"史诗", u8"烈焰爆破",
         u8"对群体敌人造成高额范围伤害，并额外灼烧虫群类敌人。",
         u8"每波开始时提升少量范围伤害。", u8"解锁燃烧，本波结束追加伤害。",
         u8"燃烧扩散，下一波开场受到少量灼烧。", u8"群体小怪多、波次多",
         C(227, 109, 75), C(255, 202, 109), 92, 12, 820, 110, RoleKind::FireMage},
        {u8"冰霜弓手", u8"控输出", u8"射手", u8"冰", u8"史诗", u8"寒霜箭雨",
         u8"伤害中等，但能降低敌人对城墙的伤害并强力克制疾行怪。",
         u8"普攻有概率附加减速。", u8"减速效果增强。",
         u8"有概率冻结敌人。", u8"疾行怪、守城压力高",
         C(132, 204, 235), C(232, 251, 255), 86, 11, 860, 120, RoleKind::FrostArcher},
        {u8"圣盾守卫", u8"防御", u8"守卫", u8"圣", u8"史诗", u8"圣盾庇护",
         u8"提高城墙最大生命并提供减伤，擅长稳定三星。",
         u8"开局提高城墙生命。", u8"每波开始获得小护盾。",
         u8"城墙首次受到致命伤害时保留生命并获得护盾。", u8"防御压力高、三星需求",
         C(98, 136, 163), C(232, 244, 247), 64, 10, 1020, 150, RoleKind::ShieldGuard},
        {u8"雷电术士", u8"副C", u8"术士", u8"雷", u8"稀有", u8"连锁闪电",
         u8"技能可连锁多个敌人，敌人越多收益越高。",
         u8"命中后有概率连锁。", u8"连锁次数增加。",
         u8"连锁概率提升，连锁衰减降低。", u8"群怪波次、飞行怪",
         C(106, 168, 232), C(255, 238, 104), 84, 11, 760, 100, RoleKind::ThunderWarlock},
        {u8"狂战士", u8"BossC", u8"战士", u8"物理", u8"史诗", u8"裂甲斩",
         u8"对高血量敌人和 Boss 有额外伤害，清杂较弱。",
         u8"对重甲怪伤害更高。", u8"敌人血量越低伤害越高。",
         u8"斩杀加成提高，Boss 首次技能必定暴击。", u8"Boss 关、重甲怪",
         C(205, 98, 81), C(255, 219, 98), 104, 13, 930, 150, RoleKind::Berserker},
        {u8"自然祭司", u8"辅助", u8"辅助", u8"自然", u8"稀有", u8"生命祈愿",
         u8"每波结束恢复城墙生命，并给全队少量攻击加成。",
         u8"波末恢复生命。", u8"治疗量提高。",
         u8"全队攻击加成提高，治疗溢出转化为护盾。", u8"长波次、补三星",
         C(119, 204, 144), C(238, 249, 224), 52, 8, 720, 90, RoleKind::NaturePriest},
    };
}

inline std::vector<StageInfo> BuildStageCatalog() {
    std::vector<std::string> chapterNames = {
        u8"新手平原", u8"虫群森林", u8"铁甲山谷", u8"疾风荒原", u8"深渊裂隙"
    };
    std::vector<std::string> bossNames = {
        u8"平原巨兽", u8"虫群母巢", u8"铁甲守卫", u8"疾风掠夺者", u8"深渊领主"
    };
    std::array<std::array<int, kStagesPerChapter>, kChapterCount> powerCurve{{
        {100, 120, 150, 190, 240, 300, 370, 450, 540, 680},
        {800, 930, 1080, 1250, 1450, 1680, 1950, 2250, 2600, 3100},
        {3600, 4150, 4800, 5500, 6300, 7200, 8200, 9400, 10800, 12500},
        {14500, 16800, 19400, 22400, 25800, 29700, 34200, 39400, 45300, 52000},
        {60000, 69000, 79000, 91000, 105000, 121000, 139000, 160000, 184000, 215000},
    }};
    std::array<std::array<std::string, kStagesPerChapter>, kChapterCount> stageNames{{
        {u8"启程草坡", u8"巡逻小径", u8"阵容训练", u8"风车路口", u8"碎片营地", u8"前哨坡道", u8"练兵山脚", u8"铁匠铺外", u8"巨兽足迹", u8"平原巨兽"},
        {u8"林缘虫鸣", u8"绿雾河岸", u8"幼虫潮", u8"树根围场", u8"虫巢精英", u8"腐叶岔路", u8"萤光巢道", u8"密林合围", u8"母巢门前", u8"虫群母巢"},
        {u8"铁屑坡", u8"锈谷入口", u8"矿车断轨", u8"重甲巡队", u8"铁卫精英", u8"熔炉栈桥", u8"钢脊窄道", u8"甲片高墙", u8"守卫广场", u8"铁甲守卫"},
        {u8"风沙驿站", u8"急风谷口", u8"疾行斥候", u8"沙脊冲锋", u8"荒原精英", u8"断旗营地", u8"快刃长路", u8"风暴边墙", u8"掠夺者营门", u8"疾风掠夺者"},
        {u8"裂隙浅层", u8"深渊回音", u8"混沌虫潮", u8"重甲裂道", u8"裂隙精英", u8"疾风暗影", u8"三相交锋", u8"深层压迫", u8"领主前厅", u8"深渊领主"},
    }};
    std::vector<StageInfo> stages;
    stages.reserve(kLevelCount);
    for (int chapter = 1; chapter <= kChapterCount; ++chapter) {
        for (int local = 1; local <= kStagesPerChapter; ++local) {
            int index = (chapter - 1) * kStagesPerChapter + (local - 1);
            StageType type = StageType::Normal;
            if (local == 5) type = StageType::Elite;
            if (local == 10) type = StageType::Boss;
            EnemyType enemy = EnemyType::Normal;
            std::string enemyName = u8"普通小怪";
            std::string tip = u8"均衡推进";
            if (chapter == 1) {
                enemy = local >= 6 ? EnemyType::Normal : EnemyType::Mixed;
                tip = u8"熟悉战斗和升级";
            } else if (chapter == 2) {
                enemy = (local >= 3) ? EnemyType::Swarm : EnemyType::Mixed;
                tip = u8"清怪阵容更有效";
            } else if (chapter == 3) {
                enemy = (local >= 4) ? EnemyType::Armored : EnemyType::Mixed;
                tip = u8"单体爆发更重要";
            } else if (chapter == 4) {
                enemy = (local >= 3) ? EnemyType::Swift : EnemyType::Mixed;
                tip = u8"控制和防守很关键";
            } else {
                enemy = (local >= 6) ? EnemyType::Boss : EnemyType::Mixed;
                tip = u8"综合检验全部养成";
            }
            if (type == StageType::Elite) {
                enemyName = u8"精英小队";
            } else if (type == StageType::Boss) {
                enemyName = bossNames[chapter - 1];
            } else if (enemy == EnemyType::Swarm) {
                enemyName = u8"群体小怪";
            } else if (enemy == EnemyType::Armored) {
                enemyName = u8"重甲怪";
            } else if (enemy == EnemyType::Swift) {
                enemyName = u8"疾行怪";
            } else if (enemy == EnemyType::Mixed) {
                enemyName = u8"混合敌群";
            }

            int recommended = powerCurve[chapter - 1][local - 1];
            int waves = chapter + 2 + (local >= 8 ? 1 : 0);
            if (type == StageType::Elite) waves += 1;
            if (type == StageType::Boss) waves += 2;
            ResourceBundle base{90 + chapter * 60 + local * 12, 70 + chapter * 42 + local * 8, 3 + chapter + local / 3, 0, 0, 0};
            ResourceBundle first = base;
            first.gold = static_cast<int>(first.gold * 1.8f);
            first.heroExp = static_cast<int>(first.heroExp * 1.5f);
            first.gearMat += 2 + chapter;
            if (type == StageType::Elite) {
                first.heroShard = 12 + chapter * 4;
                base.heroShard = 4 + chapter;
            } else if (type == StageType::Boss) {
                first.heroShard = 18 + chapter * 5;
                first.talentPoint = 1;
                first.diamond = 2 + chapter / 2;
                base.heroShard = 6 + chapter * 2;
                base.talentPoint = 0;
                base.diamond = 1;
            } else {
                base.heroShard = chapter >= 2 ? 1 : 0;
            }
            if (local == 10) {
                first.talentPoint = 1;
                if (chapter >= 2) first.diamond += 2;
            }
            float failRatio = type == StageType::Boss ? 0.22f : (type == StageType::Elite ? 0.28f : 0.25f);
            Color accent = chapter == 1 ? C(114, 199, 221) :
                           chapter == 2 ? C(132, 189, 145) :
                           chapter == 3 ? C(197, 142, 111) :
                           chapter == 4 ? C(168, 145, 216) :
                                          C(121, 193, 228);
            stages.push_back({
                index,
                chapter,
                local,
                chapterNames[chapter - 1],
                stageNames[chapter - 1][local - 1],
                type,
                enemy,
                enemyName,
                tip,
                recommended,
                waves,
                base,
                first,
                failRatio,
                accent,
            });
        }
    }
    return stages;
}

inline std::vector<EquipmentDef> BuildEquipmentCatalog() {
    return {
        {u8"武器", u8"提升全队攻击、技能伤害和 Boss 伤害",
         {u8"技能伤害 +5%", u8"Boss 伤害 +5%", u8"全队攻击 +3%", u8"前两波伤害 +8%"},
         C(230, 162, 96)},
        {u8"护甲", u8"提升城墙生命和减伤",
         {u8"城墙生命 +5%", u8"敌人伤害 -5%", u8"开局护盾", u8"低血减伤"},
         C(132, 177, 205)},
        {u8"饰品", u8"提升暴击、攻速和技能触发",
         {u8"技能触发率 +3%", u8"暴击率 +5%", u8"开局额外能量", u8"技能连发概率"},
         C(202, 139, 216)},
        {u8"宝物", u8"提供元素和流派加成",
         {u8"元素效果 +5%", u8"治疗和护盾 +5%", u8"三职业协同时全队提升", u8"每场随机强化1名角色"},
         C(136, 208, 176)},
    };
}

inline std::vector<TalentDef> BuildTalentCatalog() {
    return {
        {TalentBranch::Attack, 0, "atk_1", u8"全队攻击", u8"全队攻击 +3% / +6% / +9%", 3, -1, 0, false},
        {TalentBranch::Attack, 1, "atk_2", u8"技能强化", u8"所有角色技能伤害 +5% / +10%", 2, 0, 2, false},
        {TalentBranch::Attack, 2, "atk_3", u8"Boss伤害", u8"对 Boss 伤害 +8% / +16%", 2, 1, 1, false},
        {TalentBranch::Attack, 3, "atk_4", u8"开局压制", u8"前2波我方伤害 +15%", 1, 1, 1, true},
        {TalentBranch::Attack, 4, "atk_5", u8"暴击强化", u8"暴击伤害提高", 3, 2, 1, false},

        {TalentBranch::Defense, 0, "def_1", u8"城墙加固", u8"城墙生命 +5% / +10% / +15%", 3, -1, 0, false},
        {TalentBranch::Defense, 1, "def_2", u8"伤害减免", u8"敌人对城墙伤害 -3% / -6% / -9%", 3, 0, 1, false},
        {TalentBranch::Defense, 2, "def_3", u8"波次恢复", u8"每波结束恢复城墙2% / 4%", 2, 1, 2, false},
        {TalentBranch::Defense, 3, "def_4", u8"濒危护盾", u8"城墙低于30%时获得护盾", 1, 1, 2, true},
        {TalentBranch::Defense, 4, "def_5", u8"坚守意志", u8"每场第一次致命伤害不会失败", 1, 3, 1, true},

        {TalentBranch::Resource, 0, "res_1", u8"金币收益", u8"通关金币 +5% / +10% / +15%", 3, -1, 0, false},
        {TalentBranch::Resource, 1, "res_2", u8"经验收益", u8"角色经验 +5% / +10%", 2, 0, 2, false},
        {TalentBranch::Resource, 2, "res_3", u8"材料收益", u8"装备材料 +5% / +10%", 2, 0, 2, false},
        {TalentBranch::Resource, 3, "res_4", u8"失败补偿", u8"失败奖励提升到35%", 1, 2, 1, true},
        {TalentBranch::Resource, 4, "res_5", u8"首通强化", u8"普通关首通金币额外 +20%", 1, 1, 1, true},

        {TalentBranch::Class, 0, "cls_1", u8"法师专精", u8"法师伤害 +5% / +10%", 2, -1, 0, false},
        {TalentBranch::Class, 1, "cls_2", u8"射手专精", u8"射手控制效果 +5% / +10%", 2, 0, 1, false},
        {TalentBranch::Class, 2, "cls_3", u8"战士专精", u8"战士对 Boss 伤害 +5% / +10%", 2, 0, 1, false},
        {TalentBranch::Class, 3, "cls_4", u8"辅助专精", u8"治疗和护盾效果 +5% / +10%", 2, 1, 1, false},
        {TalentBranch::Class, 4, "cls_5", u8"多职业协同", u8"上阵3个不同职业时全队攻击和生命 +5%", 1, 2, 1, true},
    };
}
