#pragma once

#include "raylib.h"

#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <vector>

constexpr int kScreenW = 430;
constexpr int kScreenH = 900;
constexpr float kWallLineY = 626.0f;
constexpr float kSlotY = 652.0f;
constexpr int kMaxSlots = 7;
constexpr int kHeroCount = 30;
constexpr int kSupportCount = 5;
constexpr int kChapterCount = 5;
constexpr int kStagesPerChapter = 10;
constexpr int kLevelCount = kChapterCount * kStagesPerChapter;
constexpr int kMetaHeroCount = 6;
constexpr int kLineupSlots = 3;
constexpr int kEquipmentSlotCount = 4;
constexpr int kTalentBranchCount = 4;
constexpr int kTalentNodesPerBranch = 5;
constexpr int kTalentCount = kTalentBranchCount * kTalentNodesPerBranch;
constexpr int kChestCountPerChapter = 3;

inline Color C(int r, int g, int b, int a = 255) {
    return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                 static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
}

inline float Clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

inline float RandFloat(std::mt19937& rng, float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

inline int RandInt(std::mt19937& rng, int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

inline bool PointIn(Rectangle r, Vector2 p) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

struct TextKit {
    Font font{};
    bool loaded = false;

    void draw(const char* text, Vector2 pos, float size, Color color) const {
        DrawTextEx(font, text, pos, size, 0.0f, color);
    }

    Vector2 measure(const char* text, float size) const {
        return MeasureTextEx(font, text, size, 0.0f);
    }

    void centered(const char* text, Rectangle box, float size, Color color) const {
        Vector2 m = measure(text, size);
        draw(text, {box.x + (box.width - m.x) * 0.5f, box.y + (box.height - m.y) * 0.5f}, size, color);
    }

    void stroke(const char* text, Vector2 pos, float size, Color color, Color outline) const {
        for (int ox = -2; ox <= 2; ++ox) {
            for (int oy = -2; oy <= 2; ++oy) {
                if (ox == 0 && oy == 0) continue;
                DrawTextEx(font, text, {pos.x + static_cast<float>(ox), pos.y + static_cast<float>(oy)}, size, 0.0f, outline);
            }
        }
        draw(text, pos, size, color);
    }

    void centeredStroke(const char* text, Rectangle box, float size, Color color, Color outline) const {
        Vector2 m = measure(text, size);
        stroke(text, {box.x + (box.width - m.x) * 0.5f, box.y + (box.height - m.y) * 0.5f}, size, color, outline);
    }
};

struct LevelInfo {
    std::string name;
    std::string trait;
    Color accent;
    float hpScale = 1.0f;
    float speedScale = 1.0f;
    float armorScale = 1.0f;
    int baseDustReward = 100;
    int materialReward = 2;
    float gearDropRate = 0.18f;
    float opportunityRate = 0.25f;
    int densityBonus = 0;
};

enum class AttackStyle { Arrow, Frost, Slash, Poison, Bomb, Holy, Laser, Lightning, Fire, Wind, Summon, Charm };
enum class ImpactKind {
    Pulse,
    ArrowHit,
    FrostBurst,
    SlashSpark,
    PoisonCloud,
    BombBlast,
    HolyRing,
    LaserStrike,
    LightningArc,
    FireBurst,
    WindBurst,
    SummonShock,
    CharmWave
};

enum class HeroSkill {
    Assault,
    SwordImmortal,
    SteelDog,
    Rocket,
    Mushroom,
    Ranger,
    Cannon,
    Wukong,
    Nezha,
    Zhaoyun,
    BladeMaster,
    Knight,
    Kaka,
    Desila,
    Storm,
    LightningChild,
    SnowPrincess,
    FireMage,
    Venom,
    BlackLily,
    RiverMaster,
    DeathKnight,
    SeaKing,
    Annie,
    Daji,
    Angel,
    Mermaid,
    Athena,
    IceMage,
    Mole
};

struct HeroDef {
    std::string name;
    std::string job;
    std::string spriteKey;
    Color main;
    Color trim;
    Color projectile;
    AttackStyle attackStyle = AttackStyle::Arrow;
    HeroSkill skill = HeroSkill::Ranger;
    float baseDamage;
    float cooldown;
    float range;
    float splash;
};

enum class SupportBuff { Damage, AttackSpeed, Control, Wall, Economy };

enum class StageType { Normal, Elite, Boss };
enum class EnemyType { Normal, Swarm, Armored, Swift, Boss, Mixed };
enum class RoleKind { FireMage, FrostArcher, ShieldGuard, ThunderWarlock, Berserker, NaturePriest };
enum class EquipmentSlot { Weapon, Armor, Accessory, Treasure };
enum class TalentBranch { Attack, Defense, Resource, Class };
enum class FailureReason { None, Output, Defense, Clear, Boss, Control };

struct ResourceBundle {
    int gold = 0;
    int heroExp = 0;
    int gearMat = 0;
    int heroShard = 0;
    int talentPoint = 0;
    int diamond = 0;
};

inline ResourceBundle operator+(ResourceBundle a, const ResourceBundle& b) {
    a.gold += b.gold;
    a.heroExp += b.heroExp;
    a.gearMat += b.gearMat;
    a.heroShard += b.heroShard;
    a.talentPoint += b.talentPoint;
    a.diamond += b.diamond;
    return a;
}

inline ResourceBundle& operator+=(ResourceBundle& a, const ResourceBundle& b) {
    a.gold += b.gold;
    a.heroExp += b.heroExp;
    a.gearMat += b.gearMat;
    a.heroShard += b.heroShard;
    a.talentPoint += b.talentPoint;
    a.diamond += b.diamond;
    return a;
}

inline ResourceBundle ScaleReward(ResourceBundle reward, float scale) {
    reward.gold = static_cast<int>(reward.gold * scale);
    reward.heroExp = static_cast<int>(reward.heroExp * scale);
    reward.gearMat = static_cast<int>(reward.gearMat * scale);
    reward.heroShard = static_cast<int>(reward.heroShard * scale);
    reward.talentPoint = static_cast<int>(reward.talentPoint * scale);
    reward.diamond = static_cast<int>(reward.diamond * scale);
    return reward;
}

struct HeroMetaDef {
    std::string name;
    std::string role;
    std::string job;
    std::string element;
    std::string rarity;
    std::string skillName;
    std::string skillDesc;
    std::string passiveDesc;
    std::string star3Desc;
    std::string star5Desc;
    std::string recommendation;
    Color main;
    Color accent;
    int baseAttack = 0;
    int attackGrowth = 0;
    int baseLife = 0;
    int lifeGrowth = 0;
    RoleKind kind = RoleKind::FireMage;
};

struct StageInfo {
    int index = 0;
    int chapter = 1;
    int local = 1;
    std::string chapterName;
    std::string name;
    StageType type = StageType::Normal;
    EnemyType enemy = EnemyType::Normal;
    std::string enemyName;
    std::string tip;
    int recommendedPower = 100;
    int waves = 10;
    ResourceBundle baseReward;
    ResourceBundle firstReward;
    float failRatio = 0.25f;
    Color accent;
};

struct EquipmentDef {
    std::string name;
    std::string desc;
    std::array<std::string, 4> milestones{};
    Color accent;
};

struct TalentDef {
    TalentBranch branch = TalentBranch::Attack;
    int node = 0;
    std::string id;
    std::string name;
    std::string desc;
    int maxLevel = 1;
    int prerequisite = -1;
    int prerequisiteLevel = 0;
    bool keyNode = false;
};

struct StageRunResult {
    bool victory = false;
    int stageIndex = 0;
    int failedWave = 0;
    int stars = 0;
    std::array<bool, 3> starDone{false, false, false};
    int wallHp = 0;
    int wallMax = 0;
    FailureReason failure = FailureReason::None;
    ResourceBundle reward;
    ResourceBundle firstReward;
    ResourceBundle totalReward;
    bool firstClear = false;
    bool unlockedNext = false;
    bool unlockedChapter = false;
    std::string mainContributor;
    std::string advice;
};

struct SupportDef {
    std::string name;
    std::string job;
    std::string desc;
    Color accent;
    SupportBuff buff = SupportBuff::Damage;
    float baseValue = 0.0f;
    float perCopy = 0.0f;
};

struct WavePlan {
    int count = 0;
    float interval = 0.7f;
};

struct Hero {
    bool active = false;
    int type = 0;
    int star = 1;
    float cooldown = 0.0f;
    float flash = 0.0f;
};

enum class EnemyKind { Scout, Brute, Flyer, Shaman, Boss };

struct Enemy {
    int id = 0;
    EnemyKind kind = EnemyKind::Scout;
    Vector2 pos{};
    float wobble = 0.0f;
    float hp = 0.0f;
    float maxHp = 0.0f;
    float speed = 0.0f;
    float radius = 18.0f;
    float attackTimer = 0.0f;
    float slowTimer = 0.0f;
    float stunTimer = 0.0f;
    float charmTimer = 0.0f;
    float burnTimer = 0.0f;
    float burnDps = 0.0f;
    float poisonTimer = 0.0f;
    float poisonDps = 0.0f;
    float vulnerableTimer = 0.0f;
    float flash = 0.0f;
};

struct Projectile {
    Vector2 pos{};
    Vector2 prevPos{};
    int targetId = 0;
    int heroType = 0;
    AttackStyle style = AttackStyle::Arrow;
    HeroSkill skill = HeroSkill::Ranger;
    int star = 1;
    float damage = 0.0f;
    float speed = 360.0f;
    float splash = 0.0f;
    Color color{};
};

struct FloatingText {
    std::string text;
    Vector2 pos{};
    Color color{};
    float life = 0.0f;
};

struct Impact {
    Vector2 pos{};
    Color color{};
    float radius = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    ImpactKind kind = ImpactKind::Pulse;
};

struct DraftChoice {
    std::string title;
    std::string body;
    int kind = 0;
};

struct SaveData {
    ResourceBundle resources{420, 300, 24, 28, 0, 0};
    int unlockedStage = 3;
    int highestCleared = 0;
    int selectedStage = 2;
    std::array<int, kLevelCount> stageStars{};
    std::array<int, kChapterCount * kChestCountPerChapter> chestClaimed{};
    std::array<int, kMetaHeroCount> heroLevel{1, 1, 1, 1, 1, 1};
    std::array<int, kMetaHeroCount> heroStar{1, 1, 1, 1, 1, 1};
    std::array<int, kLineupSlots> lineup{0, 1, 2};
    std::array<int, kEquipmentSlotCount> equipLevel{};
    std::array<int, kTalentCount> talentLevel{};

    int unlockedLevel = 3;
    int starDust = 180;
    int equipmentParts = 12;
    int totemRunes = 10;
    int cloakSilk = 10;
    int gachaTickets = 3;
    int rareGear = 0;
    int epicGear = 0;
    int mythicGear = 0;
    int equipmentLevel = 0;
    int totemLevel = 0;
    int cloakLevel = 0;
    int bestLevel = 0;
    int victories = 0;
    int activeSupport = 0;
    int gachaPity = 0;
    std::array<int, kSupportCount> supportCopies{1, 0, 0, 0, 0};
    bool muted = false;
};

struct GameAssets {
    Texture2D scene{};
    std::vector<std::array<Texture2D, 2>> heroes{};
    std::array<std::array<Texture2D, 2>, 5> enemies{};
    Sound summon{};
    Sound merge{};
    Sound shoot{};
    Sound hit{};
    Sound freeze{};
    Sound cannon{};
    Sound draft{};
    Sound victory{};
    Sound defeat{};
    bool spritesLoaded = false;
    bool heroSpritesComplete = false;
    bool audioLoaded = false;
};

enum class GameState {
    Lobby,
    StageSelect,
    Meta,
    Heroes,
    Equipment,
    Talents,
    Lineup,
    Chests,
    Playing,
    Draft,
    Paused,
    Victory,
    Defeat
};
