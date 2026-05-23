#pragma once

#include "game_types.hpp"

#include <algorithm>
#include <string>
#include <vector>

inline std::vector<HeroDef> BuildHeroCatalog() {
    return {
        {u8"强袭", u8"射手", "hero_assault", C(92, 162, 205), C(224, 244, 255), C(255, 225, 82), AttackStyle::Arrow, HeroSkill::Assault, 44.0f, 0.64f, 486.0f, 16.0f},
        {u8"剑仙", u8"射手", "hero_sword_immortal", C(114, 182, 196), C(232, 246, 238), C(178, 239, 255), AttackStyle::Arrow, HeroSkill::SwordImmortal, 58.0f, 0.92f, 520.0f, 0.0f},
        {u8"钢铁汪", u8"射手", "hero_steel_dog", C(112, 132, 153), C(255, 211, 89), C(255, 109, 78), AttackStyle::Laser, HeroSkill::SteelDog, 70.0f, 1.18f, 500.0f, 34.0f},
        {u8"火箭", u8"射手", "hero_rocket", C(202, 112, 70), C(255, 218, 92), C(255, 163, 68), AttackStyle::Bomb, HeroSkill::Rocket, 47.0f, 0.78f, 455.0f, 38.0f},
        {u8"蘑菇头", u8"射手/毒", "hero_mushroom", C(151, 97, 185), C(205, 240, 119), C(157, 237, 93), AttackStyle::Poison, HeroSkill::Mushroom, 36.0f, 0.82f, 452.0f, 20.0f},
        {u8"游侠", u8"射手", "hero_ranger", C(97, 157, 74), C(224, 236, 154), C(250, 225, 86), AttackStyle::Arrow, HeroSkill::Ranger, 42.0f, 0.82f, 500.0f, 0.0f},
        {u8"小炮", u8"射手", "hero_cannon", C(94, 126, 171), C(242, 184, 85), C(255, 184, 70), AttackStyle::Bomb, HeroSkill::Cannon, 49.0f, 1.02f, 430.0f, 58.0f},
        {u8"孙悟空", u8"战士", "hero_wukong", C(208, 128, 66), C(255, 222, 83), C(255, 176, 66), AttackStyle::Slash, HeroSkill::Wukong, 84.0f, 1.18f, 396.0f, 38.0f},
        {u8"哪吒", u8"战士", "hero_nezha", C(220, 86, 66), C(255, 218, 83), C(255, 113, 56), AttackStyle::Fire, HeroSkill::Nezha, 72.0f, 1.04f, 420.0f, 42.0f},
        {u8"赵云", u8"战士", "hero_zhaoyun", C(122, 168, 210), C(238, 247, 255), C(162, 221, 255), AttackStyle::Slash, HeroSkill::Zhaoyun, 65.0f, 0.98f, 410.0f, 22.0f},
        {u8"剑圣", u8"战士", "hero_blademaster", C(196, 96, 78), C(255, 221, 92), C(255, 204, 92), AttackStyle::Slash, HeroSkill::BladeMaster, 78.0f, 0.96f, 390.0f, 28.0f},
        {u8"骑士", u8"战士", "hero_knight", C(199, 111, 73), C(255, 218, 92), C(255, 157, 78), AttackStyle::Slash, HeroSkill::Knight, 60.0f, 1.10f, 382.0f, 0.0f},
        {u8"卡卡", u8"战士/控制", "hero_kaka", C(106, 145, 196), C(142, 235, 255), C(109, 226, 246), AttackStyle::Lightning, HeroSkill::Kaka, 54.0f, 0.88f, 392.0f, 46.0f},
        {u8"帝斯拉", u8"法师", "hero_desila", C(107, 143, 201), C(255, 137, 86), C(255, 106, 72), AttackStyle::Fire, HeroSkill::Desila, 74.0f, 1.26f, 442.0f, 66.0f},
        {u8"暴风", u8"法师", "hero_storm", C(104, 183, 199), C(233, 248, 255), C(153, 234, 255), AttackStyle::Wind, HeroSkill::Storm, 45.0f, 1.02f, 445.0f, 72.0f},
        {u8"闪电之子", u8"法师", "hero_lightning_child", C(118, 192, 230), C(255, 238, 104), C(255, 235, 87), AttackStyle::Lightning, HeroSkill::LightningChild, 48.0f, 0.90f, 455.0f, 0.0f},
        {u8"雪姬", u8"法师/控制", "hero_snow_princess", C(118, 214, 226), C(236, 250, 255), C(124, 232, 255), AttackStyle::Frost, HeroSkill::SnowPrincess, 39.0f, 1.10f, 438.0f, 56.0f},
        {u8"火焰法师", u8"法师", "hero_fire_mage", C(211, 95, 67), C(255, 202, 91), C(255, 116, 58), AttackStyle::Fire, HeroSkill::FireMage, 50.0f, 0.96f, 432.0f, 48.0f},
        {u8"毒液", u8"毒系", "hero_venom", C(91, 70, 126), C(124, 244, 91), C(132, 242, 90), AttackStyle::Poison, HeroSkill::Venom, 43.0f, 1.06f, 438.0f, 36.0f},
        {u8"黑百合", u8"毒系", "hero_black_lily", C(139, 73, 156), C(207, 244, 122), C(154, 240, 112), AttackStyle::Poison, HeroSkill::BlackLily, 45.0f, 1.00f, 430.0f, 58.0f},
        {u8"河掌柜", u8"毒系", "hero_river_master", C(72, 154, 150), C(202, 244, 122), C(119, 227, 112), AttackStyle::Poison, HeroSkill::RiverMaster, 38.0f, 1.04f, 424.0f, 62.0f},
        {u8"死亡骑士", u8"召唤", "hero_death_knight", C(84, 98, 122), C(166, 228, 255), C(139, 212, 255), AttackStyle::Summon, HeroSkill::DeathKnight, 62.0f, 1.20f, 388.0f, 42.0f},
        {u8"海王", u8"召唤", "hero_sea_king", C(67, 151, 190), C(166, 235, 255), C(116, 216, 255), AttackStyle::Summon, HeroSkill::SeaKing, 44.0f, 1.12f, 400.0f, 58.0f},
        {u8"安妮", u8"召唤", "hero_annie", C(201, 95, 76), C(255, 207, 104), C(255, 188, 93), AttackStyle::Summon, HeroSkill::Annie, 36.0f, 1.02f, 386.0f, 52.0f},
        {u8"妲己", u8"控制", "hero_daji", C(215, 111, 172), C(255, 213, 236), C(255, 137, 205), AttackStyle::Charm, HeroSkill::Daji, 34.0f, 1.06f, 432.0f, 36.0f},
        {u8"天使", u8"辅助", "hero_angel", C(232, 197, 93), C(255, 246, 178), C(255, 244, 176), AttackStyle::Holy, HeroSkill::Angel, 25.0f, 0.78f, 410.0f, 0.0f},
        {u8"美人鱼", u8"辅助", "hero_mermaid", C(91, 191, 187), C(217, 251, 244), C(147, 238, 232), AttackStyle::Holy, HeroSkill::Mermaid, 26.0f, 0.88f, 408.0f, 30.0f},
        {u8"雅典娜", u8"辅助", "hero_athena", C(177, 139, 216), C(255, 235, 138), C(255, 232, 140), AttackStyle::Holy, HeroSkill::Athena, 28.0f, 0.90f, 402.0f, 0.0f},
        {u8"冰法师", u8"控制", "hero_ice_mage", C(128, 210, 230), C(235, 250, 255), C(151, 238, 255), AttackStyle::Frost, HeroSkill::IceMage, 31.0f, 1.02f, 424.0f, 44.0f},
        {u8"地鼠", u8"控制", "hero_mole", C(146, 107, 75), C(255, 210, 104), C(230, 178, 94), AttackStyle::Slash, HeroSkill::Mole, 35.0f, 0.96f, 360.0f, 28.0f},
    };
}

inline std::vector<SupportDef> BuildSupportCatalog() {
    return {
        {u8"星芒弓手", u8"远征支援", u8"局内全体伤害提升", C(255, 202, 88), SupportBuff::Damage, 0.08f, 0.025f},
        {u8"时钟学者", u8"节奏支援", u8"开局攻速更快", C(109, 218, 236), SupportBuff::AttackSpeed, 0.09f, 0.022f},
        {u8"霜语女巫", u8"控制支援", u8"控制时间和霜冻收益提升", C(142, 216, 255), SupportBuff::Control, 0.12f, 0.035f},
        {u8"壁垒修女", u8"城防支援", u8"城墙生命和缓慢恢复提升", C(167, 218, 136), SupportBuff::Wall, 0.10f, 0.030f},
        {u8"金铃商人", u8"资源支援", u8"召唤折扣和结算奖励提升", C(255, 185, 91), SupportBuff::Economy, 0.08f, 0.025f},
    };
}

inline std::vector<LevelInfo> BuildLevelCatalog() {
    std::vector<std::string> names = {
        u8"霜木前哨", u8"蓝雾林地", u8"异变山谷", u8"碎冰栈道", u8"回声矿洞",
        u8"冻泉营地", u8"黑松关口", u8"古塔雪径", u8"灰烬哨门", u8"裂隙冰原",
        u8"星尘荒径", u8"雪冠祭坛", u8"失温河湾", u8"月影峡门", u8"寒铁矿场",
        u8"雾钟旧城", u8"冰脊长廊", u8"极光岗楼", u8"龙骨坡道", u8"破晓边墙",
        u8"深蓝裂谷", u8"风暴石阵", u8"王庭外环", u8"沉眠山腹", u8"幽光断桥",
        u8"银雪天井", u8"寒潮王座", u8"星核哨站", u8"蔚蓝终线", u8"永恒星门",
    };
    std::vector<std::string> traits = {
        u8"小型怪增多", u8"减速抗性", u8"首领护甲", u8"飞行怪突袭", u8"金币较少",
        u8"城墙压力", u8"冲锋加速", u8"远程干扰", u8"火把增援", u8"Boss 双阶段",
    };
    std::vector<Color> accents = {
        C(114, 199, 221), C(131, 177, 214), C(143, 205, 222), C(162, 220, 240), C(155, 170, 203),
        C(118, 205, 176), C(201, 148, 111), C(170, 154, 220), C(224, 143, 96), C(135, 194, 236),
    };
    std::vector<LevelInfo> result;
    for (int i = 0; i < 30; ++i) {
        float t = static_cast<float>(i) / 29.0f;
        int chapter = i / 5;
        result.push_back({
            names[i],
            traits[i % traits.size()],
            accents[i % accents.size()],
            1.25f + t * 3.15f + chapter * 0.08f,
            1.0f + t * 0.42f,
            1.08f + t * 0.92f + chapter * 0.055f,
            96 + i * 12,
            2 + i / 4,
            std::min(0.68f, 0.16f + i * 0.012f),
            std::min(0.55f, 0.22f + i * 0.008f),
            chapter,
        });
    }
    return result;
}
