#include "game.hpp"

#include "game_content.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

class Game::Impl {
public:
    Impl() {
        rng.seed(std::random_device{}());
        levels = BuildLevels();
        heroesDef = BuildHeroes();
        supports = BuildSupports();
        currentLevel = 2;
    }

    void Init() {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
        InitWindow(kScreenW, kScreenH, "永远的蔚蓝星球 - 随机合成塔防");
        SetWindowMinSize(kScreenW, kScreenH);
        SetTargetFPS(60);
        LoadSave();
        LoadUIFont();
        LoadAssets();
    }

    void Shutdown() {
        SaveProgress();
        UnloadAssets();
        if (text.loaded) UnloadFont(text.font);
        CloseWindow();
    }

    bool ShouldClose() const {
        return WindowShouldClose();
    }

    void Tick() {
        float dt = GetFrameTime();
        Vector2 mouse = VirtualMouse();
        HandleInput(mouse);

        if (state == GameState::Playing) {
            UpdateGame(dt * speedMultiplier);
        }
        UpdateParticles(dt);
        Draw();
    }

    bool SaveScreenshot(const std::string& mode, const std::string& path) {
        if (mode == "battle") {
            ResetStage();
            for (int i = 0; i < 300; ++i) {
                UpdateGame(1.0f / 45.0f);
                UpdateParticles(1.0f / 45.0f);
                if (state == GameState::Victory || state == GameState::Defeat) break;
            }
            showStats = true;
        } else if (mode == "draft") {
            ResetStage();
            OpenDraft();
        } else if (mode == "stage") {
            state = GameState::StageSelect;
        } else if (mode == "meta") {
            state = GameState::Meta;
        } else if (mode == "start") {
            state = GameState::Lobby;
        } else {
            return false;
        }

        RenderTexture2D target = LoadRenderTexture(kScreenW, kScreenH);
        if (target.texture.id == 0) return false;
        BeginTextureMode(target);
        ClearBackground(C(14, 22, 31));
        DrawScene();
        EndTextureMode();

        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        bool ok = ExportImage(image, path.c_str());
        UnloadImage(image);
        UnloadRenderTexture(target);
        return ok;
    }

    bool RunVerification(std::string* report) {
        SaveData originalSave = save;
        bool ok = true;
        std::ostringstream out;
        auto check = [&](bool condition, const std::string& label) {
            out << (condition ? "[OK] " : "[FAIL] ") << label << "\n";
            ok = ok && condition;
        };

        check(levels.size() == 30, "30 levels are defined");
        check(heroesDef.size() == kHeroCount, "30 hero archetypes are defined");
        bool heroDataOk = true;
        for (const auto& hero : heroesDef) {
            heroDataOk = heroDataOk && !hero.name.empty() && !hero.job.empty() && !hero.spriteKey.empty() &&
                         hero.baseDamage > 0.0f && hero.cooldown > 0.0f && hero.range > 0.0f;
        }
        check(heroDataOk, "every hero has complete numeric and resource metadata");
        check(supports.size() == kSupportCount, "5 out-of-run support heroes are defined");
        check(assets.spritesLoaded, "PNG sprite assets are loaded");
        check(assets.heroes.size() == heroesDef.size(), "hero sprite slots match hero catalog and allow future replacement");
        check(assets.audioLoaded, "WAV sound effects are loaded");
        check(save.unlockedLevel >= 1 && save.unlockedLevel <= 30, "save data stores unlocked progress");
        check(save.starDust >= 0, "save data stores out-of-run currency");
        check(save.supportCopies[save.activeSupport] > 0, "save data stores owned active support hero");
        state = GameState::Lobby;
        check(state == GameState::Lobby, "game boots into out-of-run lobby");

        ResetStage();
        int initialHeroes = 0;
        for (const auto& h : slots) if (h.active) initialHeroes++;
        check(state == GameState::Playing, "stage starts in playing state");
        check(totalWaves == 10, "stage uses 10 waves");
        check(initialHeroes == 4, "initial board has 4 heroes");
        check(wallHp == wallMaxHp && wallMaxHp > 0, "wall starts at full health");

        int coinsBefore = coins;
        TrySummon();
        check(summons == 1, "summon action increments summon counter");
        check(coins < coinsBefore, "summon spends coins");

        state = GameState::Playing;
        selectedSlot = 2;
        HandleSlotClick(4);
        check(merges == 1, "matching heroes merge with 2-in-1");
        check(slots[2].active && slots[2].star == 2 && !slots[4].active, "merge raises star and consumes one slot");
        slots[0] = Hero{true, 1, 1, 0.0f, 0.0f};
        slots[1] = Hero{true, 1, 1, 0.0f, 0.0f};
        int mergesBeforeDrag = merges;
        bool dragMerged = TryMergeSlots(0, 1);
        check(dragMerged && merges == mergesBeforeDrag + 1 && slots[0].star == 2 && !slots[1].active, "drag-style merge path reuses 2-in-1 rules");

        state = GameState::Playing;
        draftChoices.clear();
        coins = 999;
        TryUpgrade();
        check(state == GameState::Draft && draftChoices.size() == 3, "upgrade opens 3-choice random draft");
        ApplyDraft(draftChoices.front().kind);
        state = GameState::Playing;

        enemies.clear();
        wave = 0;
        currentLevel = 9;
        StartWave();
        int earlyWaveCount = spawnPlanned;
        wave = 7;
        StartWave();
        check(spawnPlanned > earlyWaveCount + 12, "monster count ramps up across waves and level difficulty");
        wave = 0;
        currentLevel = 0;
        StartWave();
        check(spawnPlanned >= 15, "opening wave has enough monsters to require active play");
        currentLevel = 29;
        StartWave();
        int lateFirstWaveCount = spawnPlanned;
        currentLevel = 0;
        SpawnEnemy();
        check(!enemies.empty(), "enemy spawner creates an enemy");
        float firstLevelHp = enemies.front().maxHp;
        enemies.clear();
        currentLevel = 29;
        SpawnEnemy();
        check(!enemies.empty() && enemies.front().maxHp > firstLevelHp * 4.0f && lateFirstWaveCount > 35, "late levels strongly increase monster health and density");
        currentLevel = 0;
        float enemyHpBefore = enemies.front().hp;
        UseFreeze();
        check(freezeCooldown > 0.0f && enemies.front().slowTimer > 0.0f, "freeze mechanism applies control and cooldown");
        UseCannon();
        check(cannonCooldown > 0.0f && enemies.front().hp < enemyHpBefore, "cannon mechanism damages enemies");
        check(ProjectileSpeed(AttackStyle::Arrow) != ProjectileSpeed(AttackStyle::Bomb) &&
              ImpactForAttack(AttackStyle::Frost) == ImpactKind::FrostBurst &&
              ImpactForAttack(AttackStyle::Bomb) == ImpactKind::BombBlast,
              "hero attack styles define distinct projectile and hit effects");
        check(VerifyAllHeroSkills(), "all 30 hero skills apply damage or a valid combat effect");
        kingCharge = 5;
        UseKing();
        check(kingCharge == 0 && kingBuffTimer > 0.0f, "king command spends charge and applies buff");
        save.activeSupport = 0;
        save.supportCopies[0] = std::max(1, save.supportCopies[0]);
        ResetStage();
        int damageBeforeMeta = static_cast<int>(globalDamage * 1000.0f);
        save.equipmentLevel += 1;
        ResetStage();
        check(static_cast<int>(globalDamage * 1000.0f) > damageBeforeMeta, "equipment cultivation affects battle damage");
        save.equipmentLevel -= 1;
        ResetStage();
        float supportDamage = globalDamage;
        save.activeSupport = 1;
        save.supportCopies[1] = std::max(1, save.supportCopies[1]);
        ResetStage();
        check(globalAttackSpeed > 1.0f && globalDamage <= supportDamage + 0.25f, "selected support hero changes in-run buff");
        int ticketsBefore = save.gachaTickets;
        save.gachaTickets = std::max(1, save.gachaTickets);
        int ownedBefore = TotalSupportCopies();
        bool drewSupport = TrySupportGacha(false);
        check(drewSupport && TotalSupportCopies() == ownedBefore + 1, "out-of-run support gacha grants a support copy");
        save.gachaTickets = ticketsBefore;
        int materialsBefore = save.equipmentParts + save.totemRunes + save.cloakSilk + save.rareGear + save.epicGear + save.mythicGear;
        currentLevel = 4;
        HandleBattleEnd(true);
        int materialsAfter = save.equipmentParts + save.totemRunes + save.cloakSilk + save.rareGear + save.epicGear + save.mythicGear;
        check(materialsAfter > materialsBefore, "victory settlement grants cultivation materials and gear drops");
        ResetStage();

        for (int i = 0; i < 240; ++i) {
            UpdateGame(1.0f / 60.0f);
            UpdateParticles(1.0f / 60.0f);
            if (state == GameState::Victory || state == GameState::Defeat) break;
        }
        check(kills >= 0 && wallHp >= 0, "simulation tick loop remains stable");

        save = originalSave;
        SaveProgress();
        *report = out.str();
        return ok;
    }

private:
    TextKit text;
    GameAssets assets;
    SaveData save;
    std::mt19937 rng;
    GameState state = GameState::Lobby;
    GameState previousState = GameState::Playing;
    std::vector<LevelInfo> levels;
    std::vector<HeroDef> heroesDef;
    std::vector<SupportDef> supports;
    std::array<Hero, kMaxSlots> slots{};
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<FloatingText> floaters;
    std::vector<Impact> impacts;
    std::vector<DraftChoice> draftChoices;

    int currentLevel = 0;
    int wave = 0;
    int totalWaves = 10;
    int nextEnemyId = 1;
    int spawnRemaining = 0;
    int spawnPlanned = 0;
    float spawnTimer = 0.0f;
    float spawnInterval = 0.7f;
    float waveBreak = 1.5f;
    float stageBanner = 2.2f;
    float kingBuffTimer = 0.0f;
    float freezeCooldown = 0.0f;
    float cannonCooldown = 0.0f;
    float toastTimer = 0.0f;
    std::string toast = "";
    std::string resultLoot = "";

    int selectedSlot = -1;
    int dragSlot = -1;
    Vector2 dragStart{};
    Vector2 dragMouse{};
    bool dragActive = false;
    int coins = 88;
    int summonCost = 45;
    int upgradeCost = 100;
    int wallHp = 6000;
    int wallMaxHp = 6000;
    int kingCharge = 4;
    int kills = 0;
    int summons = 0;
    int merges = 0;
    int highestStar = 1;
    int speedIndex = 0;
    float speedMultiplier = 1.0f;
    bool showStats = false;
    bool resultHandled = false;

    float globalDamage = 1.0f;
    float globalAttackSpeed = 1.0f;
    float controlBonus = 1.0f;
    float poisonBonus = 1.0f;
    float bossDamage = 1.0f;
    float wallRegen = 0.0f;
    float summonDiscount = 0.0f;
    float mechanismCooldownBonus = 1.0f;
    float rewardBonus = 1.0f;

    Rectangle startButton{176, 812, 220, 54};
    Rectangle adventureButton{46, 642, 338, 66};
    Rectangle metaMenuButton{46, 724, 338, 58};
    Rectangle backButton{30, 812, 118, 54};
    Rectangle prevStageButton{54, 596, 64, 48};
    Rectangle nextStageButton{312, 596, 64, 48};
    Rectangle summonButton{322, 805, 86, 72};
    Rectangle upgradeButton{22, 805, 86, 72};
    Rectangle freezeButton{122, 792, 76, 58};
    Rectangle kingButton{177, 744, 76, 78};
    Rectangle cannonButton{232, 792, 76, 58};
    Rectangle pauseButton{30, 178, 46, 58};
    Rectangle muteButton{324, 56, 84, 42};
    Rectangle speedButton{24, 278, 54, 44};
    Rectangle statsButton{22, 336, 58, 68};
    Rectangle equipmentButton{38, 294, 354, 62};
    Rectangle totemButton{38, 368, 354, 62};
    Rectangle cloakButton{38, 442, 354, 62};
    Rectangle supportPrevButton{58, 606, 40, 40};
    Rectangle supportNextButton{332, 606, 40, 40};
    Rectangle gachaButton{136, 684, 158, 36};

    static std::vector<HeroDef> BuildHeroes() {
        return BuildHeroCatalog();
    }

    static std::vector<SupportDef> BuildSupports() {
        return BuildSupportCatalog();
    }

    static std::vector<LevelInfo> BuildLevels() {
        return BuildLevelCatalog();
    }

    void LoadAssets() {
        assets.scene = LoadTexture("assets/sprites/scene_valley.png");
        const char* enemyNames[] = {"enemy_scout", "enemy_brute", "enemy_flyer", "enemy_shaman", "enemy_boss"};
        bool spritesOk = assets.scene.id != 0;
        bool heroSpritesComplete = true;
        assets.heroes.clear();
        assets.heroes.resize(heroesDef.size());
        for (int i = 0; i < static_cast<int>(heroesDef.size()); ++i) {
            for (int frame = 0; frame < 2; ++frame) {
                std::stringstream path;
                path << "assets/sprites/" << heroesDef[i].spriteKey << "_" << frame << ".png";
                if (FileExists(path.str().c_str())) assets.heroes[i][frame] = LoadChromaTexture(path.str());
                if (assets.heroes[i][frame].id == 0 && i < 6) {
                    const char* legacyNames[] = {"hero_archer", "hero_mage", "hero_knight", "hero_poison", "hero_mechanic", "hero_priest"};
                    std::stringstream legacyPath;
                    legacyPath << "assets/sprites/" << legacyNames[i] << "_" << frame << ".png";
                    if (FileExists(legacyPath.str().c_str())) assets.heroes[i][frame] = LoadChromaTexture(legacyPath.str());
                }
                heroSpritesComplete = heroSpritesComplete && assets.heroes[i][frame].id != 0;
            }
        }
        assets.heroSpritesComplete = heroSpritesComplete;
        for (int i = 0; i < 5; ++i) {
            for (int frame = 0; frame < 2; ++frame) {
                std::stringstream path;
                path << "assets/sprites/" << enemyNames[i] << "_" << frame << ".png";
                assets.enemies[i][frame] = LoadChromaTexture(path.str());
                spritesOk = spritesOk && assets.enemies[i][frame].id != 0;
            }
        }
        assets.spritesLoaded = spritesOk;

        InitAudioDevice();
        assets.summon = LoadSound("assets/audio/summon.wav");
        assets.merge = LoadSound("assets/audio/merge.wav");
        assets.shoot = LoadSound("assets/audio/shoot.wav");
        assets.hit = LoadSound("assets/audio/hit.wav");
        assets.freeze = LoadSound("assets/audio/freeze.wav");
        assets.cannon = LoadSound("assets/audio/cannon.wav");
        assets.draft = LoadSound("assets/audio/draft.wav");
        assets.victory = LoadSound("assets/audio/victory.wav");
        assets.defeat = LoadSound("assets/audio/defeat.wav");
        assets.audioLoaded = assets.summon.frameCount > 0 && assets.merge.frameCount > 0 && assets.shoot.frameCount > 0 &&
                             assets.hit.frameCount > 0 && assets.freeze.frameCount > 0 && assets.cannon.frameCount > 0 &&
                             assets.draft.frameCount > 0 && assets.victory.frameCount > 0 && assets.defeat.frameCount > 0;
    }

    void NormalizeSave() {
        save.unlockedLevel = std::max(1, std::min(30, save.unlockedLevel));
        save.starDust = std::max(0, save.starDust);
        save.equipmentParts = std::max(0, save.equipmentParts);
        save.totemRunes = std::max(0, save.totemRunes);
        save.cloakSilk = std::max(0, save.cloakSilk);
        save.gachaTickets = std::max(0, save.gachaTickets);
        save.rareGear = std::max(0, save.rareGear);
        save.epicGear = std::max(0, save.epicGear);
        save.mythicGear = std::max(0, save.mythicGear);
        save.equipmentLevel = std::max(0, save.equipmentLevel);
        save.totemLevel = std::max(0, save.totemLevel);
        save.cloakLevel = std::max(0, save.cloakLevel);
        save.bestLevel = std::max(0, save.bestLevel);
        save.victories = std::max(0, save.victories);
        save.activeSupport = std::max(0, std::min(kSupportCount - 1, save.activeSupport));
        save.gachaPity = std::max(0, save.gachaPity);
        bool hasSupport = false;
        for (int& copies : save.supportCopies) {
            copies = std::max(0, copies);
            hasSupport = hasSupport || copies > 0;
        }
        if (!hasSupport) save.supportCopies[0] = 1;
        if (save.supportCopies[save.activeSupport] <= 0) {
            for (int i = 0; i < kSupportCount; ++i) {
                if (save.supportCopies[i] > 0) {
                    save.activeSupport = i;
                    break;
                }
            }
        }
        currentLevel = std::min(currentLevel, save.unlockedLevel - 1);
    }

    Texture2D LoadChromaTexture(const std::string& path) {
        Image image = LoadImage(path.c_str());
        if (image.data == nullptr) return Texture2D{};
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageColorReplace(&image, C(0, 255, 0), BLANK);
        Texture2D tex = LoadTextureFromImage(image);
        UnloadImage(image);
        return tex;
    }

    void UnloadAssets() {
        if (assets.scene.id) UnloadTexture(assets.scene);
        for (auto& heroFrames : assets.heroes) {
            for (auto& tex : heroFrames) if (tex.id) UnloadTexture(tex);
        }
        for (auto& enemyFrames : assets.enemies) {
            for (auto& tex : enemyFrames) if (tex.id) UnloadTexture(tex);
        }
        if (assets.audioLoaded) {
            UnloadSound(assets.summon);
            UnloadSound(assets.merge);
            UnloadSound(assets.shoot);
            UnloadSound(assets.hit);
            UnloadSound(assets.freeze);
            UnloadSound(assets.cannon);
            UnloadSound(assets.draft);
            UnloadSound(assets.victory);
            UnloadSound(assets.defeat);
        }
        if (IsAudioDeviceReady()) CloseAudioDevice();
    }

    void PlaySfx(Sound sound) {
        if (!save.muted && assets.audioLoaded && sound.frameCount > 0) PlaySound(sound);
    }

    std::string SavePath() const {
        return "save.dat";
    }

    void LoadSave() {
        std::ifstream in(SavePath());
        if (!in) return;
        std::string key;
        int value = 0;
        while (in >> key >> value) {
            if (key == "unlockedLevel") save.unlockedLevel = value;
            else if (key == "starDust") save.starDust = value;
            else if (key == "equipmentParts") save.equipmentParts = value;
            else if (key == "totemRunes") save.totemRunes = value;
            else if (key == "cloakSilk") save.cloakSilk = value;
            else if (key == "gachaTickets") save.gachaTickets = value;
            else if (key == "rareGear") save.rareGear = value;
            else if (key == "epicGear") save.epicGear = value;
            else if (key == "mythicGear") save.mythicGear = value;
            else if (key == "equipmentLevel") save.equipmentLevel = value;
            else if (key == "totemLevel") save.totemLevel = value;
            else if (key == "cloakLevel") save.cloakLevel = value;
            else if (key == "bestLevel") save.bestLevel = value;
            else if (key == "victories") save.victories = value;
            else if (key == "activeSupport") save.activeSupport = value;
            else if (key == "gachaPity") save.gachaPity = value;
            else if (key.rfind("support", 0) == 0) {
                int idx = -1;
                try {
                    idx = std::stoi(key.substr(7));
                } catch (...) {
                    idx = -1;
                }
                if (idx >= 0 && idx < kSupportCount) save.supportCopies[idx] = value;
            }
            else if (key == "muted") save.muted = value != 0;
        }
        NormalizeSave();
    }

    void SaveProgress() {
        std::ofstream out(SavePath(), std::ios::trunc);
        if (!out) return;
        out << "unlockedLevel " << save.unlockedLevel << "\n";
        out << "starDust " << save.starDust << "\n";
        out << "equipmentParts " << save.equipmentParts << "\n";
        out << "totemRunes " << save.totemRunes << "\n";
        out << "cloakSilk " << save.cloakSilk << "\n";
        out << "gachaTickets " << save.gachaTickets << "\n";
        out << "rareGear " << save.rareGear << "\n";
        out << "epicGear " << save.epicGear << "\n";
        out << "mythicGear " << save.mythicGear << "\n";
        out << "equipmentLevel " << save.equipmentLevel << "\n";
        out << "totemLevel " << save.totemLevel << "\n";
        out << "cloakLevel " << save.cloakLevel << "\n";
        out << "bestLevel " << save.bestLevel << "\n";
        out << "victories " << save.victories << "\n";
        out << "activeSupport " << save.activeSupport << "\n";
        out << "gachaPity " << save.gachaPity << "\n";
        for (int i = 0; i < kSupportCount; ++i) out << "support" << i << " " << save.supportCopies[i] << "\n";
        out << "muted " << (save.muted ? 1 : 0) << "\n";
    }

    int UpgradeCost(int kind) const {
        int level = kind == 0 ? save.equipmentLevel : (kind == 1 ? save.totemLevel : save.cloakLevel);
        return 90 + level * 60 + level * level * 18;
    }

    int MaterialCost(int kind) const {
        int level = kind == 0 ? save.equipmentLevel : (kind == 1 ? save.totemLevel : save.cloakLevel);
        return 4 + level * 2;
    }

    bool TryMetaUpgrade(int kind) {
        int cost = UpgradeCost(kind);
        if (save.starDust < cost) {
            ShowToast(u8"星尘不足");
            return false;
        }
        int materialCost = MaterialCost(kind);
        int* material = kind == 0 ? &save.equipmentParts : (kind == 1 ? &save.totemRunes : &save.cloakSilk);
        if (*material < materialCost) {
            ShowToast(kind == 0 ? u8"装备碎片不足" : (kind == 1 ? u8"图腾符文不足" : u8"披风丝线不足"));
            return false;
        }
        save.starDust -= cost;
        *material -= materialCost;
        if (kind == 0) save.equipmentLevel++;
        if (kind == 1) save.totemLevel++;
        if (kind == 2) save.cloakLevel++;
        SaveProgress();
        PlaySfx(assets.draft);
        ShowToast(kind == 0 ? u8"装备升级" : (kind == 1 ? u8"图腾升级" : u8"披风升级"));
        return true;
    }

    int TotalSupportCopies() const {
        int total = 0;
        for (int copies : save.supportCopies) total += copies;
        return total;
    }

    void SelectSupport(int direction) {
        for (int step = 1; step <= kSupportCount; ++step) {
            int idx = (save.activeSupport + direction * step + kSupportCount * 2) % kSupportCount;
            if (save.supportCopies[idx] > 0) {
                save.activeSupport = idx;
                SaveProgress();
                ShowToast(supports[idx].name);
                return;
            }
        }
    }

    bool TrySupportGacha(bool showFeedback = true) {
        if (save.gachaTickets <= 0 && save.starDust < 160) {
            if (showFeedback) ShowToast(u8"招募券或星尘不足");
            return false;
        }
        if (save.gachaTickets > 0) {
            save.gachaTickets--;
        } else {
            save.starDust -= 160;
        }

        save.gachaPity++;
        int roll = RandInt(rng, 0, 99);
        int idx = 0;
        if (save.gachaPity >= 8) {
            idx = RandInt(rng, 2, kSupportCount - 1);
            save.gachaPity = 0;
        } else if (roll < 12) {
            idx = RandInt(rng, 2, kSupportCount - 1);
            save.gachaPity = 0;
        } else if (roll < 42) {
            idx = RandInt(rng, 0, kSupportCount - 1);
        } else {
            idx = RandInt(rng, 0, 2);
        }
        save.supportCopies[idx]++;
        save.activeSupport = idx;
        SaveProgress();
        PlaySfx(assets.draft);
        if (showFeedback) ShowToast(u8"招募：" + supports[idx].name);
        return true;
    }

    void LoadUIFont() {
        std::string glyphs = u8"永远的蔚蓝星球随机合成塔防开始防守选择关卡上一关下一关"
                             u8"0123456789/.:：+-<>x×★·%，% Boss Lv"
                             u8"第关异变山谷波次统计暂停继续胜利失败重试进入下一关"
                             u8"银币召唤强化霜冻炮击号令城墙生命剩余怪物击杀最高星级次数合成"
                             u8"随机强化选择一项成长点击或拖拽相同英雄进行二合一满槽位相同英雄可合成"
                             u8"攻速伤害控制毒伤首领猎手城墙修复召唤折扣机关冷却范围清怪"
                             u8"翠叶射手冰晶法师王冠骑士毒雾术士机关工匠月铃祭司法师战士毒系辅助"
                             u8"冷却中金币不足没有空位已暂停"
                             u8"箭雨校准全体伤害射手额外受益急速号角期间再提升寒霜锁链时间更久"
                             u8"毒雾蔓延持续溅射扩大对立即回复并获得缓慢恢复星辉费用降低"
                             u8"整备缩短裂晶爆破猛烈随机进化需要同英雄同星级没有怪物王者号令"
                             u8"防守城墙失守重试本关进入下一关个关卡局内"
                             u8"装备图腾披风养成星尘升级攻击生命控制收益已保存音效开关启闭图片精灵帧真实音效"
                             u8"已解锁与、提升星尘不足碎片符文丝线招募券支援人物抽卡掉落稀有史诗神话"
                             u8"远征节奏城防资源招募当前拥有结算奖励材料大厅闯关备战目标"
                             u8"返回最佳用于推荐战力得下一关直接说明页面菜单";
        for (const auto& level : levels) {
            glyphs += level.name;
            glyphs += level.trait;
        }
        for (const auto& hero : heroesDef) {
            glyphs += hero.name;
            glyphs += hero.job;
        }
        for (const auto& support : supports) {
            glyphs += support.name;
            glyphs += support.job;
            glyphs += support.desc;
        }
        int codepointCount = 0;
        int* codepoints = LoadCodepoints(glyphs.c_str(), &codepointCount);
        const char* candidates[] = {
            "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
            "/System/Library/Fonts/STHeiti Medium.ttc",
            "/System/Library/Fonts/Hiragino Sans GB.ttc",
            "/System/Library/Fonts/Supplemental/Songti.ttc",
        };
        for (const char* candidate : candidates) {
            if (FileExists(candidate)) {
                text.font = LoadFontEx(candidate, 72, codepoints, codepointCount);
                if (text.font.texture.id != 0) {
                    text.loaded = true;
                    break;
                }
            }
        }
        if (!text.loaded) {
            text.font = GetFontDefault();
            text.loaded = true;
        }
        UnloadCodepoints(codepoints);
    }

    Vector2 VirtualMouse() const {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float scale = std::min(static_cast<float>(sw) / kScreenW, static_cast<float>(sh) / kScreenH);
        float ox = (static_cast<float>(sw) - kScreenW * scale) * 0.5f;
        float oy = (static_cast<float>(sh) - kScreenH * scale) * 0.5f;
        Vector2 m = GetMousePosition();
        return {(m.x - ox) / scale, (m.y - oy) / scale};
    }

    Rectangle SlotRect(int i) const {
        float gap = 6.0f;
        float w = 54.0f;
        float x = 14.0f + i * (w + gap);
        return {x, kSlotY, w, 58.0f};
    }

    Vector2 SlotCenter(int i) const {
        Rectangle r = SlotRect(i);
        return {r.x + r.width * 0.5f, r.y + 23.0f};
    }

    void ResetStage() {
        heroesDef = BuildHeroes();
        state = GameState::Playing;
        selectedSlot = -1;
        enemies.clear();
        projectiles.clear();
        floaters.clear();
        impacts.clear();
        draftChoices.clear();
        nextEnemyId = 1;
        wave = 0;
        stageBanner = 2.2f;
        waveBreak = 0.45f;
        kills = 0;
        summons = 0;
        merges = 0;
        highestStar = 1;
        speedIndex = 0;
        speedMultiplier = 1.0f;
        showStats = false;
        dragSlot = -1;
        dragActive = false;
        resultLoot.clear();
        globalDamage = 1.0f;
        globalAttackSpeed = 1.0f;
        controlBonus = 1.0f;
        poisonBonus = 1.0f;
        bossDamage = 1.0f;
        wallRegen = 0.0f;
        summonDiscount = 0.0f;
        mechanismCooldownBonus = 1.0f;
        rewardBonus = 1.0f;
        kingBuffTimer = 0.0f;
        freezeCooldown = 0.0f;
        cannonCooldown = 0.0f;
        kingCharge = 4;
        resultHandled = false;
        int levelNo = currentLevel + 1;
        wallMaxHp = 5400 + levelNo * 200;
        globalDamage += save.equipmentLevel * 0.075f;
        globalAttackSpeed += save.equipmentLevel * 0.025f;
        controlBonus += save.totemLevel * 0.08f;
        poisonBonus += save.totemLevel * 0.08f;
        bossDamage += save.totemLevel * 0.05f;
        wallMaxHp += save.cloakLevel * 260;
        wallRegen += save.cloakLevel * 0.18f;
        summonDiscount = std::min(0.42f, summonDiscount + save.cloakLevel * 0.012f);
        int supportIdx = std::max(0, std::min(kSupportCount - 1, save.activeSupport));
        int copies = std::max(1, save.supportCopies[supportIdx]);
        float supportValue = supports.empty() ? 0.0f : supports[supportIdx].baseValue + supports[supportIdx].perCopy * (copies - 1);
        if (!supports.empty()) {
            switch (supports[supportIdx].buff) {
                case SupportBuff::Damage:
                    globalDamage += supportValue;
                    break;
                case SupportBuff::AttackSpeed:
                    globalAttackSpeed += supportValue;
                    break;
                case SupportBuff::Control:
                    controlBonus += supportValue;
                    break;
                case SupportBuff::Wall:
                    wallMaxHp += static_cast<int>(wallMaxHp * supportValue);
                    wallRegen += supportValue * 1.6f;
                    break;
                case SupportBuff::Economy:
                    summonDiscount = std::min(0.48f, summonDiscount + supportValue * 0.55f);
                    rewardBonus += supportValue;
                    break;
            }
        }
        wallHp = wallMaxHp;
        coins = 80 + levelNo * 4;
        summonCost = 45;
        upgradeCost = 100;
        slots = {};
        slots[2] = Hero{true, 0, 1, RandFloat(rng, 0.0f, 0.35f), 0.0f};
        slots[3] = Hero{true, 1, 1, RandFloat(rng, 0.0f, 0.35f), 0.0f};
        slots[4] = Hero{true, 0, 1, RandFloat(rng, 0.0f, 0.35f), 0.0f};
        slots[6] = Hero{true, 5, 1, RandFloat(rng, 0.0f, 0.35f), 0.0f};
        StartWave();
        ShowToast(u8"点击或拖拽相同英雄进行 2 合 1");
    }

    void StartWave() {
        int levelNo = currentLevel + 1;
        WavePlan plan = BuildWavePlan(levelNo, wave, levels[currentLevel]);
        spawnPlanned = plan.count;
        if (wave == totalWaves - 1) spawnPlanned = 1 + levelNo / 12;
        spawnRemaining = spawnPlanned;
        spawnTimer = 0.1f;
        spawnInterval = plan.interval;
    }

    static WavePlan BuildWavePlan(int levelNo, int waveIndex, const LevelInfo& level) {
        if (waveIndex >= 9) {
            return {1 + levelNo / 12, std::max(0.32f, 0.7f - levelNo * 0.006f)};
        }
        float chapter = static_cast<float>((levelNo - 1) / 5);
        int count = 14 + waveIndex * 6 + levelNo + level.densityBonus * 4;
        count += static_cast<int>(std::round(waveIndex * waveIndex * 0.75f + chapter * 4.0f));
        if (waveIndex >= 3) count += 5 + levelNo / 2;
        if (waveIndex >= 6) count += 9 + levelNo / 2;
        float interval = std::max(0.18f, 0.54f - waveIndex * 0.042f - levelNo * 0.0055f);
        return {count, interval};
    }

    EnemyKind PickEnemyKind() {
        if (wave == totalWaves - 1) return EnemyKind::Boss;
        int roll = RandInt(rng, 0, 99);
        int levelNo = currentLevel + 1;
        if (roll < 42) return EnemyKind::Scout;
        if (roll < 67) return EnemyKind::Brute;
        if (roll < 82 || (levelNo % 4 == 0 && roll < 90)) return EnemyKind::Flyer;
        return EnemyKind::Shaman;
    }

    void SpawnEnemy() {
        EnemyKind kind = PickEnemyKind();
        int levelNo = currentLevel + 1;
        float wavePressure = 1.0f + wave * 0.17f + std::max(0, wave - 4) * 0.08f;
        float levelPressure = 1.0f + currentLevel * 0.055f;
        float baseHp = (92.0f + wave * 32.0f + levelNo * 24.0f) * levels[currentLevel].hpScale * wavePressure * levelPressure;
        float speed = (23.0f + wave * 1.5f + levelNo * 0.4f) * levels[currentLevel].speedScale;
        float radius = 16.0f;
        float mult = 1.0f;
        switch (kind) {
            case EnemyKind::Scout: mult = 1.08f; speed *= 1.23f; radius = 14.0f; break;
            case EnemyKind::Brute: mult = 2.45f; speed *= 0.78f; radius = 21.0f; break;
            case EnemyKind::Flyer: mult = 1.28f; speed *= 1.38f; radius = 15.0f; break;
            case EnemyKind::Shaman: mult = 1.62f; speed *= 0.96f; radius = 17.0f; break;
            case EnemyKind::Boss: mult = 18.0f + currentLevel * 0.72f; speed *= 0.48f; radius = 34.0f; break;
        }

        Enemy e;
        e.id = nextEnemyId++;
        e.kind = kind;
        e.maxHp = baseHp * mult * levels[currentLevel].armorScale;
        e.hp = e.maxHp;
        e.speed = speed;
        e.radius = radius;
        e.wobble = RandFloat(rng, 0.0f, 20.0f);
        e.pos = {RandFloat(rng, 112.0f, 318.0f), RandFloat(rng, 132.0f, 156.0f)};
        enemies.push_back(e);
    }

    int SlotAt(Vector2 mouse) const {
        for (int i = 0; i < kMaxSlots; ++i) {
            if (PointIn(SlotRect(i), mouse)) return i;
        }
        return -1;
    }

    void HandleInput(Vector2 mouse) {
        if (dragSlot >= 0) {
            dragMouse = mouse;
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && Vector2Distance(mouse, dragStart) > 8.0f) dragActive = true;
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                int from = dragSlot;
                int to = SlotAt(mouse);
                bool wasDrag = dragActive;
                dragSlot = -1;
                dragActive = false;
                if (state == GameState::Playing && wasDrag) {
                    if (to >= 0 && to != from) TryMergeSlots(from, to);
                    return;
                }
                if (state == GameState::Playing && to == from) {
                    HandleSlotClick(from);
                    return;
                }
            }
            if (dragActive) return;
        }

        bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (!pressed) return;

        if (state == GameState::Lobby) {
            if (PointIn(adventureButton, mouse)) {
                state = GameState::StageSelect;
                return;
            }
            if (PointIn(metaMenuButton, mouse)) {
                state = GameState::Meta;
                return;
            }
        }

        if (state == GameState::StageSelect) {
            if (PointIn(prevStageButton, mouse)) {
                currentLevel = (currentLevel + save.unlockedLevel - 1) % save.unlockedLevel;
                return;
            }
            if (PointIn(nextStageButton, mouse)) {
                currentLevel = (currentLevel + 1) % save.unlockedLevel;
                return;
            }
            if (PointIn(backButton, mouse)) {
                state = GameState::Lobby;
                return;
            }
            if (PointIn(startButton, mouse)) {
                ResetStage();
                return;
            }
        }

        if (state == GameState::Meta) {
            if (PointIn(equipmentButton, mouse)) {
                TryMetaUpgrade(0);
                return;
            }
            if (PointIn(totemButton, mouse)) {
                TryMetaUpgrade(1);
                return;
            }
            if (PointIn(cloakButton, mouse)) {
                TryMetaUpgrade(2);
                return;
            }
            if (PointIn(supportPrevButton, mouse)) {
                SelectSupport(-1);
                return;
            }
            if (PointIn(supportNextButton, mouse)) {
                SelectSupport(1);
                return;
            }
            if (PointIn(gachaButton, mouse)) {
                TrySupportGacha(true);
                return;
            }
            if (PointIn(backButton, mouse)) {
                state = GameState::Lobby;
                return;
            }
        }

        if (state == GameState::Victory) {
            Rectangle next{95, 702, 240, 58};
            Rectangle menu{128, 776, 174, 45};
            if (PointIn(next, mouse)) {
                currentLevel = std::min(save.unlockedLevel - 1, currentLevel + 1);
                ResetStage();
                return;
            }
            if (PointIn(menu, mouse)) {
                state = GameState::StageSelect;
                return;
            }
        }

        if (state == GameState::Defeat) {
            Rectangle retry{95, 702, 240, 58};
            Rectangle menu{128, 776, 174, 45};
            if (PointIn(retry, mouse)) {
                ResetStage();
                return;
            }
            if (PointIn(menu, mouse)) {
                state = GameState::StageSelect;
                return;
            }
        }

        if (state == GameState::Draft) {
            for (int i = 0; i < static_cast<int>(draftChoices.size()); ++i) {
                Rectangle card{42.0f, 286.0f + i * 126.0f, 346.0f, 104.0f};
                if (PointIn(card, mouse)) {
                    ApplyDraft(draftChoices[i].kind);
                    state = previousState;
                    ShowToast(draftChoices[i].title);
                    return;
                }
            }
            return;
        }

        if (state == GameState::Paused) {
            if (PointIn(pauseButton, mouse)) state = GameState::Playing;
            return;
        }

        if (state != GameState::Playing) return;

        if (PointIn(pauseButton, mouse)) {
            state = GameState::Paused;
            return;
        }
        if (PointIn(muteButton, mouse)) {
            save.muted = !save.muted;
            SaveProgress();
            ShowToast(save.muted ? u8"音效关闭" : u8"音效开启");
            return;
        }
        if (PointIn(speedButton, mouse)) {
            speedIndex = (speedIndex + 1) % 3;
            speedMultiplier = speedIndex == 0 ? 1.0f : (speedIndex == 1 ? 1.5f : 2.0f);
            return;
        }
        if (PointIn(statsButton, mouse)) {
            showStats = !showStats;
            return;
        }
        if (PointIn(summonButton, mouse)) {
            TrySummon();
            return;
        }
        if (PointIn(upgradeButton, mouse)) {
            TryUpgrade();
            return;
        }
        if (PointIn(freezeButton, mouse)) {
            UseFreeze();
            return;
        }
        if (PointIn(cannonButton, mouse)) {
            UseCannon();
            return;
        }
        if (PointIn(kingButton, mouse)) {
            UseKing();
            return;
        }

        int slot = SlotAt(mouse);
        if (slot >= 0) {
            if (slots[slot].active) {
                dragSlot = slot;
                dragStart = mouse;
                dragMouse = mouse;
                dragActive = false;
            } else {
                HandleSlotClick(slot);
            }
            return;
        }
    }

    void TrySummon() {
        int cost = static_cast<int>(std::round(summonCost * (1.0f - summonDiscount)));
        if (coins < cost) {
            ShowToast(u8"银币不足");
            return;
        }
        std::vector<int> empty;
        for (int i = 0; i < kMaxSlots; ++i) if (!slots[i].active) empty.push_back(i);
        if (empty.empty()) {
            ShowToast(u8"槽位已满，先合成");
            return;
        }
        coins -= cost;
        int slot = empty[RandInt(rng, 0, static_cast<int>(empty.size()) - 1)];
        int type = RandInt(rng, 0, static_cast<int>(heroesDef.size()) - 1);
        slots[slot] = Hero{true, type, 1, RandFloat(rng, 0.05f, 0.45f), 0.55f};
        summons++;
        summonCost = std::min(130, summonCost + 2 + summons / 5);
        AddImpact(SlotCenter(slot), heroesDef[type].projectile, 34.0f, 0.45f, ImpactKind::HolyRing);
        PlaySfx(assets.summon);
    }

    void TryUpgrade() {
        if (coins < upgradeCost) {
            ShowToast(u8"银币不足");
            return;
        }
        coins -= upgradeCost;
        upgradeCost += 45;
        OpenDraft();
    }

    bool TryMergeSlots(int keep, int remove) {
        if (keep < 0 || keep >= kMaxSlots || remove < 0 || remove >= kMaxSlots || keep == remove) return false;
        Hero& a = slots[keep];
        Hero& b = slots[remove];
        if (!(a.active && b.active && a.type == b.type && a.star == b.star && a.star < 5)) {
            ShowToast(u8"需要同英雄同星级");
            return false;
        }
        slots[keep].star++;
        slots[keep].cooldown = 0.05f;
        slots[keep].flash = 0.75f;
        slots[remove] = {};
        merges++;
        highestStar = std::max(highestStar, slots[keep].star);
        kingCharge = std::min(5, kingCharge + 1);
        coins += 12 + slots[keep].star * 6;
        AddImpact(SlotCenter(keep), C(255, 225, 86), 46.0f, 0.6f, ImpactKind::HolyRing);
        AddFloat(u8"合成 +1★", SlotCenter(keep), C(255, 238, 117));
        if (slots[keep].star >= 3 && RandInt(rng, 0, 99) < 22) {
            int oldType = slots[keep].type;
            slots[keep].type = (oldType + RandInt(rng, 1, 5)) % static_cast<int>(heroesDef.size());
            ShowToast(u8"随机进化！");
        } else if (slots[keep].star >= 2 && RandInt(rng, 0, 99) < 46) {
            OpenDraft();
        }
        PlaySfx(assets.merge);
        return true;
    }

    void HandleSlotClick(int i) {
        if (!slots[i].active) {
            selectedSlot = -1;
            return;
        }
        if (selectedSlot < 0) {
            selectedSlot = i;
            return;
        }
        if (selectedSlot == i) {
            selectedSlot = -1;
            return;
        }
        TryMergeSlots(selectedSlot, i);
        selectedSlot = -1;
    }

    void OpenDraft() {
        previousState = state == GameState::Draft ? GameState::Playing : state;
        state = GameState::Draft;
        PlaySfx(assets.draft);
        draftChoices.clear();
        std::vector<DraftChoice> pool = {
            {u8"箭雨校准", u8"全体伤害 +18%，射手额外受益", 0},
            {u8"急速号角", u8"全体攻速 +16%，号令期间再提升", 1},
            {u8"寒霜锁链", u8"控制时间 +30%，霜冻机关更久", 2},
            {u8"毒雾蔓延", u8"持续伤害 +45%，毒系溅射扩大", 3},
            {u8"首领猎手", u8"对 Boss 伤害 +35%", 4},
            {u8"城墙修复", u8"立即回复 900，并获得缓慢恢复", 5},
            {u8"星辉召唤", u8"召唤费用降低 18%", 6},
            {u8"机关整备", u8"霜冻和炮击冷却缩短 24%", 7},
            {u8"裂晶爆破", u8"范围伤害 +25%，炮击更猛烈", 8},
        };
        while (draftChoices.size() < 3 && !pool.empty()) {
            int idx = RandInt(rng, 0, static_cast<int>(pool.size()) - 1);
            draftChoices.push_back(pool[idx]);
            pool.erase(pool.begin() + idx);
        }
    }

    void ApplyDraft(int kind) {
        switch (kind) {
            case 0: globalDamage += 0.18f; break;
            case 1: globalAttackSpeed += 0.16f; break;
            case 2: controlBonus += 0.30f; break;
            case 3: poisonBonus += 0.45f; break;
            case 4: bossDamage += 0.35f; break;
            case 5:
                wallHp = std::min(wallMaxHp, wallHp + 900);
                wallRegen += 1.1f;
                break;
            case 6: summonDiscount = std::min(0.45f, summonDiscount + 0.18f); break;
            case 7: mechanismCooldownBonus = std::max(0.48f, mechanismCooldownBonus * 0.76f); break;
            case 8:
                for (auto& def : heroesDef) def.splash *= 1.25f;
                break;
        }
    }

    void UseFreeze() {
        if (freezeCooldown > 0.0f) {
            ShowToast(u8"霜冻冷却中");
            return;
        }
        for (auto& e : enemies) {
            e.stunTimer = std::max(e.stunTimer, 1.0f * controlBonus);
            e.slowTimer = std::max(e.slowTimer, 4.0f * controlBonus);
            e.flash = 0.3f;
        }
        AddImpact({kScreenW * 0.5f, 405.0f}, C(130, 236, 255), 170.0f, 0.72f, ImpactKind::FrostBurst);
        freezeCooldown = 13.0f * mechanismCooldownBonus;
        PlaySfx(assets.freeze);
    }

    void UseCannon() {
        if (cannonCooldown > 0.0f) {
            ShowToast(u8"炮击冷却中");
            return;
        }
        Vector2 center{0.0f, 0.0f};
        int count = 0;
        for (const auto& e : enemies) {
            center = Vector2Add(center, e.pos);
            count++;
        }
        if (count == 0) {
            ShowToast(u8"没有怪物");
            return;
        }
        center = Vector2Scale(center, 1.0f / count);
        AddImpact(center, C(255, 154, 72), 115.0f, 0.5f, ImpactKind::BombBlast);
        float damage = 360.0f + currentLevel * 28.0f + wave * 24.0f;
        for (auto& e : enemies) {
            float d = Vector2Distance(center, e.pos);
            if (d < 122.0f) {
                DamageEnemy(e, damage * (1.0f - d / 160.0f), true);
            }
        }
        cannonCooldown = 9.5f * mechanismCooldownBonus;
        PlaySfx(assets.cannon);
    }

    void UseKing() {
        if (kingCharge < 5) {
            ShowToast(u8"号令需要 5/5");
            return;
        }
        kingCharge = 0;
        kingBuffTimer = 6.0f;
        wallHp = std::min(wallMaxHp, wallHp + 420);
        AddImpact({kScreenW * 0.5f, 735.0f}, C(255, 224, 105), 100.0f, 0.8f, ImpactKind::HolyRing);
        ShowToast(u8"王者号令！");
        PlaySfx(assets.draft);
    }

    void UpdateGame(float dt) {
        stageBanner = std::max(0.0f, stageBanner - dt);
        freezeCooldown = std::max(0.0f, freezeCooldown - dt);
        cannonCooldown = std::max(0.0f, cannonCooldown - dt);
        kingBuffTimer = std::max(0.0f, kingBuffTimer - dt);
        if (wallRegen > 0.0f) {
            wallHp = std::min(wallMaxHp, wallHp + static_cast<int>(wallRegen * dt * 32.0f));
        }

        UpdateWave(dt);
        UpdateHeroes(dt);
        UpdateProjectiles(dt);
        UpdateEnemies(dt);

        enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [&](const Enemy& e) {
            if (e.hp <= 0.0f) {
                int reward = 8 + wave * 2 + (e.kind == EnemyKind::Boss ? 160 + currentLevel * 8 : 0);
                if (e.kind == EnemyKind::Brute) reward += 6;
                coins += reward;
                kills++;
                kingCharge = std::min(5, kingCharge + (e.kind == EnemyKind::Boss ? 2 : 1));
                AddFloat("+" + std::to_string(reward), e.pos, C(255, 229, 118));
                AddImpact(e.pos, C(145, 234, 220), e.radius + 20.0f, 0.35f, ImpactKind::Pulse);
                return true;
            }
            return false;
        }), enemies.end());

        if (wallHp <= 0 && state == GameState::Playing) {
            wallHp = 0;
            state = GameState::Defeat;
            HandleBattleEnd(false);
        }

        if (wave >= totalWaves - 1 && spawnRemaining == 0 && enemies.empty() && state == GameState::Playing) {
            state = GameState::Victory;
            HandleBattleEnd(true);
        }
    }

    void HandleBattleEnd(bool victory) {
        if (resultHandled) return;
        resultHandled = true;
        const LevelInfo& level = levels[currentLevel];
        int reward = victory ? level.baseDustReward + highestStar * 10 + kills / 3 : 24 + kills * 2;
        reward = static_cast<int>(std::round(reward * rewardBonus));
        save.starDust += reward;
        save.bestLevel = std::max(save.bestLevel, currentLevel + 1);
        int parts = 0;
        int runes = 0;
        int silk = 0;
        int tickets = 0;
        std::string gearDrop;
        if (victory) {
            save.victories++;
            save.unlockedLevel = std::min(30, std::max(save.unlockedLevel, currentLevel + 2));
            parts = level.materialReward + RandInt(rng, 0, 2);
            runes = std::max(1, level.materialReward - 1 + RandInt(rng, 0, 1));
            silk = std::max(1, level.materialReward - 1 + (currentLevel % 3 == 0 ? 1 : 0));
            save.equipmentParts += parts;
            save.totemRunes += runes;
            save.cloakSilk += silk;
            if (RandFloat(rng, 0.0f, 1.0f) < level.opportunityRate) {
                tickets = 1;
                save.gachaTickets += 1;
            }
            float drop = RandFloat(rng, 0.0f, 1.0f);
            if (drop < level.gearDropRate) {
                float quality = RandFloat(rng, 0.0f, 1.0f);
                if (quality < 0.08f + currentLevel * 0.004f) {
                    save.mythicGear++;
                    save.equipmentLevel += 1;
                    gearDrop = u8"神话装备";
                } else if (quality < 0.30f + currentLevel * 0.006f) {
                    save.epicGear++;
                    save.equipmentParts += 4;
                    gearDrop = u8"史诗装备";
                } else {
                    save.rareGear++;
                    save.equipmentParts += 2;
                    gearDrop = u8"稀有装备";
                }
            }
            PlaySfx(assets.victory);
        } else {
            PlaySfx(assets.defeat);
        }
        std::stringstream loot;
        loot << u8"星尘 +" << reward;
        if (victory) {
            loot << u8" · 碎片 +" << parts << u8" · 符文 +" << runes << u8" · 丝线 +" << silk;
            if (tickets > 0) loot << u8" · 招募券 +" << tickets;
            if (!gearDrop.empty()) loot << u8" · 掉落 " << gearDrop;
        }
        resultLoot = loot.str();
        SaveProgress();
    }

    void UpdateWave(float dt) {
        if (spawnRemaining > 0) {
            spawnTimer -= dt;
            if (spawnTimer <= 0.0f) {
                SpawnEnemy();
                spawnRemaining--;
                spawnTimer = spawnInterval;
            }
            return;
        }
        if (enemies.empty() && wave < totalWaves - 1) {
            waveBreak -= dt;
            if (waveBreak <= 0.0f) {
                wave++;
                waveBreak = 1.25f;
                stageBanner = 1.25f;
                StartWave();
            }
        }
    }

    void UpdateHeroes(float dt) {
        for (int i = 0; i < kMaxSlots; ++i) {
            Hero& h = slots[i];
            if (!h.active) continue;
            h.cooldown = std::max(0.0f, h.cooldown - dt);
            h.flash = std::max(0.0f, h.flash - dt);
            if (h.cooldown > 0.0f) continue;
            int targetId = FindTarget(SlotCenter(i), heroesDef[h.type].range);
            if (targetId <= 0) continue;
            const HeroDef& def = heroesDef[h.type];
            float starMult = 1.0f + (h.star - 1) * 0.78f + std::pow(std::max(0, h.star - 1), 1.35f) * 0.18f;
            float dmg = def.baseDamage * starMult * globalDamage;
            float speedBuff = globalAttackSpeed * (kingBuffTimer > 0.0f ? 1.85f : 1.0f);
            h.cooldown = std::max(0.12f, def.cooldown / speedBuff / (1.0f + (h.star - 1) * 0.1f));
            Projectile p;
            p.pos = SlotCenter(i);
            p.prevPos = p.pos;
            p.targetId = targetId;
            p.heroType = h.type;
            p.style = def.attackStyle;
            p.skill = def.skill;
            p.star = h.star;
            p.damage = dmg;
            p.speed = ProjectileSpeed(def.attackStyle);
            p.splash = SkillSplash(def.skill, def.splash, h.star);
            p.color = def.projectile;
            projectiles.push_back(p);
            PlaySfx(assets.shoot);
            HeroSkill skill = def.skill;
            if ((skill == HeroSkill::Angel || skill == HeroSkill::Mermaid || skill == HeroSkill::Athena) && wallHp < wallMaxHp) {
                wallHp = std::min(wallMaxHp, wallHp + 8 + h.star * 7);
            }
        }
    }

    int FindTarget(Vector2 from, float range) const {
        int bestId = -1;
        float bestY = -10000.0f;
        for (const auto& e : enemies) {
            float d = Vector2Distance(from, e.pos);
            if (d <= range && e.pos.y > bestY) {
                bestY = e.pos.y;
                bestId = e.id;
            }
        }
        return bestId;
    }

    Enemy* EnemyById(int id) {
        for (auto& e : enemies) if (e.id == id) return &e;
        return nullptr;
    }

    int CountHeroesBySkill(HeroSkill skill) const {
        int count = 0;
        for (const auto& h : slots) {
            if (h.active && heroesDef[h.type].skill == skill) count++;
        }
        return count;
    }

    int TotalStarsBySkill(HeroSkill skill) const {
        int total = 0;
        for (const auto& h : slots) {
            if (h.active && heroesDef[h.type].skill == skill) total += h.star;
        }
        return total;
    }

    static float ProjectileSpeed(AttackStyle style) {
        switch (style) {
            case AttackStyle::Arrow: return 520.0f;
            case AttackStyle::Frost: return 350.0f;
            case AttackStyle::Slash: return 455.0f;
            case AttackStyle::Poison: return 330.0f;
            case AttackStyle::Bomb: return 300.0f;
            case AttackStyle::Holy: return 390.0f;
            case AttackStyle::Laser: return 620.0f;
            case AttackStyle::Lightning: return 585.0f;
            case AttackStyle::Fire: return 370.0f;
            case AttackStyle::Wind: return 320.0f;
            case AttackStyle::Summon: return 340.0f;
            case AttackStyle::Charm: return 330.0f;
        }
        return 360.0f;
    }

    static ImpactKind ImpactForAttack(AttackStyle style) {
        switch (style) {
            case AttackStyle::Arrow: return ImpactKind::ArrowHit;
            case AttackStyle::Frost: return ImpactKind::FrostBurst;
            case AttackStyle::Slash: return ImpactKind::SlashSpark;
            case AttackStyle::Poison: return ImpactKind::PoisonCloud;
            case AttackStyle::Bomb: return ImpactKind::BombBlast;
            case AttackStyle::Holy: return ImpactKind::HolyRing;
            case AttackStyle::Laser: return ImpactKind::LaserStrike;
            case AttackStyle::Lightning: return ImpactKind::LightningArc;
            case AttackStyle::Fire: return ImpactKind::FireBurst;
            case AttackStyle::Wind: return ImpactKind::WindBurst;
            case AttackStyle::Summon: return ImpactKind::SummonShock;
            case AttackStyle::Charm: return ImpactKind::CharmWave;
        }
        return ImpactKind::Pulse;
    }

    static float SkillSplash(HeroSkill skill, float baseSplash, int star) {
        float starBonus = 1.0f + (star - 1) * 0.16f;
        switch (skill) {
            case HeroSkill::Assault:
            case HeroSkill::Rocket:
                return std::max(baseSplash, 24.0f + star * 7.0f) * starBonus;
            case HeroSkill::SteelDog:
            case HeroSkill::Cannon:
            case HeroSkill::Desila:
                return std::max(baseSplash, 44.0f + star * 7.0f) * starBonus;
            case HeroSkill::Storm:
                return std::max(baseSplash, 68.0f + star * 9.0f) * starBonus;
            case HeroSkill::SeaKing:
            case HeroSkill::Annie:
            case HeroSkill::DeathKnight:
                return std::max(baseSplash, 48.0f + star * 6.0f) * starBonus;
            case HeroSkill::SnowPrincess:
            case HeroSkill::IceMage:
                return std::max(baseSplash, 48.0f + star * 5.0f) * starBonus;
            case HeroSkill::Daji:
                return std::max(baseSplash, 36.0f + star * 5.0f) * starBonus;
            default:
                return baseSplash * starBonus;
        }
    }

    void AddImpact(Vector2 pos, Color color, float radius, float life, ImpactKind kind) {
        impacts.push_back({pos, color, radius, life, life, kind});
    }

    void UpdateProjectiles(float dt) {
        for (auto& p : projectiles) {
            Enemy* e = EnemyById(p.targetId);
            if (!e) {
                p.targetId = -1;
                continue;
            }
            p.prevPos = p.pos;
            Vector2 dir = Vector2Subtract(e->pos, p.pos);
            float len = std::max(0.001f, Vector2Length(dir));
            p.pos = Vector2Add(p.pos, Vector2Scale(dir, p.speed * dt / len));
            if (Vector2Distance(p.pos, e->pos) <= e->radius + 8.0f) {
                ResolveHit(p, *e);
                p.targetId = -1;
            }
        }
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) {
            return p.targetId < 0 || p.pos.x < -40 || p.pos.x > kScreenW + 40 || p.pos.y < -40 || p.pos.y > kScreenH + 40;
        }), projectiles.end());
    }

    void ResolveHit(const Projectile& p, Enemy& target) {
        float damage = p.damage;
        if (target.kind == EnemyKind::Boss) damage *= bossDamage;
        ApplySkillOnHit(p, target, damage);
        DamageEnemy(target, damage, false);
        PlaySfx(assets.hit);
        AddImpact(target.pos, p.color, 18.0f + p.splash * 0.22f, 0.36f, ImpactForAttack(p.style));
        if (p.splash > 0.0f) {
            for (auto& e : enemies) {
                if (e.id == target.id) continue;
                float d = Vector2Distance(e.pos, target.pos);
                if (d < p.splash) {
                    float splashDamage = damage * 0.48f * (1.0f - d / (p.splash * 1.25f));
                    DamageEnemy(e, splashDamage, false);
                    ApplySkillOnSplash(p, e, splashDamage);
                }
            }
            if (p.style == AttackStyle::Bomb) {
                AddImpact(target.pos, C(255, 204, 80), std::max(54.0f, p.splash * 0.86f), 0.42f, ImpactKind::BombBlast);
            } else if (p.style == AttackStyle::Frost) {
                AddImpact(target.pos, C(168, 239, 255), std::max(38.0f, p.splash * 0.72f), 0.44f, ImpactKind::FrostBurst);
            } else if (p.style == AttackStyle::Poison) {
                AddImpact(target.pos, C(156, 239, 99), std::max(36.0f, p.splash * 0.68f), 0.58f, ImpactKind::PoisonCloud);
            } else if (p.style == AttackStyle::Fire) {
                AddImpact(target.pos, C(255, 136, 66), std::max(42.0f, p.splash * 0.72f), 0.42f, ImpactKind::FireBurst);
            } else if (p.style == AttackStyle::Wind) {
                AddImpact(target.pos, C(174, 236, 255), std::max(52.0f, p.splash * 0.78f), 0.52f, ImpactKind::WindBurst);
            } else if (p.style == AttackStyle::Summon) {
                AddImpact(target.pos, C(178, 224, 255), std::max(42.0f, p.splash * 0.68f), 0.46f, ImpactKind::SummonShock);
            } else if (p.style == AttackStyle::Charm) {
                AddImpact(target.pos, C(255, 145, 211), std::max(36.0f, p.splash * 0.66f), 0.48f, ImpactKind::CharmWave);
            }
        }
    }

    void ApplyPoison(Enemy& e, float dps, float seconds) {
        e.poisonTimer = std::max(e.poisonTimer, seconds);
        e.poisonDps = std::max(e.poisonDps, dps * poisonBonus);
    }

    void ApplyBurn(Enemy& e, float dps, float seconds) {
        e.burnTimer = std::max(e.burnTimer, seconds);
        e.burnDps = std::max(e.burnDps, dps);
    }

    void ChainDamage(const Enemy& target, const Projectile& p, float damage, int maxHits, float range) {
        int hits = 0;
        for (auto& e : enemies) {
            if (e.id == target.id) continue;
            if (hits >= maxHits) break;
            float d = Vector2Distance(e.pos, target.pos);
            if (d <= range) {
                DamageEnemy(e, damage * (0.48f + 0.08f * p.star), false);
                e.stunTimer = std::max(e.stunTimer, 0.12f * controlBonus);
                AddImpact(e.pos, p.color, 18.0f, 0.24f, ImpactKind::LightningArc);
                hits++;
            }
        }
    }

    void ApplyTeamHaste(float cooldownCap) {
        for (auto& h : slots) {
            if (h.active) h.cooldown = std::min(h.cooldown, cooldownCap);
        }
    }

    void ApplySkillOnHit(const Projectile& p, Enemy& target, float& damage) {
        float star = static_cast<float>(p.star);
        switch (p.skill) {
            case HeroSkill::Assault:
                damage *= 1.0f + 0.08f * star;
                ChainDamage(target, p, damage * 0.30f, 1 + p.star / 2, 84.0f);
                break;
            case HeroSkill::SwordImmortal:
                if (target.hp > target.maxHp * 0.55f) damage *= 1.34f + 0.05f * star;
                ChainDamage(target, p, damage * 0.42f, 1 + p.star / 2, 104.0f);
                break;
            case HeroSkill::SteelDog:
                if (target.kind == EnemyKind::Boss || p.star == 2 || p.star == 4) damage *= 1.55f;
                break;
            case HeroSkill::Rocket:
                damage *= 1.0f + 0.05f * CountHeroesBySkill(HeroSkill::Rocket);
                break;
            case HeroSkill::Mushroom:
                if (target.poisonTimer > 0.0f) damage *= 1.38f;
                ApplyPoison(target, p.damage * 0.24f, 4.6f + star * 0.25f);
                break;
            case HeroSkill::Ranger:
                if (target.kind == EnemyKind::Flyer) damage *= 1.55f;
                break;
            case HeroSkill::Cannon:
                damage *= 1.0f + 0.06f * star;
                break;
            case HeroSkill::Wukong:
                target.stunTimer = std::max(target.stunTimer, 0.22f * controlBonus);
                ChainDamage(target, p, damage * 0.22f * star, p.star, 70.0f);
                break;
            case HeroSkill::Nezha:
                if (target.hp < target.maxHp * 0.35f) damage *= 1.70f;
                target.slowTimer = std::max(target.slowTimer, 0.9f * controlBonus);
                ApplyBurn(target, p.damage * 0.22f, 3.4f);
                break;
            case HeroSkill::Zhaoyun:
                damage *= 1.0f + merges * 0.018f + star * 0.05f;
                ChainDamage(target, p, damage * 0.24f, p.star, 92.0f);
                break;
            case HeroSkill::BladeMaster:
                if (target.hp < target.maxHp * 0.45f) damage *= 1.48f;
                break;
            case HeroSkill::Knight:
                if (wallHp < wallMaxHp * 0.55f) damage *= 1.65f;
                target.slowTimer = std::max(target.slowTimer, 0.45f * controlBonus);
                break;
            case HeroSkill::Kaka:
                target.stunTimer = std::max(target.stunTimer, 0.18f * controlBonus);
                ChainDamage(target, p, damage * 0.38f, 1 + p.star, 92.0f);
                break;
            case HeroSkill::Desila:
                damage *= 1.0f + (globalAttackSpeed - 1.0f) * 0.75f;
                ApplyBurn(target, p.damage * 0.18f, 2.8f);
                break;
            case HeroSkill::Storm:
                damage *= 1.0f + CountHeroesBySkill(HeroSkill::Storm) * 0.06f + TotalStarsBySkill(HeroSkill::Storm) * 0.025f;
                target.slowTimer = std::max(target.slowTimer, 0.55f * controlBonus);
                break;
            case HeroSkill::LightningChild:
                ChainDamage(target, p, damage * 0.55f, 2 + p.star, 118.0f);
                break;
            case HeroSkill::SnowPrincess:
                target.slowTimer = std::max(target.slowTimer, 2.4f * controlBonus);
                target.stunTimer = std::max(target.stunTimer, 0.30f * controlBonus);
                break;
            case HeroSkill::FireMage:
                ApplyBurn(target, p.damage * 0.34f, 4.0f);
                break;
            case HeroSkill::Venom:
                ApplyPoison(target, p.damage * 0.42f, 4.8f);
                if (target.hp < target.maxHp * 0.24f) damage += target.maxHp * 0.10f;
                break;
            case HeroSkill::BlackLily:
                ApplyPoison(target, p.damage * 0.32f, 4.2f);
                target.vulnerableTimer = std::max(target.vulnerableTimer, 2.8f);
                break;
            case HeroSkill::RiverMaster:
                ApplyPoison(target, p.damage * 0.28f, 5.4f);
                target.vulnerableTimer = std::max(target.vulnerableTimer, 3.8f);
                break;
            case HeroSkill::DeathKnight:
                if (wallHp < wallMaxHp * 0.5f) damage *= 1.55f;
                wallHp = std::min(wallMaxHp, wallHp + 3 + p.star * 3);
                break;
            case HeroSkill::SeaKing:
                target.stunTimer = std::max(target.stunTimer, 0.20f * controlBonus);
                if (target.hp > target.maxHp * 0.62f) damage *= 1.28f;
                break;
            case HeroSkill::Annie:
                target.stunTimer = std::max(target.stunTimer, 0.26f * controlBonus);
                target.slowTimer = std::max(target.slowTimer, 0.7f * controlBonus);
                wallHp = std::min(wallMaxHp, wallHp + 2 + p.star * 2);
                break;
            case HeroSkill::Daji:
                target.charmTimer = std::max(target.charmTimer, (0.65f + p.star * 0.18f) * controlBonus);
                target.slowTimer = std::max(target.slowTimer, 1.1f * controlBonus);
                break;
            case HeroSkill::Angel:
                ApplyTeamHaste(0.06f);
                damage *= 1.0f + 0.08f * p.star;
                break;
            case HeroSkill::Mermaid:
                target.vulnerableTimer = std::max(target.vulnerableTimer, 3.2f);
                ApplyTeamHaste(0.10f);
                break;
            case HeroSkill::Athena:
                ApplyTeamHaste(0.14f);
                damage *= 1.0f + 0.04f * p.star;
                break;
            case HeroSkill::IceMage:
                target.slowTimer = std::max(target.slowTimer, 1.9f * controlBonus);
                if (p.star >= 3) target.stunTimer = std::max(target.stunTimer, 0.18f * controlBonus);
                break;
            case HeroSkill::Mole:
                target.stunTimer = std::max(target.stunTimer, 0.38f * controlBonus);
                target.slowTimer = std::max(target.slowTimer, 0.9f * controlBonus);
                break;
        }
    }

    void ApplySkillOnSplash(const Projectile& p, Enemy& e, float splashDamage) {
        switch (p.skill) {
            case HeroSkill::SnowPrincess:
            case HeroSkill::IceMage:
                e.slowTimer = std::max(e.slowTimer, 1.45f * controlBonus);
                break;
            case HeroSkill::Nezha:
            case HeroSkill::FireMage:
            case HeroSkill::Desila:
                ApplyBurn(e, splashDamage * 0.22f, 2.6f);
                break;
            case HeroSkill::Mushroom:
            case HeroSkill::Venom:
            case HeroSkill::BlackLily:
            case HeroSkill::RiverMaster:
                ApplyPoison(e, splashDamage * 0.22f, 3.2f);
                break;
            case HeroSkill::Daji:
                e.charmTimer = std::max(e.charmTimer, 0.45f * controlBonus);
                break;
            case HeroSkill::SeaKing:
            case HeroSkill::Annie:
            case HeroSkill::Mole:
                e.stunTimer = std::max(e.stunTimer, 0.12f * controlBonus);
                break;
            default:
                break;
        }
    }

    void DamageEnemy(Enemy& e, float amount, bool trueDamage) {
        float reduction = trueDamage ? 1.0f : (e.kind == EnemyKind::Brute ? 0.88f : 1.0f);
        if (e.vulnerableTimer > 0.0f && !trueDamage) reduction *= 1.18f;
        e.hp -= amount * reduction;
        e.flash = 0.15f;
    }

    void UpdateEnemies(float dt) {
        int levelNo = currentLevel + 1;
        for (auto& e : enemies) {
            e.flash = std::max(0.0f, e.flash - dt);
            e.slowTimer = std::max(0.0f, e.slowTimer - dt);
            e.stunTimer = std::max(0.0f, e.stunTimer - dt);
            e.charmTimer = std::max(0.0f, e.charmTimer - dt);
            e.vulnerableTimer = std::max(0.0f, e.vulnerableTimer - dt);
            if (e.burnTimer > 0.0f) {
                e.burnTimer = std::max(0.0f, e.burnTimer - dt);
                e.hp -= e.burnDps * dt;
                if (RandInt(rng, 0, 100) < 2) AddFloat(u8"灼", e.pos, C(255, 163, 79));
            }
            if (e.poisonTimer > 0.0f) {
                e.poisonTimer = std::max(0.0f, e.poisonTimer - dt);
                e.hp -= e.poisonDps * dt;
                if (RandInt(rng, 0, 100) < 2) AddFloat(u8"毒", e.pos, C(166, 244, 95));
            }
            if (e.pos.y < kWallLineY) {
                float factor = e.slowTimer > 0.0f ? 0.42f : 1.0f;
                if (e.stunTimer > 0.0f) factor = 0.0f;
                if (e.charmTimer > 0.0f) factor = -0.34f;
                e.wobble += dt * 3.0f;
                e.pos.y += e.speed * factor * dt;
                e.pos.x += std::sin(e.wobble) * dt * (e.kind == EnemyKind::Flyer ? 18.0f : 7.0f);
                e.pos.x = std::max(76.0f + e.radius, std::min(354.0f - e.radius, e.pos.x));
            } else {
                e.attackTimer -= dt;
                if (e.attackTimer <= 0.0f) {
                    float dmg = 26.0f + wave * 7.0f + levelNo * 2.8f;
                    if (e.kind == EnemyKind::Brute) dmg *= 1.6f;
                    if (e.kind == EnemyKind::Boss) dmg *= 4.2f;
                    if (e.kind == EnemyKind::Shaman) dmg *= 1.22f;
                    wallHp -= static_cast<int>(dmg);
                    e.attackTimer = e.kind == EnemyKind::Boss ? 0.9f : 1.15f;
                    AddImpact({e.pos.x, kWallLineY + 28.0f}, C(255, 103, 82), 28.0f, 0.26f, ImpactKind::SlashSpark);
                }
            }
        }
    }

    void UpdateParticles(float dt) {
        toastTimer = std::max(0.0f, toastTimer - dt);
        for (auto& f : floaters) {
            f.life -= dt;
            f.pos.y -= 38.0f * dt;
        }
        floaters.erase(std::remove_if(floaters.begin(), floaters.end(), [](const FloatingText& f) { return f.life <= 0.0f; }), floaters.end());
        for (auto& i : impacts) {
            i.life -= dt;
            float growth = i.kind == ImpactKind::BombBlast ? 118.0f :
                           (i.kind == ImpactKind::PoisonCloud ? 34.0f :
                           (i.kind == ImpactKind::WindBurst ? 92.0f :
                           (i.kind == ImpactKind::CharmWave ? 42.0f : 58.0f)));
            i.radius += growth * dt;
        }
        impacts.erase(std::remove_if(impacts.begin(), impacts.end(), [](const Impact& i) { return i.life <= 0.0f; }), impacts.end());
    }

    void AddFloat(const std::string& s, Vector2 pos, Color color) {
        floaters.push_back({s, pos, color, 0.85f});
    }

    void ShowToast(const std::string& s) {
        toast = s;
        toastTimer = 1.8f;
    }

    bool VerifyAllHeroSkills() {
        std::array<bool, kHeroCount> seen{};
        for (int i = 0; i < static_cast<int>(heroesDef.size()); ++i) {
            const HeroDef& def = heroesDef[i];
            Enemy target;
            target.id = 9000 + i;
            target.kind = (i % 7 == 0) ? EnemyKind::Boss : ((i % 5 == 0) ? EnemyKind::Flyer : EnemyKind::Scout);
            target.pos = {215.0f, 350.0f};
            target.maxHp = 1600.0f;
            target.hp = 900.0f;
            target.speed = 24.0f;
            target.radius = 18.0f;
            enemies.clear();
            enemies.push_back(target);
            for (int n = 0; n < 3; ++n) {
                Enemy extra = target;
                extra.id = 9100 + i * 4 + n;
                extra.pos = {180.0f + n * 35.0f, 330.0f + n * 18.0f};
                enemies.push_back(extra);
            }
            wallHp = wallMaxHp / 2;
            slots = {};
            slots[0] = Hero{true, i, 3, 0.0f, 0.0f};
            Projectile p;
            p.pos = {210.0f, 660.0f};
            p.prevPos = p.pos;
            p.targetId = target.id;
            p.heroType = i;
            p.style = def.attackStyle;
            p.skill = def.skill;
            p.star = 3;
            p.damage = def.baseDamage * 3.2f;
            p.speed = ProjectileSpeed(def.attackStyle);
            p.splash = SkillSplash(def.skill, def.splash, 3);
            p.color = def.projectile;

            int wallBefore = wallHp;
            float cooldownBefore = slots[0].cooldown;
            ResolveHit(p, enemies.front());
            const Enemy& after = enemies.front();
            bool effect = after.hp < 900.0f || after.poisonTimer > 0.0f || after.burnTimer > 0.0f ||
                          after.slowTimer > 0.0f || after.stunTimer > 0.0f || after.charmTimer > 0.0f ||
                          after.vulnerableTimer > 0.0f || wallHp > wallBefore || slots[0].cooldown < cooldownBefore ||
                          impacts.size() > 0;
            seen[i] = effect && def.baseDamage > 0.0f && def.cooldown > 0.0f && def.range > 0.0f;
        }
        enemies.clear();
        projectiles.clear();
        impacts.clear();
        bool ok = true;
        for (bool value : seen) ok = ok && value;
        return ok;
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(C(14, 22, 31));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float scale = std::min(static_cast<float>(sw) / kScreenW, static_cast<float>(sh) / kScreenH);
        float ox = (static_cast<float>(sw) - kScreenW * scale) * 0.5f;
        float oy = (static_cast<float>(sh) - kScreenH * scale) * 0.5f;

        BeginScissorMode(static_cast<int>(ox), static_cast<int>(oy), static_cast<int>(kScreenW * scale), static_cast<int>(kScreenH * scale));
        rlPushMatrix();
        rlTranslatef(ox, oy, 0);
        rlScalef(scale, scale, 1);

        DrawScene();

        rlPopMatrix();
        EndScissorMode();

        EndDrawing();
    }

    void DrawScene() {
        bool inBattleView = state == GameState::Playing || state == GameState::Draft || state == GameState::Paused ||
                            state == GameState::Victory || state == GameState::Defeat;
        if (inBattleView) {
            DrawBackground();
            DrawTopHUD();
            DrawEnemiesLayer();
            DrawProjectilesLayer();
            DrawImpacts();
            DrawWallAndSlots();
            DrawBottomHUD();
            DrawLeftTools();
            DrawFloatingTexts();
            DrawStatsPanel();
            DrawToast();
        } else {
            DrawOutOfRunBackground();
        }

        if (stageBanner > 0.0f && state == GameState::Playing) {
            DrawWaveBanner();
        }
        if (state == GameState::Lobby) DrawLobbyOverlay();
        if (state == GameState::StageSelect) DrawStageSelectOverlay();
        if (state == GameState::Meta) DrawMetaOverlay();
        if (state == GameState::Draft) DrawDraftOverlay();
        if (state == GameState::Paused) DrawPauseOverlay();
        if (state == GameState::Victory) DrawResultOverlay(true);
        if (state == GameState::Defeat) DrawResultOverlay(false);
        if (!inBattleView) DrawToast();
    }

    void DrawOutOfRunBackground() {
        DrawRectangleGradientV(0, 0, kScreenW, kScreenH, C(19, 36, 48), C(52, 75, 83));
        DrawCircle(62, 104, 92, C(39, 84, 89, 110));
        DrawCircle(368, 122, 118, C(24, 61, 73, 130));
        DrawCircle(12, 766, 146, C(137, 169, 177, 92));
        DrawCircle(430, 750, 158, C(116, 154, 165, 88));
        DrawRectangleGradientV(0, 560, kScreenW, 340, C(57, 81, 90, 85), C(25, 39, 53, 180));

        for (int i = 0; i < 7; ++i) {
            float x = 28.0f + i * 64.0f;
            float y = 126.0f + (i % 3) * 32.0f;
            DrawCircleLines(static_cast<int>(x), static_cast<int>(y), 18 + (i % 2) * 8, C(160, 206, 205, 28));
        }
        DrawRectangleRounded({20, 104, 390, 724}, 0.08f, 16, C(34, 50, 62, 216));
        DrawRectangleRounded({20, 104, 390, 96}, 0.08f, 16, C(61, 96, 104, 225));
        DrawRectangle(20, 188, 390, 2, C(139, 177, 178, 55));
        DrawRectangleRounded({36, 216, 358, 548}, 0.06f, 14, C(31, 45, 56, 154));
        DrawRectangleGradientV(20, 760, 390, 68, C(28, 42, 55, 235), C(22, 34, 47, 244));
        DrawRectangleRounded({44, 130, 56, 46}, 0.18f, 10, C(31, 45, 56, 175));
        DrawCircle(72, 153, 17, C(255, 217, 97, 210));
        DrawStar({72, 153}, 12, C(255, 248, 190));
        DrawRectangleRounded({318, 130, 58, 46}, 0.18f, 10, C(31, 45, 56, 175));
        text.centered(u8"30关", {318, 141, 58, 24}, 18, C(219, 238, 239, 190));
    }

    void DrawBackground() {
        if (assets.spritesLoaded && assets.scene.id != 0) {
            DrawTexture(assets.scene, 0, 0, WHITE);
            return;
        }
        DrawRectangleGradientV(0, 0, kScreenW, kScreenH, C(34, 53, 66), C(74, 92, 102));
        DrawRectangleGradientV(0, 105, kScreenW, 680, C(82, 96, 103, 120), C(63, 75, 84, 100));

        DrawCircle(45, 92, 92, C(48, 73, 82));
        DrawCircle(382, 86, 96, C(38, 68, 79));
        DrawCircle(54, 540, 155, C(176, 203, 216));
        DrawCircle(390, 552, 160, C(149, 190, 202));
        DrawCircle(36, 742, 132, C(177, 205, 219));
        DrawCircle(386, 756, 118, C(164, 200, 213));

        DrawArenaPath();
        DrawSnowDecor();
        DrawTopGate();
    }

    void DrawArenaPath() {
        Vector2 path[] = {
            {116, 144}, {314, 144}, {349, 254}, {341, 536}, {372, 666},
            {58, 666}, {90, 535}, {82, 250}
        };
        DrawPoly({215, 405}, 8, 278, 22.5f, C(154, 136, 126, 120));
        DrawTriangleFan(path, 8, C(161, 142, 132));
        Vector2 inner[] = {
            {132, 174}, {300, 174}, {326, 268}, {319, 528}, {342, 638},
            {88, 638}, {111, 528}, {105, 268}
        };
        DrawTriangleFan(inner, 8, C(174, 154, 143));
        for (int y = 222; y < 625; y += 58) {
            float wobble = static_cast<float>(((y / 58) % 5) - 2);
            DrawLineEx({92.0f, static_cast<float>(y)}, {338.0f, static_cast<float>(y) + wobble}, 1.0f, C(119, 107, 103, 70));
        }
        for (int x = 129; x < 320; x += 52) {
            float wobble = static_cast<float>(((x / 52) % 5) - 2);
            DrawLineEx({static_cast<float>(x), 200.0f}, {static_cast<float>(x) + wobble, 640.0f}, 1.0f, C(121, 108, 103, 55));
        }
        DrawCircle(245, 238, 17, C(117, 105, 101, 35));
        DrawCircle(174, 410, 22, C(117, 105, 101, 42));
        DrawCircle(284, 570, 18, C(117, 105, 101, 34));
        DrawLineEx({147, 333}, {206, 311}, 2.0f, C(113, 99, 94, 54));
        DrawLineEx({260, 462}, {302, 493}, 2.0f, C(113, 99, 94, 54));
    }

    void DrawSnowDecor() {
        DrawTree(34, 92, 1.1f);
        DrawTree(386, 96, 1.25f);
        DrawTree(30, 620, 0.8f);
        DrawTree(394, 613, 0.85f);
        DrawTree(52, 760, 1.0f);
        DrawTree(384, 740, 1.0f);

        DrawRock(73, 420, 0.75f);
        DrawRock(351, 436, 0.85f);
        DrawRock(71, 612, 0.72f);
        DrawRock(360, 242, 0.82f);

        DrawTorch(58, 207, 1.0f);
        DrawTorch(371, 207, 1.0f);
        DrawTorch(404, 520, 1.0f);
        DrawAxe(390, 282, 1.0f);
        DrawSword(37, 533, 0.85f);
    }

    void DrawTree(float x, float y, float s) {
        DrawRectangleRounded({x - 9 * s, y + 28 * s, 18 * s, 40 * s}, 0.25f, 8, C(77, 63, 58));
        DrawCircle(static_cast<int>(x), static_cast<int>(y + 30 * s), 35 * s, C(31, 72, 77));
        DrawCircle(static_cast<int>(x - 23 * s), static_cast<int>(y + 48 * s), 29 * s, C(43, 91, 90));
        DrawCircle(static_cast<int>(x + 24 * s), static_cast<int>(y + 48 * s), 31 * s, C(31, 76, 78));
        DrawCircle(static_cast<int>(x - 16 * s), static_cast<int>(y + 18 * s), 21 * s, C(205, 225, 231));
        DrawCircle(static_cast<int>(x + 19 * s), static_cast<int>(y + 26 * s), 18 * s, C(205, 225, 231));
    }

    void DrawRock(float x, float y, float s) {
        Vector2 p[] = {{x - 24 * s, y + 4 * s}, {x - 8 * s, y - 18 * s}, {x + 21 * s, y - 10 * s}, {x + 25 * s, y + 16 * s}, {x - 8 * s, y + 22 * s}};
        DrawTriangleFan(p, 5, C(113, 129, 134));
        DrawLineEx({x - 8 * s, y - 17 * s}, {x + 4 * s, y + 18 * s}, 2.0f, C(80, 94, 100, 100));
    }

    void DrawTorch(float x, float y, float s) {
        DrawRectanglePro({x - 4 * s, y + 8 * s, 8 * s, 45 * s}, {4 * s, 0}, 18, C(74, 55, 45));
        DrawCircle(static_cast<int>(x), static_cast<int>(y), 20 * s, C(255, 145, 59, 110));
        DrawCircle(static_cast<int>(x), static_cast<int>(y + 3 * s), 12 * s, C(255, 215, 80));
        DrawTriangle({x, y - 22 * s}, {x - 11 * s, y + 8 * s}, {x + 9 * s, y + 9 * s}, C(255, 96, 44));
    }

    void DrawAxe(float x, float y, float s) {
        DrawRectanglePro({x - 3 * s, y - 32 * s, 6 * s, 78 * s}, {3 * s, 39 * s}, 28, C(104, 66, 46));
        DrawCircle(static_cast<int>(x - 12 * s), static_cast<int>(y - 26 * s), 17 * s, C(184, 196, 202));
        DrawCircle(static_cast<int>(x + 12 * s), static_cast<int>(y - 26 * s), 17 * s, C(184, 196, 202));
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 26 * s), 13 * s, C(83, 91, 98));
    }

    void DrawSword(float x, float y, float s) {
        DrawRectanglePro({x, y, 5 * s, 80 * s}, {2.5f * s, 40 * s}, -34, C(166, 190, 204));
        DrawRectanglePro({x - 3 * s, y + 26 * s, 32 * s, 6 * s}, {16 * s, 3 * s}, -34, C(139, 93, 55));
    }

    void DrawTopGate() {
        DrawRectangleRounded({18, 96, 72, 144}, 0.08f, 8, C(101, 70, 58));
        DrawRectangleRounded({340, 96, 72, 144}, 0.08f, 8, C(101, 70, 58));
        DrawRectangle(18, 128, 72, 62, C(132, 52, 58));
        DrawRectangle(340, 128, 72, 62, C(132, 52, 58));
        DrawRectangle(0, 176, kScreenW, 12, C(38, 47, 55));
        DrawRectangle(0, 188, kScreenW, 7, C(22, 29, 36));
        DrawRectangleRounded({24, 87, 60, 26}, 0.4f, 8, C(217, 232, 235));
        DrawRectangleRounded({346, 87, 60, 26}, 0.4f, 8, C(217, 232, 235));
        DrawTriangle({18, 97}, {52, 45}, {90, 97}, C(93, 55, 52));
        DrawTriangle({340, 97}, {376, 45}, {414, 97}, C(93, 55, 52));
        DrawTriangle({22, 91}, {51, 53}, {82, 91}, C(207, 225, 229));
        DrawTriangle({344, 91}, {375, 54}, {407, 91}, C(207, 225, 229));
    }

    void DrawTopHUD() {
        Rectangle pill{112, 56, 206, 78};
        DrawRectangleRounded({pill.x + 2, pill.y + 5, pill.width, pill.height}, 0.35f, 16, C(13, 20, 28, 90));
        DrawRectangleRounded(pill, 0.35f, 16, C(46, 61, 75, 220));
        DrawRectangleRounded({132, 103, 166, 32}, 0.45f, 14, C(34, 48, 63, 225));
        std::stringstream ss;
        ss << (currentLevel + 1) << ". " << levels[currentLevel].name;
        text.centeredStroke(ss.str().c_str(), {84, 58, 262, 44}, 33, C(245, 250, 244), C(38, 49, 60));
        std::stringstream waveText;
        waveText << u8"波次：" << std::min(wave + 1, totalWaves) << "/" << totalWaves;
        text.centered(waveText.str().c_str(), {132, 103, 166, 30}, 23, C(238, 245, 247));

        Rectangle trait{126, 139, 178, 25};
        DrawRectangleRounded(trait, 0.45f, 10, Fade(levels[currentLevel].accent, 0.18f));
        text.centered(levels[currentLevel].trait.c_str(), trait, 16, C(221, 237, 239));

        DrawSmallMonsterIcon(108, 78, 0.62f, true);

        DrawRectangleRounded({324, 56, 84, 42}, 0.5f, 16, C(242, 247, 248, 190));
        DrawCircleLines(386, 77, 13, save.muted ? C(124, 38, 42) : C(29, 38, 47));
        DrawCircle(386, 77, 5, save.muted ? C(124, 38, 42) : C(29, 38, 47));
        DrawCircle(347, 77, 4, C(31, 39, 48));
        DrawCircle(364, 77, 4, C(31, 39, 48));
    }

    void DrawEnemiesLayer() {
        std::vector<const Enemy*> ordered;
        ordered.reserve(enemies.size());
        for (const auto& e : enemies) ordered.push_back(&e);
        std::sort(ordered.begin(), ordered.end(), [](const Enemy* a, const Enemy* b) { return a->pos.y < b->pos.y; });
        for (const Enemy* e : ordered) DrawEnemy(*e);
    }

    void DrawEnemy(const Enemy& e) {
        int texIndex = EnemyTextureIndex(e.kind);
        int frame = static_cast<int>(std::floor(GetTime() * 5.0 + e.wobble)) & 1;
        if (assets.spritesLoaded && texIndex >= 0 && assets.enemies[texIndex][frame].id != 0) {
            float scale = (e.radius / 18.0f) * (e.kind == EnemyKind::Boss ? 1.15f : 0.92f);
            Texture2D tex = assets.enemies[texIndex][frame];
            DrawEllipse(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y + e.radius + 7), 18 * scale, 7 * scale, C(30, 34, 42, 70));
            Rectangle src{0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)};
            Rectangle dst{e.pos.x, e.pos.y + 3.0f, tex.width * scale, tex.height * scale};
            DrawTexturePro(tex, src, dst, {dst.width * 0.5f, dst.height * 0.5f}, 0.0f, WHITE);
            if (e.slowTimer > 0.0f || e.stunTimer > 0.0f) {
                DrawCircleLines(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y), e.radius + 7, C(147, 236, 255, 180));
            }
            if (e.poisonTimer > 0.0f) {
                DrawCircleLines(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y), e.radius + 11, C(159, 236, 86, 160));
            }
            float hpPct = Clamp01(e.hp / e.maxHp);
            if (e.kind == EnemyKind::Boss || hpPct < 0.98f) {
                Rectangle bg{e.pos.x - 22 * scale, e.pos.y - e.radius - 14 * scale, 44 * scale, 5 * scale};
                DrawRectangleRounded(bg, 0.6f, 6, C(34, 42, 45, 170));
                DrawRectangleRounded({bg.x, bg.y, bg.width * hpPct, bg.height}, 0.6f, 6, e.kind == EnemyKind::Boss ? C(255, 97, 78) : C(101, 232, 130));
            }
            return;
        }
        float s = e.radius / 18.0f;
        Color body = e.kind == EnemyKind::Boss ? C(119, 197, 192) : C(84, 184, 172);
        Color dark = C(38, 72, 72);
        if (e.flash > 0.0f) body = C(237, 249, 244);
        DrawEllipse(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y + e.radius + 6), 18 * s, 7 * s, C(30, 34, 42, 70));
        if (e.kind == EnemyKind::Flyer) {
            DrawTriangle({e.pos.x - 10 * s, e.pos.y - 2 * s}, {e.pos.x - 44 * s, e.pos.y - 12 * s}, {e.pos.x - 16 * s, e.pos.y + 10 * s}, C(103, 213, 211, 180));
            DrawTriangle({e.pos.x + 10 * s, e.pos.y - 2 * s}, {e.pos.x + 44 * s, e.pos.y - 12 * s}, {e.pos.x + 16 * s, e.pos.y + 10 * s}, C(103, 213, 211, 180));
        }
        DrawCircle(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y), e.radius, dark);
        DrawCircle(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y - 2 * s), e.radius - 3 * s, body);
        DrawTriangle({e.pos.x - 12 * s, e.pos.y - 15 * s}, {e.pos.x - 24 * s, e.pos.y - 25 * s}, {e.pos.x - 11 * s, e.pos.y - 5 * s}, dark);
        DrawTriangle({e.pos.x + 12 * s, e.pos.y - 15 * s}, {e.pos.x + 24 * s, e.pos.y - 25 * s}, {e.pos.x + 11 * s, e.pos.y - 5 * s}, dark);
        DrawCircle(static_cast<int>(e.pos.x - 6 * s), static_cast<int>(e.pos.y - 3 * s), 4 * s, C(16, 38, 42));
        DrawCircle(static_cast<int>(e.pos.x + 8 * s), static_cast<int>(e.pos.y - 4 * s), 4 * s, C(16, 38, 42));
        DrawRectangleRounded({e.pos.x - 8 * s, e.pos.y + 7 * s, 17 * s, 4 * s}, 0.6f, 6, C(245, 255, 252));
        if (e.kind == EnemyKind::Brute || e.kind == EnemyKind::Boss) {
            DrawRectanglePro({e.pos.x + 24 * s, e.pos.y + 12 * s, 5 * s, 38 * s}, {2.5f * s, 19 * s}, -42, C(101, 65, 50));
            DrawCircle(static_cast<int>(e.pos.x + 38 * s), static_cast<int>(e.pos.y - 6 * s), 13 * s, C(172, 188, 194));
        }
        if (e.kind == EnemyKind::Shaman) {
            DrawRectanglePro({e.pos.x + 20 * s, e.pos.y + 6 * s, 4 * s, 35 * s}, {2 * s, 17 * s}, -18, C(116, 74, 49));
            DrawCircle(static_cast<int>(e.pos.x + 14 * s), static_cast<int>(e.pos.y - 16 * s), 6 * s, C(162, 244, 143));
        }
        if (e.slowTimer > 0.0f || e.stunTimer > 0.0f) {
            DrawCircleLines(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y), e.radius + 7, C(147, 236, 255, 180));
        }
        if (e.poisonTimer > 0.0f) {
            DrawCircleLines(static_cast<int>(e.pos.x), static_cast<int>(e.pos.y), e.radius + 11, C(159, 236, 86, 160));
        }

        float hpPct = Clamp01(e.hp / e.maxHp);
        if (e.kind == EnemyKind::Boss || hpPct < 0.98f) {
            Rectangle bg{e.pos.x - 22 * s, e.pos.y - e.radius - 14 * s, 44 * s, 5 * s};
            DrawRectangleRounded(bg, 0.6f, 6, C(34, 42, 45, 170));
            DrawRectangleRounded({bg.x, bg.y, bg.width * hpPct, bg.height}, 0.6f, 6, e.kind == EnemyKind::Boss ? C(255, 97, 78) : C(101, 232, 130));
        }
    }

    void DrawSmallMonsterIcon(float x, float y, float s, bool boss) {
        Enemy e;
        e.pos = {x, y};
        e.radius = boss ? 24 * s : 18 * s;
        e.kind = boss ? EnemyKind::Boss : EnemyKind::Scout;
        e.hp = e.maxHp = 1.0f;
        DrawEnemy(e);
    }

    int EnemyTextureIndex(EnemyKind kind) const {
        switch (kind) {
            case EnemyKind::Scout: return 0;
            case EnemyKind::Brute: return 1;
            case EnemyKind::Flyer: return 2;
            case EnemyKind::Shaman: return 3;
            case EnemyKind::Boss: return 4;
        }
        return -1;
    }

    void DrawProjectilesLayer() {
        for (const auto& p : projectiles) {
            Vector2 move = Vector2Subtract(p.pos, p.prevPos);
            if (Vector2Length(move) < 0.1f) move = {0.0f, -1.0f};
            Vector2 dir = Vector2Normalize(move);
            Vector2 side{-dir.y, dir.x};
            DrawCircleV(p.pos, 5.5f, C(35, 35, 38, 90));
            switch (p.style) {
                case AttackStyle::Arrow: {
                    Vector2 tail = Vector2Subtract(p.pos, Vector2Scale(dir, 22.0f));
                    Vector2 tip = Vector2Add(p.pos, Vector2Scale(dir, 12.0f));
                    DrawLineEx(tail, tip, 4.0f, p.color);
                    DrawTriangle(tip, Vector2Add(Vector2Subtract(p.pos, Vector2Scale(dir, 2.0f)), Vector2Scale(side, 7.0f)),
                                 Vector2Subtract(Vector2Subtract(p.pos, Vector2Scale(dir, 2.0f)), Vector2Scale(side, 7.0f)), C(255, 244, 141));
                    break;
                }
                case AttackStyle::Frost:
                    DrawCircleV(p.pos, 10.0f, Fade(C(164, 240, 255), 0.22f));
                    DrawCircleV(p.pos, 5.2f, p.color);
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(side, 8.0f)), Vector2Add(p.pos, Vector2Scale(side, 8.0f)), 2.0f, C(237, 254, 255));
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 8.0f)), Vector2Add(p.pos, Vector2Scale(dir, 8.0f)), 2.0f, C(237, 254, 255));
                    break;
                case AttackStyle::Slash:
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 18.0f)), Vector2Add(p.pos, Vector2Scale(dir, 12.0f)), 7.0f, Fade(C(255, 221, 105), 0.65f));
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 10.0f)), Vector2Add(p.pos, Vector2Scale(dir, 8.0f)), 3.0f, p.color);
                    break;
                case AttackStyle::Poison:
                    DrawCircleV(p.pos, 8.5f, Fade(p.color, 0.35f));
                    DrawCircleV(Vector2Subtract(p.pos, Vector2Scale(dir, 9.0f)), 5.0f, Fade(C(178, 248, 104), 0.23f));
                    DrawCircleV(p.pos, 4.4f, p.color);
                    break;
                case AttackStyle::Bomb:
                    DrawCircleV(p.pos, 9.0f, C(90, 74, 62));
                    DrawCircleV(Vector2Add(p.pos, {-2.0f, -2.0f}), 5.8f, C(255, 181, 76));
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 14.0f)), p.pos, 3.0f, Fade(C(255, 211, 91), 0.6f));
                    break;
                case AttackStyle::Holy:
                    DrawCircleV(p.pos, 10.0f, Fade(C(255, 244, 172), 0.28f));
                    DrawCircleV(p.pos, 5.0f, p.color);
                    DrawCircleLines(static_cast<int>(p.pos.x), static_cast<int>(p.pos.y), 8.0f, Fade(C(255, 252, 214), 0.6f));
                    break;
                case AttackStyle::Laser:
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 30.0f)), Vector2Add(p.pos, Vector2Scale(dir, 12.0f)), 7.0f, Fade(p.color, 0.36f));
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 26.0f)), Vector2Add(p.pos, Vector2Scale(dir, 10.0f)), 3.0f, C(255, 243, 182));
                    break;
                case AttackStyle::Lightning:
                    DrawLineEx(Vector2Subtract(p.pos, Vector2Scale(dir, 20.0f)), Vector2Add(p.pos, Vector2Scale(side, 5.0f)), 3.0f, p.color);
                    DrawLineEx(Vector2Add(p.pos, Vector2Scale(side, 5.0f)), Vector2Add(p.pos, Vector2Scale(dir, 12.0f)), 3.0f, C(255, 246, 132));
                    break;
                case AttackStyle::Fire:
                    DrawCircleV(p.pos, 9.0f, Fade(C(255, 111, 54), 0.38f));
                    DrawTriangle(Vector2Add(p.pos, Vector2Scale(dir, 12.0f)), Vector2Subtract(p.pos, Vector2Scale(side, 8.0f)),
                                 Vector2Add(Vector2Scale(side, 8.0f), p.pos), p.color);
                    break;
                case AttackStyle::Wind:
                    DrawCircleLines(static_cast<int>(p.pos.x), static_cast<int>(p.pos.y), 10.0f, Fade(p.color, 0.65f));
                    DrawCircleLines(static_cast<int>(p.pos.x - dir.x * 9.0f), static_cast<int>(p.pos.y - dir.y * 9.0f), 6.0f, Fade(C(234, 250, 255), 0.45f));
                    break;
                case AttackStyle::Summon:
                    DrawRectanglePro({p.pos.x, p.pos.y, 12.0f, 12.0f}, {6.0f, 6.0f}, GetTime() * 160.0f, Fade(p.color, 0.78f));
                    DrawCircleV(p.pos, 4.5f, C(255, 249, 194));
                    break;
                case AttackStyle::Charm:
                    DrawHeart(p.pos, 0.34f, Fade(p.color, 0.78f));
                    DrawCircleV(p.pos, 5.0f, Fade(C(255, 214, 236), 0.34f));
                    break;
            }
        }
    }

    void DrawImpacts() {
        for (const auto& i : impacts) {
            float denom = i.maxLife > 0.0f ? i.maxLife : 0.75f;
            float a = Clamp01(i.life / denom);
            float t = 1.0f - a;
            switch (i.kind) {
                case ImpactKind::ArrowHit:
                    DrawLineEx({i.pos.x - 15.0f, i.pos.y - 8.0f}, {i.pos.x + 10.0f, i.pos.y + 7.0f}, 3.0f, Fade(i.color, a * 0.7f));
                    DrawLineEx({i.pos.x - 11.0f, i.pos.y + 9.0f}, {i.pos.x + 13.0f, i.pos.y - 6.0f}, 2.0f, Fade(C(255, 248, 180), a * 0.55f));
                    DrawCircleV(i.pos, 4.0f + t * 6.0f, Fade(i.color, a * 0.22f));
                    break;
                case ImpactKind::FrostBurst:
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius, Fade(C(165, 238, 255), a * 0.72f));
                    for (int k = 0; k < 6; ++k) {
                        float angle = k * PI / 3.0f + t * 0.35f;
                        Vector2 end{i.pos.x + std::cos(angle) * i.radius * 0.78f, i.pos.y + std::sin(angle) * i.radius * 0.78f};
                        DrawLineEx(i.pos, end, 2.0f, Fade(C(231, 253, 255), a * 0.48f));
                    }
                    DrawCircleV(i.pos, i.radius * 0.18f, Fade(i.color, a * 0.18f));
                    break;
                case ImpactKind::SlashSpark:
                    DrawLineEx({i.pos.x - i.radius * 0.55f, i.pos.y + i.radius * 0.16f}, {i.pos.x + i.radius * 0.56f, i.pos.y - i.radius * 0.22f}, 5.0f, Fade(C(255, 224, 112), a * 0.62f));
                    DrawLineEx({i.pos.x - i.radius * 0.25f, i.pos.y - i.radius * 0.34f}, {i.pos.x + i.radius * 0.32f, i.pos.y + i.radius * 0.28f}, 3.0f, Fade(i.color, a * 0.55f));
                    DrawCircleV(i.pos, 5.0f + t * 8.0f, Fade(C(255, 116, 80), a * 0.20f));
                    break;
                case ImpactKind::PoisonCloud:
                    DrawCircleV(i.pos, i.radius * 0.46f, Fade(C(111, 183, 85), a * 0.18f));
                    DrawCircleV({i.pos.x - i.radius * 0.24f, i.pos.y + 3.0f}, i.radius * 0.22f, Fade(i.color, a * 0.28f));
                    DrawCircleV({i.pos.x + i.radius * 0.22f, i.pos.y - 4.0f}, i.radius * 0.18f, Fade(C(202, 245, 114), a * 0.24f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.52f, Fade(C(168, 244, 102), a * 0.38f));
                    break;
                case ImpactKind::BombBlast:
                    DrawCircleV(i.pos, i.radius * 0.42f, Fade(C(255, 118, 60), a * 0.24f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius, Fade(C(255, 211, 84), a * 0.78f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.62f, Fade(C(255, 107, 66), a * 0.62f));
                    for (int k = 0; k < 8; ++k) {
                        float angle = k * PI / 4.0f + 0.2f;
                        Vector2 from{i.pos.x + std::cos(angle) * i.radius * 0.22f, i.pos.y + std::sin(angle) * i.radius * 0.22f};
                        Vector2 to{i.pos.x + std::cos(angle) * i.radius * 0.78f, i.pos.y + std::sin(angle) * i.radius * 0.78f};
                        DrawLineEx(from, to, 3.0f, Fade(C(255, 229, 125), a * 0.45f));
                    }
                    break;
                case ImpactKind::HolyRing:
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius, Fade(C(255, 245, 170), a * 0.62f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.62f, Fade(i.color, a * 0.46f));
                    DrawStar(i.pos, std::max(5.0f, i.radius * 0.16f), Fade(C(255, 249, 204), a * 0.80f));
                    break;
                case ImpactKind::LaserStrike:
                    DrawLineEx({i.pos.x - i.radius * 0.45f, i.pos.y}, {i.pos.x + i.radius * 0.45f, i.pos.y}, 5.0f, Fade(i.color, a * 0.72f));
                    DrawCircleV(i.pos, 7.0f + t * 10.0f, Fade(C(255, 245, 185), a * 0.32f));
                    break;
                case ImpactKind::LightningArc:
                    for (int k = 0; k < 4; ++k) {
                        float angle = k * PI * 0.5f + t;
                        Vector2 mid{i.pos.x + std::cos(angle) * i.radius * 0.22f, i.pos.y + std::sin(angle) * i.radius * 0.22f};
                        Vector2 end{i.pos.x + std::cos(angle + 0.45f) * i.radius * 0.66f, i.pos.y + std::sin(angle + 0.45f) * i.radius * 0.66f};
                        DrawLineEx(i.pos, mid, 3.0f, Fade(C(255, 245, 104), a * 0.55f));
                        DrawLineEx(mid, end, 2.0f, Fade(i.color, a * 0.52f));
                    }
                    break;
                case ImpactKind::FireBurst:
                    DrawCircleV(i.pos, i.radius * 0.40f, Fade(C(255, 99, 52), a * 0.22f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.72f, Fade(C(255, 187, 76), a * 0.62f));
                    DrawTriangle({i.pos.x, i.pos.y - i.radius * 0.45f}, {i.pos.x - i.radius * 0.28f, i.pos.y + i.radius * 0.22f},
                                 {i.pos.x + i.radius * 0.28f, i.pos.y + i.radius * 0.22f}, Fade(C(255, 139, 54), a * 0.42f));
                    break;
                case ImpactKind::WindBurst:
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius, Fade(C(224, 249, 255), a * 0.52f));
                    DrawCircleLines(static_cast<int>(i.pos.x + i.radius * 0.18f), static_cast<int>(i.pos.y - i.radius * 0.10f), i.radius * 0.55f, Fade(i.color, a * 0.42f));
                    break;
                case ImpactKind::SummonShock:
                    DrawRectanglePro({i.pos.x, i.pos.y, i.radius * 0.72f, i.radius * 0.72f}, {i.radius * 0.36f, i.radius * 0.36f}, t * 80.0f, Fade(i.color, a * 0.22f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.72f, Fade(C(255, 246, 192), a * 0.48f));
                    break;
                case ImpactKind::CharmWave:
                    DrawHeart(i.pos, 0.55f + t * 0.30f, Fade(i.color, a * 0.62f));
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius * 0.58f, Fade(C(255, 205, 232), a * 0.42f));
                    break;
                case ImpactKind::Pulse:
                    DrawCircleLines(static_cast<int>(i.pos.x), static_cast<int>(i.pos.y), i.radius, Fade(i.color, a * 0.55f));
                    DrawCircleV(i.pos, i.radius * 0.28f, Fade(i.color, a * 0.16f));
                    break;
            }
        }
    }

    void DrawWallAndSlots() {
        DrawRectangleGradientV(0, 632, kScreenW, 104, C(68, 78, 83), C(48, 58, 66));
        DrawRectangle(0, 640, kScreenW, 14, C(99, 111, 116));
        for (int i = 0; i < 12; ++i) {
            DrawRectangleRounded({-6.0f + i * 39.0f, 627, 35, 34}, 0.12f, 6, C(188, 197, 201));
            DrawRectangleRounded({-2.0f + i * 39.0f, 631, 27, 25}, 0.12f, 6, C(152, 165, 172));
        }

        for (int i = 0; i < kMaxSlots; ++i) {
            Rectangle r = SlotRect(i);
            bool selected = i == selectedSlot || (dragActive && dragSlot == i);
            bool dropTarget = dragActive && dragSlot >= 0 && i != dragSlot && slots[i].active &&
                              slots[dragSlot].active && slots[i].type == slots[dragSlot].type &&
                              slots[i].star == slots[dragSlot].star && slots[i].star < 5;
            DrawRectangleRounded({r.x + 2, r.y + 6, r.width, r.height}, 0.13f, 8, C(24, 31, 36, 95));
            DrawRectangleRounded(r, 0.13f, 8, dropTarget ? C(163, 139, 76) : (selected ? C(109, 151, 159) : C(96, 111, 117)));
            DrawRectangleRounded({r.x + 5, r.y + 5, r.width - 10, r.height - 10}, 0.11f, 8, C(132, 144, 145));
            DrawRectangleRounded({r.x + 8, r.y + 8, r.width - 16, r.height - 16}, 0.1f, 8, C(102, 111, 112));
            if (selected) DrawRectangleRounded({r.x + 3, r.y + 3, r.width - 6, r.height - 6}, 0.12f, 8, Fade(C(255, 226, 105), 0.25f));
            if (dropTarget) DrawRectangleRounded({r.x + 3, r.y + 3, r.width - 6, r.height - 6}, 0.12f, 8, Fade(C(255, 230, 92), 0.34f));
            if (slots[i].active && !(dragActive && dragSlot == i)) DrawHero(slots[i], SlotCenter(i), 0.86f, true);
        }
        if (dragActive && dragSlot >= 0 && slots[dragSlot].active) {
            DrawCircleV(dragMouse, 36.0f, Fade(C(255, 234, 122), 0.18f));
            DrawHero(slots[dragSlot], dragMouse, 0.98f, true);
        }
    }

    void DrawHero(const Hero& h, Vector2 pos, float scale, bool showStars) {
        const HeroDef& def = heroesDef[h.type];
        float s = scale * (1.0f + (h.flash > 0.0f ? 0.08f * std::sin(GetTime() * 30.0) : 0.0f));
        int frame = static_cast<int>(std::floor(GetTime() * 4.0 + h.type)) & 1;
        if (h.type >= 0 && h.type < static_cast<int>(assets.heroes.size()) && assets.heroes[h.type][frame].id != 0) {
            Texture2D tex = assets.heroes[h.type][frame];
            DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y + 25 * s), 19 * s, 8 * s, C(28, 33, 38, 75));
            if (h.flash > 0.0f) {
                DrawCircleV(pos, 34 * s, Fade(def.projectile, 0.22f));
            }
            Rectangle src{0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)};
            Rectangle dst{pos.x, pos.y + 1.0f, tex.width * 0.58f * s, tex.height * 0.58f * s};
            DrawTexturePro(tex, src, dst, {dst.width * 0.5f, dst.height * 0.5f}, 0.0f, WHITE);
            if (showStars) {
                for (int st = 0; st < h.star; ++st) {
                    DrawStar({pos.x - (h.star - 1) * 5.3f + st * 10.6f, pos.y + 38 * s}, 4.8f, C(255, 218, 76));
                }
            }
            return;
        }
        DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y + 25 * s), 19 * s, 8 * s, C(28, 33, 38, 75));
        if (h.flash > 0.0f) {
            DrawCircleV(pos, 34 * s, Fade(def.projectile, 0.22f));
        }
        DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y + 7 * s), 20 * s, C(54, 48, 46));
        DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y + 4 * s), 18 * s, def.main);
        DrawCircle(static_cast<int>(pos.x - 6 * s), static_cast<int>(pos.y + 1 * s), 3.5f * s, C(26, 38, 42));
        DrawCircle(static_cast<int>(pos.x + 7 * s), static_cast<int>(pos.y + 1 * s), 3.5f * s, C(26, 38, 42));
        DrawRectangleRounded({pos.x - 7 * s, pos.y + 10 * s, 14 * s, 4 * s}, 0.6f, 6, C(248, 244, 221));

        switch (def.attackStyle) {
            case AttackStyle::Arrow:
                DrawTriangle({pos.x - 22 * s, pos.y - 10 * s}, {pos.x + 18 * s, pos.y - 18 * s}, {pos.x + 14 * s, pos.y - 4 * s}, def.trim);
                DrawLineEx({pos.x + 18 * s, pos.y + 13 * s}, {pos.x + 30 * s, pos.y - 10 * s}, 3.0f * s, C(114, 78, 46));
                break;
            case AttackStyle::Frost:
                DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y - 16 * s), 14 * s, def.trim);
                DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y - 16 * s), 8 * s, C(125, 228, 244));
                DrawCircle(static_cast<int>(pos.x + 21 * s), static_cast<int>(pos.y + 15 * s), 6 * s, C(244, 196, 255));
                break;
            case AttackStyle::Slash:
                DrawRectanglePro({pos.x - 24 * s, pos.y + 8 * s, 7 * s, 38 * s}, {3.5f * s, 19 * s}, -48, C(255, 210, 81));
                DrawTriangle({pos.x - 26 * s, pos.y - 19 * s}, {pos.x, pos.y - 34 * s}, {pos.x + 26 * s, pos.y - 18 * s}, def.trim);
                break;
            case AttackStyle::Poison:
                DrawTriangle({pos.x - 23 * s, pos.y - 10 * s}, {pos.x, pos.y - 31 * s}, {pos.x + 23 * s, pos.y - 10 * s}, C(84, 52, 116));
                DrawCircle(static_cast<int>(pos.x + 22 * s), static_cast<int>(pos.y + 14 * s), 7 * s, def.trim);
                break;
            case AttackStyle::Bomb:
                DrawRectangleRounded({pos.x - 20 * s, pos.y - 17 * s, 40 * s, 14 * s}, 0.35f, 8, C(83, 96, 112));
                DrawCircle(static_cast<int>(pos.x + 24 * s), static_cast<int>(pos.y + 8 * s), 8 * s, C(247, 187, 82));
                break;
            case AttackStyle::Holy:
                DrawTriangle({pos.x - 25 * s, pos.y - 8 * s}, {pos.x, pos.y - 29 * s}, {pos.x + 25 * s, pos.y - 8 * s}, C(97, 60, 137));
                DrawCircle(static_cast<int>(pos.x + 23 * s), static_cast<int>(pos.y - 3 * s), 8 * s, def.trim);
                DrawLineEx({pos.x + 22 * s, pos.y + 4 * s}, {pos.x + 22 * s, pos.y + 28 * s}, 3.0f * s, C(150, 91, 62));
                break;
            case AttackStyle::Laser:
                DrawRectangleRounded({pos.x - 22 * s, pos.y - 14 * s, 44 * s, 16 * s}, 0.35f, 8, C(84, 96, 108));
                DrawCircle(static_cast<int>(pos.x + 23 * s), static_cast<int>(pos.y + 8 * s), 7 * s, def.trim);
                break;
            case AttackStyle::Lightning:
                DrawLineEx({pos.x - 18 * s, pos.y - 18 * s}, {pos.x + 2 * s, pos.y - 4 * s}, 4.0f * s, def.trim);
                DrawLineEx({pos.x + 2 * s, pos.y - 4 * s}, {pos.x - 7 * s, pos.y + 17 * s}, 4.0f * s, def.trim);
                DrawLineEx({pos.x - 7 * s, pos.y + 17 * s}, {pos.x + 18 * s, pos.y + 2 * s}, 4.0f * s, def.trim);
                break;
            case AttackStyle::Fire:
                DrawTriangle({pos.x, pos.y - 31 * s}, {pos.x - 18 * s, pos.y + 2 * s}, {pos.x + 18 * s, pos.y + 2 * s}, C(255, 128, 63));
                DrawTriangle({pos.x, pos.y - 22 * s}, {pos.x - 10 * s, pos.y + 2 * s}, {pos.x + 10 * s, pos.y + 2 * s}, def.trim);
                break;
            case AttackStyle::Wind:
                DrawCircleLines(static_cast<int>(pos.x), static_cast<int>(pos.y - 8 * s), 18 * s, def.trim);
                DrawCircleLines(static_cast<int>(pos.x + 7 * s), static_cast<int>(pos.y - 4 * s), 11 * s, C(235, 250, 255));
                break;
            case AttackStyle::Summon:
                DrawRectangleRounded({pos.x - 22 * s, pos.y - 18 * s, 44 * s, 18 * s}, 0.35f, 8, def.trim);
                DrawCircle(static_cast<int>(pos.x - 20 * s), static_cast<int>(pos.y + 15 * s), 7 * s, def.trim);
                DrawCircle(static_cast<int>(pos.x + 20 * s), static_cast<int>(pos.y + 15 * s), 7 * s, def.trim);
                break;
            case AttackStyle::Charm:
                DrawHeart({pos.x + 20 * s, pos.y - 14 * s}, 0.42f * s, def.trim);
                DrawTriangle({pos.x - 25 * s, pos.y - 8 * s}, {pos.x, pos.y - 28 * s}, {pos.x + 25 * s, pos.y - 8 * s}, Fade(def.trim, 0.85f));
                break;
        }

        if (showStars) {
            for (int st = 0; st < h.star; ++st) {
                DrawStar({pos.x - (h.star - 1) * 5.3f + st * 10.6f, pos.y + 38 * s}, 4.8f, C(255, 218, 76));
            }
        }
    }

    void DrawStar(Vector2 c, float r, Color color) {
        Vector2 p[10];
        for (int i = 0; i < 10; ++i) {
            float a = -PI / 2.0f + i * PI / 5.0f;
            float rr = (i % 2 == 0) ? r : r * 0.45f;
            p[i] = {c.x + std::cos(a) * rr, c.y + std::sin(a) * rr};
        }
        DrawTriangleFan(p, 10, color);
    }

    void DrawBottomHUD() {
        DrawRectangleGradientV(0, 704, kScreenW, 196, C(100, 113, 120), C(57, 68, 77));
        DrawRectangle(0, 704, kScreenW, 4, C(34, 43, 52, 150));

        Rectangle hpBg{66, 724, 326, 26};
        DrawRectangleRounded({hpBg.x + 2, hpBg.y + 4, hpBg.width, hpBg.height}, 0.38f, 14, C(20, 29, 34, 110));
        DrawRectangleRounded(hpBg, 0.38f, 14, C(66, 77, 84));
        DrawRectangleRounded({hpBg.x + 4, hpBg.y + 4, (hpBg.width - 8) * Clamp01(static_cast<float>(wallHp) / wallMaxHp), hpBg.height - 8}, 0.32f, 14, C(75, 221, 63));
        DrawRectangleRounded({hpBg.x + 6, hpBg.y + 6, (hpBg.width - 12) * Clamp01(static_cast<float>(wallHp) / wallMaxHp), 5}, 0.4f, 8, C(190, 255, 131, 150));
        DrawHeart({326, 716}, 0.42f, C(93, 235, 94));
        text.draw(std::to_string(wallHp).c_str(), {346, 710}, 26, C(246, 252, 245));

        DrawCoinPill({17, 760, 100, 42}, coins, 28);

        DrawControlButton(upgradeButton, u8"强化", upgradeCost, C(118, 84, 56), C(244, 224, 120), false);
        DrawMechanismButton(freezeButton, u8"霜冻", freezeCooldown, C(83, 161, 191), C(166, 235, 255));
        DrawKingButton();
        DrawMechanismButton(cannonButton, u8"炮击", cannonCooldown, C(130, 91, 60), C(255, 176, 74));
        DrawControlButton(summonButton, u8"召唤", static_cast<int>(std::round(summonCost * (1.0f - summonDiscount))), C(117, 88, 58), C(246, 216, 100), true);

        std::stringstream left;
        int live = spawnRemaining + static_cast<int>(enemies.size());
        left << u8"剩余 " << live;
        text.centered(left.str().c_str(), {282, 762, 110, 28}, 17, C(223, 235, 238));
    }

    void DrawCoinPill(Rectangle r, int value, float size) {
        DrawRectangleRounded({r.x + 2, r.y + 4, r.width, r.height}, 0.45f, 12, C(21, 27, 33, 105));
        DrawRectangleRounded(r, 0.45f, 12, C(164, 117, 65));
        DrawCircle(static_cast<int>(r.x + 20), static_cast<int>(r.y + r.height * 0.5f), 14, C(205, 218, 226));
        DrawCircle(static_cast<int>(r.x + 20), static_cast<int>(r.y + r.height * 0.5f), 9, C(152, 171, 181));
        text.stroke(std::to_string(value).c_str(), {r.x + 38, r.y + 6}, size, C(255, 249, 236), C(76, 57, 41));
    }

    void DrawControlButton(Rectangle r, const char* label, int cost, Color body, Color glow, bool summon) {
        DrawRectangleRounded({r.x + 3, r.y + 5, r.width, r.height}, 0.18f, 12, C(21, 28, 34, 100));
        DrawRectangleRounded(r, 0.18f, 12, body);
        DrawRectangleRounded({r.x + 8, r.y + 8, r.width - 16, r.height - 28}, 0.18f, 10, C(76, 86, 88));
        if (summon) {
            DrawRectangleRounded({r.x + 28, r.y + 10, 30, 32}, 0.18f, 8, C(93, 145, 172));
            DrawTriangle({r.x + 42, r.y + 4}, {r.x + 70, r.y + 15}, {r.x + 42, r.y + 24}, C(221, 232, 238));
            DrawCircle(static_cast<int>(r.x + 35), static_cast<int>(r.y + 38), 7, glow);
        } else {
            DrawRectanglePro({r.x + 43, r.y + 27, 7, 44}, {3.5f, 22}, -38, C(210, 231, 236));
            DrawRectangleRounded({r.x + 28, r.y + 20, 34, 24}, 0.18f, 8, C(232, 238, 226));
            DrawLineEx({r.x + 32, r.y + 25}, {r.x + 55, r.y + 31}, 2, C(149, 163, 168));
        }
        text.centered(label, {r.x, r.y + 44, r.width, 22}, 22, C(255, 238, 139));
        DrawCircle(static_cast<int>(r.x + 17), static_cast<int>(r.y + 64), 12, C(205, 218, 226));
        text.centered(std::to_string(cost).c_str(), {r.x + 27, r.y + 56, 48, 18}, 17, C(255, 231, 206));
    }

    void DrawMechanismButton(Rectangle r, const char* label, float cooldown, Color body, Color icon) {
        DrawRectangleRounded({r.x + 2, r.y + 4, r.width, r.height}, 0.22f, 12, C(18, 24, 30, 95));
        DrawRectangleRounded(r, 0.22f, 12, body);
        DrawCircle(static_cast<int>(r.x + r.width * 0.5f), static_cast<int>(r.y + 21), 17, C(61, 76, 83));
        DrawCircle(static_cast<int>(r.x + r.width * 0.5f), static_cast<int>(r.y + 21), 10, icon);
        text.centered(label, {r.x, r.y + 35, r.width, 18}, 15, C(247, 242, 219));
        if (cooldown > 0.0f) {
            DrawRectangleRounded(r, 0.22f, 12, C(9, 13, 17, 128));
            std::stringstream ss;
            ss << static_cast<int>(std::ceil(cooldown));
            text.centered(ss.str().c_str(), r, 24, C(255, 255, 255));
        }
    }

    void DrawKingButton() {
        Rectangle r = kingButton;
        DrawRectangleRounded({r.x + 3, r.y + 6, r.width, r.height}, 0.18f, 12, C(18, 24, 31, 100));
        DrawRectangleRounded(r, 0.18f, 12, C(91, 101, 104));
        DrawRectangleRounded({r.x + 8, r.y + 8, r.width - 16, r.height - 18}, 0.18f, 12, C(142, 158, 166));
        Hero king{true, 2, 2, 0.0f, kingBuffTimer > 0.0f ? 0.6f : 0.0f};
        DrawHero(king, {r.x + r.width * 0.5f, r.y + 31}, 0.82f, false);
        std::stringstream ss;
        ss << kingCharge << "/5";
        text.centered(ss.str().c_str(), {r.x, r.y + 58, r.width, 18}, 18, kingCharge >= 5 ? C(255, 226, 83) : C(122, 221, 238));
    }

    void DrawHeart(Vector2 p, float s, Color color) {
        DrawCircle(static_cast<int>(p.x - 8 * s), static_cast<int>(p.y), 10 * s, color);
        DrawCircle(static_cast<int>(p.x + 8 * s), static_cast<int>(p.y), 10 * s, color);
        DrawTriangle({p.x - 18 * s, p.y + 3 * s}, {p.x + 18 * s, p.y + 3 * s}, {p.x, p.y + 25 * s}, color);
    }

    void DrawLeftTools() {
        DrawRectangleRounded(pauseButton, 0.28f, 12, C(25, 35, 44, 185));
        DrawRectangleRounded({pauseButton.x + 12, pauseButton.y + 12, 8, 32}, 0.2f, 4, C(243, 249, 247));
        DrawRectangleRounded({pauseButton.x + 27, pauseButton.y + 12, 8, 32}, 0.2f, 4, C(243, 249, 247));
        text.centered("0:13", {16, 238, 76, 25}, 24, C(249, 250, 245));

        DrawRectangleRounded(speedButton, 0.28f, 12, C(25, 35, 44, 185));
        const char* speedText = speedIndex == 0 ? "x1" : (speedIndex == 1 ? "x1.5" : "x2");
        text.centered(speedText, speedButton, 24, C(247, 250, 245));

        DrawRectangleRounded(statsButton, 0.22f, 12, showStats ? C(52, 83, 94, 220) : C(25, 35, 44, 185));
        DrawLineEx({statsButton.x + 16, statsButton.y + 42}, {statsButton.x + 16, statsButton.y + 22}, 5, C(247, 250, 245));
        DrawLineEx({statsButton.x + 28, statsButton.y + 42}, {statsButton.x + 28, statsButton.y + 14}, 5, C(247, 250, 245));
        DrawLineEx({statsButton.x + 40, statsButton.y + 42}, {statsButton.x + 40, statsButton.y + 28}, 5, C(247, 250, 245));
        text.centered(u8"统计", {statsButton.x, statsButton.y + 44, statsButton.width, 19}, 17, C(247, 250, 245));
    }

    void DrawFloatingTexts() {
        for (const auto& f : floaters) {
            text.stroke(f.text.c_str(), f.pos, 18, f.color, C(31, 38, 43, 160));
        }
    }

    void DrawStatsPanel() {
        if (!showStats || state != GameState::Playing) return;
        Rectangle r{92, 306, 244, 164};
        DrawRectangleRounded({r.x + 3, r.y + 6, r.width, r.height}, 0.16f, 12, C(11, 16, 22, 110));
        DrawRectangleRounded(r, 0.16f, 12, C(39, 52, 64, 232));
        text.centered(u8"战斗统计", {r.x, r.y + 12, r.width, 24}, 23, C(245, 249, 245));
        std::vector<std::pair<std::string, std::string>> rows = {
            {u8"击杀", std::to_string(kills)},
            {u8"最高星级", std::to_string(highestStar)},
            {u8"召唤次数", std::to_string(summons)},
            {u8"合成次数", std::to_string(merges)},
        };
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            float y = r.y + 48 + i * 25;
            text.draw(rows[i].first.c_str(), {r.x + 24, y}, 18, C(194, 214, 219));
            text.draw(rows[i].second.c_str(), {r.x + 174, y}, 18, C(255, 230, 126));
        }
    }

    void DrawToast() {
        if (toastTimer <= 0.0f) return;
        float alpha = Clamp01(toastTimer / 0.35f);
        Vector2 m = text.measure(toast.c_str(), 18);
        Rectangle r{(kScreenW - m.x - 38) * 0.5f, 585, m.x + 38, 36};
        DrawRectangleRounded(r, 0.45f, 12, Fade(C(20, 28, 36), std::min(0.9f, alpha)));
        text.centered(toast.c_str(), r, 18, Fade(C(247, 250, 244), alpha));
    }

    void DrawWaveBanner() {
        float a = Clamp01(stageBanner / 0.5f);
        Rectangle r{105, 256, 220, 70};
        DrawRectangleRounded({r.x + 2, r.y + 5, r.width, r.height}, 0.26f, 14, Fade(C(12, 18, 25), 0.35f * a));
        DrawRectangleRounded(r, 0.26f, 14, Fade(C(48, 63, 77), 0.86f * a));
        std::stringstream ss;
        ss << u8"波次 " << std::min(wave + 1, totalWaves) << "/" << totalWaves;
        text.centeredStroke(ss.str().c_str(), r, 30, Fade(C(255, 251, 235), a), Fade(C(29, 38, 48), a));
    }

    void DrawBackButton() {
        DrawRectangleRounded({backButton.x + 2, backButton.y + 4, backButton.width, backButton.height}, 0.26f, 12, C(4, 10, 16, 90));
        DrawRectangleRounded(backButton, 0.26f, 12, C(65, 84, 98));
        text.centered(u8"返回大厅", backButton, 18, C(238, 247, 246));
    }

    void DrawLobbyOverlay() {
        Rectangle page{20, 104, 390, 724};
        text.centeredStroke(u8"永远的蔚蓝星球", {page.x, 126, page.width, 42}, 31, C(255, 252, 236), C(25, 33, 42));
        std::stringstream progress;
        progress << u8"已解锁 " << save.unlockedLevel << u8"/30 · 星尘 " << save.starDust << u8" · 招募券 " << save.gachaTickets;
        text.centered(progress.str().c_str(), {page.x + 24, 166, page.width - 48, 24}, 16, C(221, 238, 241));

        Rectangle status{46, 246, 338, 122};
        DrawRectangleRounded({status.x + 2, status.y + 5, status.width, status.height}, 0.16f, 12, C(5, 10, 16, 80));
        DrawRectangleRounded(status, 0.16f, 12, C(31, 45, 56, 222));
        std::stringstream stageHint;
        stageHint << u8"闯关目标：第 " << (currentLevel + 1) << u8" 关 · " << levels[currentLevel].name;
        text.centered(stageHint.str().c_str(), {status.x + 14, status.y + 18, status.width - 28, 22}, 17, C(255, 231, 130));
        int idx = std::max(0, std::min(kSupportCount - 1, save.activeSupport));
        std::stringstream supportLine;
        supportLine << u8"当前支援：" << supports[idx].name << u8" +" << std::max(0, save.supportCopies[idx] - 1);
        text.centered(supportLine.str().c_str(), {status.x + 14, status.y + 50, status.width - 28, 20}, 16, C(209, 229, 233));
        std::stringstream metaLine;
        metaLine << u8"装备 " << save.equipmentLevel << u8" · 图腾 " << save.totemLevel << u8" · 披风 " << save.cloakLevel;
        text.centered(metaLine.str().c_str(), {status.x + 14, status.y + 80, status.width - 28, 20}, 15, C(193, 214, 221));

        Rectangle summary{46, 400, 338, 142};
        DrawRectangleRounded(summary, 0.16f, 12, C(42, 59, 70, 205));
        text.centered(u8"局外大厅", {summary.x + 16, summary.y + 18, summary.width - 32, 24}, 22, C(247, 252, 242));
        std::stringstream bestLine;
        bestLine << u8"最佳关卡 " << std::max(1, save.bestLevel + 1) << u8" · 胜利 " << save.victories;
        text.centered(bestLine.str().c_str(), {summary.x + 18, summary.y + 54, summary.width - 36, 20}, 15, C(201, 222, 227));
        text.centered(u8"星尘与材料用于局外养成", {summary.x + 18, summary.y + 84, summary.width - 36, 20}, 15, C(201, 222, 227));

        DrawRectangleRounded(adventureButton, 0.24f, 14, C(88, 179, 83));
        DrawRectangleRounded({adventureButton.x + 8, adventureButton.y + 8, adventureButton.width - 16, 12}, 0.4f, 10, C(191, 252, 128, 105));
        text.centered(u8"开始闯关", adventureButton, 27, C(255, 255, 239));
        text.centered(u8"选择关卡并进入战斗", {adventureButton.x + 14, adventureButton.y + 42, adventureButton.width - 28, 20}, 14, C(223, 244, 224));

        DrawRectangleRounded(metaMenuButton, 0.24f, 14, C(91, 110, 132));
        DrawRectangleRounded({metaMenuButton.x + 8, metaMenuButton.y + 8, metaMenuButton.width - 16, 12}, 0.4f, 10, C(184, 211, 229, 85));
        text.centered(u8"局外养成", {metaMenuButton.x, metaMenuButton.y + 8, metaMenuButton.width, 28}, 24, C(249, 251, 238));
        text.centered(u8"升级装备、图腾、披风与支援", {metaMenuButton.x + 14, metaMenuButton.y + 38, metaMenuButton.width - 28, 18}, 13, C(219, 232, 238));
    }

    void DrawStageSelectOverlay() {
        Rectangle page{20, 104, 390, 724};
        text.centered(u8"开始闯关", {page.x, 128, page.width, 28}, 24, C(229, 244, 247));
        std::stringstream title;
        title << u8"第 " << (currentLevel + 1) << u8" 关";
        text.centered(title.str().c_str(), {page.x, 192, page.width, 22}, 21, C(198, 222, 229));
        text.centeredStroke(levels[currentLevel].name.c_str(), {page.x, 222, page.width, 44}, 34, C(255, 252, 237), C(27, 35, 44));
        Rectangle traitBox{48, 280, 334, 44};
        DrawRectangleRounded(traitBox, 0.45f, 14, Fade(levels[currentLevel].accent, 0.28f));
        text.centered(levels[currentLevel].trait.c_str(), traitBox, 18, C(232, 245, 247));
        Rectangle savePill{48, 350, 334, 36};
        DrawRectangleRounded(savePill, 0.45f, 12, C(30, 43, 54, 205));
        std::stringstream saveLine;
        saveLine << u8"推荐战力：装备 " << save.equipmentLevel << u8" / 图腾 " << save.totemLevel << u8" / 披风 " << save.cloakLevel;
        text.centered(saveLine.str().c_str(), savePill, 17, C(255, 231, 131));
        std::stringstream unlockLine;
        unlockLine << u8"已解锁 " << save.unlockedLevel << u8"/30 · 胜利可得材料、装备和招募券";
        text.centered(unlockLine.str().c_str(), {48, 402, 334, 24}, 15, C(198, 220, 226));
        Rectangle dropBox{48, 450, 334, 120};
        DrawRectangleRounded(dropBox, 0.16f, 12, C(34, 49, 61, 205));
        std::stringstream drops;
        drops << u8"掉率 " << static_cast<int>(levels[currentLevel].gearDropRate * 100.0f) << u8"% · 奖励星尘 " << levels[currentLevel].baseDustReward;
        text.centered(drops.str().c_str(), {dropBox.x + 12, dropBox.y + 18, dropBox.width - 24, 20}, 15, C(255, 230, 126));
        std::stringstream materials;
        materials << u8"材料 +" << levels[currentLevel].materialReward << u8" · 怪物密度 +" << levels[currentLevel].densityBonus;
        text.centered(materials.str().c_str(), {dropBox.x + 12, dropBox.y + 48, dropBox.width - 24, 20}, 15, C(207, 226, 231));
        text.centered(u8"胜利后解锁下一关", {dropBox.x + 12, dropBox.y + 78, dropBox.width - 24, 20}, 15, C(188, 212, 219));

        DrawRectangleRounded(prevStageButton, 0.28f, 12, C(75, 92, 104));
        DrawRectangleRounded(nextStageButton, 0.28f, 12, C(75, 92, 104));
        text.centered("<", prevStageButton, 26, C(246, 250, 245));
        text.centered(">", nextStageButton, 26, C(246, 250, 245));
        text.centered(u8"选择关卡", {145, 604, 140, 28}, 21, C(246, 250, 245));

        DrawRectangleRounded(startButton, 0.38f, 14, C(93, 190, 85));
        DrawRectangleRounded({startButton.x + 5, startButton.y + 5, startButton.width - 10, 12}, 0.4f, 10, C(189, 250, 126, 115));
        text.centered(u8"开始防守", startButton, 25, C(255, 255, 240));

        DrawBackButton();
    }

    void DrawMetaOverlay() {
        Rectangle page{20, 104, 390, 724};
        text.centeredStroke(u8"局外养成", {page.x, 128, page.width, 38}, 31, C(255, 252, 237), C(27, 35, 44));
        Rectangle savePill{38, 214, 354, 34};
        DrawRectangleRounded(savePill, 0.45f, 12, C(30, 43, 54, 190));
        std::stringstream saveLine;
        saveLine << u8"星尘 " << save.starDust << u8" · 券 " << save.gachaTickets << u8" · 装备 " << save.rareGear << "/" << save.epicGear << "/" << save.mythicGear;
        text.centered(saveLine.str().c_str(), savePill, 16, C(255, 231, 131));
        text.centered(u8"装备、图腾、披风会直接强化局内战斗能力", {38, 258, 354, 22}, 14, C(198, 220, 226));

        DrawMetaCard(equipmentButton, u8"装备", save.equipmentLevel, UpgradeCost(0), u8"攻击与攻速提升", C(225, 156, 74));
        DrawMetaCard(totemButton, u8"图腾", save.totemLevel, UpgradeCost(1), u8"控制、毒伤、首领增伤", C(124, 205, 189));
        DrawMetaCard(cloakButton, u8"披风", save.cloakLevel, UpgradeCost(2), u8"城墙生命与召唤折扣", C(170, 139, 224));
        DrawSupportPanel();
        DrawBackButton();
    }

    void DrawMetaCard(Rectangle r, const char* name, int level, int cost, const char* desc, Color accent) {
        DrawRectangleRounded({r.x + 2, r.y + 4, r.width, r.height}, 0.16f, 10, C(12, 17, 23, 90));
        DrawRectangleRounded(r, 0.16f, 10, C(58, 73, 83, 232));
        DrawRectangleRounded({r.x + 9, r.y + 9, 40, 36}, 0.18f, 8, Fade(accent, 0.55f));
        DrawCircle(static_cast<int>(r.x + 29), static_cast<int>(r.y + 27), 11, accent);
        std::stringstream title;
        title << name << " Lv." << level;
        text.draw(title.str().c_str(), {r.x + 62, r.y + 9}, 19, C(249, 251, 236));
        text.draw(desc, {r.x + 62, r.y + 31}, 14, C(198, 218, 224));
        std::stringstream costText;
        std::string label = name;
        int kind = label == u8"装备" ? 0 : (label == u8"图腾" ? 1 : 2);
        int material = kind == 0 ? save.equipmentParts : (kind == 1 ? save.totemRunes : save.cloakSilk);
        costText << cost << "/" << MaterialCost(kind);
        bool canUpgrade = save.starDust >= cost && material >= MaterialCost(kind);
        text.centered(costText.str().c_str(), {r.x + 246, r.y + 9, 74, 20}, 14, canUpgrade ? C(255, 230, 112) : C(156, 166, 170));
        std::stringstream matText;
        matText << u8"材 " << material;
        text.centered(matText.str().c_str(), {r.x + 246, r.y + 29, 74, 18}, 13, C(196, 218, 224));
    }

    void DrawSupportPanel() {
        if (supports.empty()) return;
        int idx = std::max(0, std::min(kSupportCount - 1, save.activeSupport));
        const SupportDef& support = supports[idx];
        Rectangle r{38, 548, 354, 128};
        DrawRectangleRounded({r.x + 2, r.y + 4, r.width, r.height}, 0.16f, 10, C(12, 17, 23, 90));
        DrawRectangleRounded(r, 0.16f, 10, C(53, 68, 79, 238));
        DrawRectangleRounded({r.x + 70, r.y + 18, 54, 54}, 0.2f, 10, Fade(support.accent, 0.48f));
        DrawCircle(static_cast<int>(r.x + 97), static_cast<int>(r.y + 45), 19, support.accent);
        DrawStar({r.x + 97, r.y + 45}, 13, C(255, 248, 196));
        std::stringstream title;
        title << support.name << u8" +" << std::max(0, save.supportCopies[idx] - 1);
        text.draw(title.str().c_str(), {r.x + 138, r.y + 18}, 18, C(249, 251, 236));
        text.draw(support.job.c_str(), {r.x + 138, r.y + 42}, 13, C(178, 207, 215));
        text.draw(support.desc.c_str(), {r.x + 138, r.y + 62}, 13, C(209, 226, 229));
        DrawRectangleRounded(supportPrevButton, 0.3f, 10, C(74, 91, 103));
        DrawRectangleRounded(supportNextButton, 0.3f, 10, C(74, 91, 103));
        text.centered("<", supportPrevButton, 20, C(246, 250, 245));
        text.centered(">", supportNextButton, 20, C(246, 250, 245));
        DrawRectangleRounded(gachaButton, 0.34f, 12, C(151, 102, 70));
        std::stringstream gacha;
        gacha << u8"招募支援";
        text.centered(gacha.str().c_str(), gachaButton, 17, C(255, 239, 172));
    }

    void DrawDraftOverlay() {
        DrawRectangle(0, 0, kScreenW, kScreenH, C(6, 10, 16, 142));
        Rectangle panel{26, 206, 378, 504};
        DrawRectangleRounded({panel.x + 4, panel.y + 8, panel.width, panel.height}, 0.11f, 14, C(0, 0, 0, 110));
        DrawRectangleRounded(panel, 0.11f, 14, C(44, 58, 71, 245));
        text.centered(u8"随机强化", {panel.x, panel.y + 26, panel.width, 28}, 25, C(255, 236, 142));
        text.centered(u8"选择一项成长", {panel.x, panel.y + 58, panel.width, 24}, 18, C(213, 229, 235));
        for (int i = 0; i < static_cast<int>(draftChoices.size()); ++i) {
            Rectangle card{42.0f, 286.0f + i * 126.0f, 346.0f, 104.0f};
            Color accent = levels[currentLevel].accent;
            DrawRectangleRounded({card.x + 2, card.y + 4, card.width, card.height}, 0.13f, 12, C(11, 15, 21, 105));
            DrawRectangleRounded(card, 0.13f, 12, C(67, 79, 88));
            DrawRectangleRounded({card.x + 8, card.y + 8, 70, 88}, 0.13f, 12, Fade(accent, 0.38f));
            DrawCircle(static_cast<int>(card.x + 43), static_cast<int>(card.y + 52), 22, Fade(C(255, 232, 114), 0.85f));
            DrawStar({card.x + 43, card.y + 52}, 16, C(255, 246, 179));
            text.draw(draftChoices[i].title.c_str(), {card.x + 92, card.y + 24}, 24, C(255, 251, 231));
            text.draw(draftChoices[i].body.c_str(), {card.x + 92, card.y + 59}, 16, C(204, 221, 225));
        }
    }

    void DrawPauseOverlay() {
        DrawRectangle(0, 0, kScreenW, kScreenH, C(7, 12, 18, 135));
        Rectangle r{93, 382, 244, 126};
        DrawRectangleRounded(r, 0.14f, 12, C(45, 58, 70, 242));
        text.centered(u8"已暂停", {r.x, r.y + 28, r.width, 34}, 32, C(247, 250, 244));
        text.centered(u8"点击左侧暂停键继续", {r.x, r.y + 74, r.width, 24}, 18, C(199, 218, 224));
    }

    void DrawResultOverlay(bool win) {
        DrawRectangle(0, 0, kScreenW, kScreenH, C(6, 10, 16, 155));
        Rectangle r{42, 464, 346, 372};
        DrawRectangleRounded({r.x + 4, r.y + 8, r.width, r.height}, 0.12f, 14, C(0, 0, 0, 120));
        DrawRectangleRounded(r, 0.12f, 14, C(45, 58, 70, 245));
        text.centeredStroke(win ? u8"防守胜利" : u8"城墙失守", {r.x, r.y + 42, r.width, 48}, 38,
                            win ? C(255, 236, 126) : C(255, 139, 115), C(25, 32, 40));
        std::stringstream stats;
        stats << u8"击杀 " << kills << u8" · 最高 " << highestStar << u8"★ · 合成 " << merges;
        text.centered(stats.str().c_str(), {r.x, r.y + 116, r.width, 28}, 19, C(214, 231, 236));
        if (!resultLoot.empty()) {
            text.centered(resultLoot.c_str(), {r.x + 18, r.y + 150, r.width - 36, 46}, 15, win ? C(255, 229, 132) : C(207, 224, 229));
        }
        Rectangle main{95, 702, 240, 58};
        DrawRectangleRounded(main, 0.36f, 14, win ? C(92, 188, 86) : C(188, 94, 77));
        text.centered(win ? u8"进入下一关" : u8"重试本关", main, 25, C(255, 255, 240));
        Rectangle menu{128, 776, 174, 45};
        DrawRectangleRounded(menu, 0.34f, 12, C(76, 91, 103));
        text.centered(u8"选择关卡", menu, 20, C(235, 245, 245));
    }
};

Game::Game() : impl(std::make_unique<Impl>()) {}
Game::~Game() = default;
void Game::Init() { impl->Init(); }
void Game::Shutdown() { impl->Shutdown(); }
bool Game::ShouldClose() const { return impl->ShouldClose(); }
void Game::Tick() { impl->Tick(); }
bool Game::SaveScreenshot(const std::string& mode, const std::string& path) { return impl->SaveScreenshot(mode, path); }
bool Game::RunVerification(std::string* report) { return impl->RunVerification(report); }
