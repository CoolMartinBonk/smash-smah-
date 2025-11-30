#include <SDL.h>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <iostream>
#include <ctime>

const float PI = 3.14159265359f;
int WINDOW_WIDTH = 1024;
int WINDOW_HEIGHT = 768;

struct Vec2 {
    float x, y;
};

struct Color {
    Uint8 r, g, b, a;
};

const Color COL_BG_DARK = { 26, 32, 44, 255 };
const Color COL_RED_500 = { 239, 68, 68, 255 };
const Color COL_YELLOW_400 = { 250, 204, 21, 255 };
const Color COL_BLUE_500 = { 59, 130, 246, 255 };
const Color COL_WHITE = { 255, 255, 255, 255 };
const Color COL_ORANGE = { 249, 115, 22, 255 };
const Color COL_PURPLE = { 147, 51, 234, 255 };
const Color COL_SKIN = { 190, 140, 100, 255 };
const int WINDUP_FRAMES = 8;
const float SMASH_SPEED = 0.6f;

int calculateFlightFrames(float speed, float threshold = 0.05f) {
    //if (speed >= 1.0f) return 1;
    //if (speed <= 0.0f) return 999;

    return (int)std::ceil(std::log(threshold) / std::log(1.0f - speed));
}

float randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

float dist(Vec2 a, Vec2 b) {
    return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2));
}

class Renderer {
public:
    SDL_Renderer* renderer;
    float camZoom = 1.0f;
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    int screenW, screenH;

    Renderer(SDL_Renderer* r, int w, int h) : renderer(r), screenW(w), screenH(h) {}

    Vec2 transform(float x, float y) {
        float cx = screenW / 2.0f;
        float cy = screenH / 2.0f;
        // Zoom around center
        float tx = (x - cx) * camZoom + cx;
        float ty = (y - cy) * camZoom + cy;
        return { tx + shakeX, ty + shakeY };
    }

    void setColor(Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    }

    void fillCircle(float x, float y, float radius, Color c) {
        Vec2 center = transform(x, y);
        float r = radius * camZoom;

        const int segments = 30;
        SDL_Vertex vertices[segments + 1];

        vertices[0].position = { center.x, center.y };
        vertices[0].color = { c.r, c.g, c.b, c.a };
        vertices[0].tex_coord = { 0, 0 };

        for (int i = 0; i < segments; i++) {
            float angle = 2.0f * PI * i / segments;
            vertices[i + 1].position = {
                center.x + std::cos(angle) * r,
                center.y + std::sin(angle) * r
            };
            vertices[i + 1].color = { c.r, c.g, c.b, c.a };
            vertices[i + 1].tex_coord = { 0, 0 };
        }

        int indices[segments * 3];
        for (int i = 0; i < segments; i++) {
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = (i == segments - 1) ? 1 : i + 2;
        }

        SDL_RenderGeometry(renderer, NULL, vertices, segments + 1, indices, segments * 3);
    }

    void drawThickLine(float x1, float y1, float x2, float y2, float width, Color c) {
        Vec2 p1 = transform(x1, y1);
        Vec2 p2 = transform(x2, y2);
        float w = width * camZoom * 0.5f;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len == 0) return;

        float nx = -dy / len * w;
        float ny = dx / len * w;

        SDL_Vertex v[4];

        for (int i = 0; i < 4; i++) v[i].color = { c.r, c.g, c.b, c.a };

        v[0].position = { p1.x + nx, p1.y + ny };
        v[1].position = { p1.x - nx, p1.y - ny };
        v[2].position = { p2.x - nx, p2.y - ny };
        v[3].position = { p2.x + nx, p2.y + ny };

        int indices[] = { 0, 1, 2, 0, 2, 3 };
        SDL_RenderGeometry(renderer, NULL, v, 4, indices, 6);
    }

    void drawQuadraticBezier(Vec2 start, Vec2 control, Vec2 end, float width, Color c) {
        int segments = 30;
        Vec2 prev = start;
        for (int i = 1; i <= segments; i++) {
            float t = (float)i / segments;
            float invT = 1.0f - t;
            // B(t) = (1-t)^2 * P0 + 2(1-t)t * P1 + t^2 * P2
            float x = invT * invT * start.x + 2 * invT * t * control.x + t * t * end.x;
            float y = invT * invT * start.y + 2 * invT * t * control.y + t * t * end.y;
            Vec2 curr = { x, y };
            drawThickLine(prev.x, prev.y, curr.x, curr.y, width, c);
            prev = curr;
        }
    }

    void drawPolygon(float x, float y, const std::vector<Vec2>& points, float rotation, float scale, Color c) {
        Vec2 center = transform(x, y);
        float s = scale * camZoom;

        std::vector<SDL_Vertex> verts;
        verts.push_back({ {center.x, center.y}, {c.r, c.g, c.b, c.a}, {0,0} });

        for (const auto& pt : points) {
            float rx = pt.x * std::cos(rotation) - pt.y * std::sin(rotation);
            float ry = pt.x * std::sin(rotation) + pt.y * std::cos(rotation);
            verts.push_back({ {center.x + rx * s, center.y + ry * s}, {c.r, c.g, c.b, c.a}, {0,0} });
        }

        int n = points.size();
        std::vector<int> indices;
        for (int i = 0; i < n; i++) {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back((i == n - 1) ? 1 : i + 2);
        }
        SDL_RenderGeometry(renderer, NULL, verts.data(), verts.size(), indices.data(), indices.size());
    }

    void drawNumber(int number, float x, float y, float size, Color c) {
        std::string s = std::to_string(number);
        float currX = x;
        for (char ch : s) {
            float w = size *0.6f;
            float h = size;

            auto line = [&](float x1, float y1, float x2, float y2) {
                drawThickLine(currX + x1 * w, y + y1 * h, currX + x2 * w, y + y2 * h, 3, c);
            };

            if (strchr("02356789", ch)) line(0, 0, 1, 0); // Top
            if (strchr("2345689", ch))  line(0, 0.5, 1, 0.5); // Mid
            if (strchr("0235689", ch))  line(0, 1, 1, 1); // Bot
            if (strchr("045689", ch))   line(0, 0, 0, 0.5); // TopLeft
            if (strchr("01234789", ch)) line(1, 0, 1, 0.5); // TopRight
            if (strchr("0268", ch))     line(0, 0.5, 0, 1); // BotLeft
            if (strchr("013456789", ch))line(1, 0.5, 1, 1); // BotRight

            currX += w + 10;
        }
    }

    void drawText(std::string text, float x, float y, float size, Color c) {

    }
};

enum EnemyType { CRATE, SPIKE, HEX };

struct Enemy {
    float x, y;
    float size;
    float speed;
    float vx;
    float swayOffset, swaySpeed, swayAmplitude;
    float rotation, rotSpeed;
    EnemyType type;
    Color color;
    bool active;
};

struct Particle {
    float x, y;
    float vx, vy;
    float life; // 0.0 - 1.0
    float size;
    Color color;
    float decay;
    std::vector<Vec2> lightningPath;
    int type; // 0: Normal, 1: Lightning, 2: Debris, 3: Spark
    float w, h; // For Debris
    float rotation, vRot;
};

struct FloatingText {
    float x, y;
    int value; // Simplified to just numbers for this renderer
    float vy;
    float life;
    Color color;
};

struct Explosion {
    float x, y;
    float life;
    float rotation;
    float scale;
};

struct Shockwave {
    float x, y;
    float radius;
    float maxRadius;
    float alpha;
    float width;
    Color color;
};


enum GameState { MENU, PLAYING, GAME_OVER };
GameState gameState = MENU;
int score = 0;
float health = 100.0f;
int level = 1;
long frames = 0;
float shakeIntensity = 0;
float flashIntensity = 0;
float camZoom = 1.0f;

struct Player {
    float x, y;
    float width = 40, height = 60;
    Color color = COL_BLUE_500;
} player;

struct Mouse {
    int x, y;
    bool clicked;
} mouse;
SDL_Texture* playerTexture = nullptr;
Vec2 leftArm = { 0,0 }, rightArm = { 0,0 };
enum PunchState { IDLE, WINDUP, SMASH, HOLD, RECOVER };
PunchState punchState = IDLE;
int punchTimer = 0;
Vec2 punchTarget = { 0,0 };
Enemy* lockedEnemy = nullptr;
int hitStop = 0;
bool hasSmashImpacted = false;

std::vector<Enemy> enemies;
std::vector<Particle> particles;
std::vector<Shockwave> shockwaves;
std::vector<Explosion> explosions;
std::vector<FloatingText> floatingTexts;

void initGame() {
    score = 0;
    health = 100;
    level = 1;
    enemies.clear();
    particles.clear();
    shockwaves.clear();
    explosions.clear();
    floatingTexts.clear();
    player.x = WINDOW_WIDTH / 2.0f;
    player.y = WINDOW_HEIGHT - 100.0f;
    punchState = IDLE;
    frames = 0;
}

void spawnEnemy() {
    float size = randomFloat(30, 70);
    int typeRoll = rand() % 3;
    EnemyType type = (EnemyType)typeRoll;
    Color c;
    if (type == CRATE) c = { 217, 119, 6, 255 }; // Wood
    else if (type == SPIKE) c = { 239, 68, 68, 255 }; // Red
    else c = { 139, 92, 246, 255 }; // Purple

    float speedBonus = std::min(5.0f, score / 3000.0f);

    Enemy e;
    e.x = randomFloat(size, WINDOW_WIDTH - size);
    e.y = -size;
    e.size = size;
    e.speed = randomFloat(2, 4) + speedBonus;
    e.vx = (randomFloat(0, 1) - 0.5f) * 4.0f;
    e.swayOffset = randomFloat(0, PI * 2);
    e.swaySpeed = 0.05f + randomFloat(0, 0.05f);
    e.rotation = 0;
    e.rotSpeed = (randomFloat(0, 1) - 0.5f) * 0.1f;
    e.swayAmplitude = 7.0f;
    e.type = type;
    e.color = c;
    e.active = true;
    enemies.push_back(e);
}

void createParticles(float x, float y, Color c, int count, float scale = 1.0f) {
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, PI * 2);
        float speed = randomFloat(1, 4) * scale;
        p.x = x; p.y = y;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.life = 1.0f;
        p.color = c;
        p.size = randomFloat(2, 7);
        p.decay = 0.03f;
        p.type = 0; // Normal
        particles.push_back(p);
    }
}

void createDebris(float x, float y, Color c, int count, float scale) {
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, PI * 2);
        float force = randomFloat(5, 15) * scale;
        p.x = x; p.y = y;
        p.vx = std::cos(angle) * force;
        p.vy = std::sin(angle) * force;
        p.rotation = randomFloat(0, PI);
        p.vRot = (randomFloat(0, 1) - 0.5f) * 0.8f;
        p.life = 1.0f;
        p.color = c;
        p.w = randomFloat(4, 16) * scale;
        p.h = randomFloat(4, 16) * scale;
        p.type = 2; // Debris
        particles.push_back(p);
    }
}

void triggerPunch() {
    punchState = WINDUP;
    punchTimer = 0;
    hasSmashImpacted = false;
    lockedEnemy = nullptr;
    float closestDist = 9999.0f;

    for (auto& e : enemies) {
        if (!e.active) continue;
        float d = dist({ (float)mouse.x, (float)mouse.y }, { e.x, e.y });
        if (d < e.size + 100 && d < closestDist) {
            closestDist = d;
            lockedEnemy = &e;
        }
    }

    if (lockedEnemy) {

        int flightFrames = calculateFlightFrames(SMASH_SPEED);
        int totalPredictionFrames = WINDUP_FRAMES + flightFrames;

        float simX = lockedEnemy->x;
        float simY = lockedEnemy->y;
        float simVx = lockedEnemy->vx;

        for (int i = 1; i <= totalPredictionFrames; i++) {
            long simFrame = frames + i;

            simY += lockedEnemy->speed;

            float simWind = std::sin(simFrame * lockedEnemy->swaySpeed + lockedEnemy->swayOffset) * lockedEnemy->swayAmplitude;

            simX += simVx + simWind;

            float margin = lockedEnemy->size / 2.0f;
            if (simX < margin) {
                simX = margin;
                simVx *= -1;
            }
            else if (simX > WINDOW_WIDTH - margin) {
                simX = WINDOW_WIDTH - margin;
                simVx *= -1;
            }
        }
        punchTarget.x = simX;
        punchTarget.y = simY;
    }
    else {
        punchTarget.x = (float)mouse.x;
        punchTarget.y = (float)mouse.y;
    }
}

void triggerImpact(float x, float y, float scale) {
    // Level specific effects
    if (level == 1) {
        shakeIntensity = 10;
    }
    else if (level == 2) {
        shockwaves.push_back({ x, y, 20, 15, 1.0f, 30, COL_ORANGE });
        createParticles(x, y, COL_ORANGE, 20);
        shakeIntensity = 25;
    }
    else if (level >= 3) {
        shockwaves.push_back({ x, y, 30, 25, 1.0f, 10, COL_YELLOW_400 });
        // Sparks
        for (int i = 0; i < 10; i++) {
            Particle p;
            float angle = randomFloat(0, PI * 2);
            float spd = randomFloat(10, 25);
            p.x = x; p.y = y;
            p.vx = std::cos(angle) * spd; p.vy = std::sin(angle) * spd;
            p.life = 1.0f; p.type = 3; p.color = COL_WHITE;
            p.w = 3 * scale; // width
            particles.push_back(p);
        }
        shakeIntensity = 40;
        camZoom = 1.4f;
        if (level == 4) {
            flashIntensity = 0.8f;
            camZoom = 1.6f;
            shockwaves.push_back({ x, y, 10, 40, 1.0f, 20, COL_PURPLE });
        }
    }
}

void checkCollision(float x1, float x2, float y) {
    float centerX = (x1 + x2) / 2;
    float distBetweenGloves = std::abs(x2 - x1);
    float scale = 1.0f + (level - 1) * 0.5f;
    float gloveReach = 44 * scale;

    if (distBetweenGloves <= gloveReach * 2 ) {
        if (!hasSmashImpacted) {
            triggerImpact(centerX, y, scale);
            hasSmashImpacted = true;
        }
    }

    int hitCount = 0;
    for (auto& e : enemies) {
        if (!e.active) continue;
        float killWidth = 44 * scale;
        // AABB Collision
        if (e.x > centerX - killWidth && e.x < centerX + killWidth &&
            e.y > y - 80 * scale && e.y < y + 80 * scale) {

            e.active = false;
            int pts = (int)(e.size * scale * 2);
            score += pts;
            hitCount++;

            createDebris(e.x, e.y, e.color, 8 + level * 4, scale);
            floatingTexts.push_back({ e.x, e.y, pts, -2.0f, 1.0f, COL_YELLOW_400 });

            if (level >= 2) hitStop = 4;
        }
    }

    if (hitCount > 0) {
        shakeIntensity += 5 * hitCount;
    }
}

void update() {
    if (gameState != PLAYING) return;
    if (hitStop > 0) { hitStop--; return; }

    frames++;

    if (shakeIntensity > 0) shakeIntensity *= 0.85f;
    if (shakeIntensity < 0.5f) shakeIntensity = 0;
    if (flashIntensity > 0) flashIntensity -= 0.1f;
    if (camZoom > 1.0f) camZoom -= 0.05f;
    if (camZoom < 1.0f) camZoom = 1.0f;

    int newLevel = std::min(4, (score / 1000) + 1);
    if (newLevel > level) {
        level = newLevel;
        shakeIntensity = 30;
        flashIntensity = 0.5f;
    }

    player.x = (float)mouse.x;
    if (player.x < 20) player.x = 20;
    if (player.x > WINDOW_WIDTH - 20) player.x = WINDOW_WIDTH - 20;

    int spawnRate = std::max(10, 60 - (score / 100));
    if (frames % spawnRate == 0) spawnEnemy();

    for (auto& e : enemies) {
        if (!e.active) continue;

        e.y += e.speed;
        float currentWind = std::sin(frames * e.swaySpeed + e.swayOffset) * e.swayAmplitude;
        e.x += e.vx + currentWind;

        if (e.x < e.size / 2) {
            e.x = e.size / 2;
            e.vx *= -1;
        }
        if (e.x > WINDOW_WIDTH - e.size / 2) {
            e.x = WINDOW_WIDTH - e.size / 2;
            e.vx *= -1;
        }

        e.rotation += e.rotSpeed;

        if (dist({ e.x, e.y }, { player.x, player.y - player.height }) < e.size / 2 + player.width / 2) {
            e.active = false;
            health -= 20;
            createDebris(e.x, e.y, e.color, 10, 1.0f);
            shakeIntensity = 15;
            flashIntensity = 0.4f;
            if (health <= 0) gameState = GAME_OVER;
        }

        bool canBlock = (punchState == IDLE);

        if (canBlock) {
            float currentScale = 1.0f + (level - 1) * 0.5f;
            float gloveRadius = 30.0f * currentScale;

            bool hitLeft = dist({ e.x, e.y }, { leftArm.x, leftArm.y }) < (e.size / 2 + gloveRadius);
            bool hitRight = dist({ e.x, e.y }, { rightArm.x, rightArm.y }) < (e.size / 2 + gloveRadius);

            if (hitLeft || hitRight) {
                e.active = false;
                createParticles(e.x, e.y, { 220, 220, 220, 255 }, 10, 1.2f);
                shakeIntensity = 5;
                score += 10;
                continue;
            }
        }
        if (e.y > WINDOW_HEIGHT + e.size / 2) e.active = false;
    }

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.active; }), enemies.end());

    for (auto& p : particles) {
        if (p.type == 2) {
            p.x += p.vx; p.y += p.vy;
            p.rotation += p.vRot;
            p.vy += 0.4f;
            p.life -= 0.015f;
            if (p.y > WINDOW_HEIGHT || p.y < 0 || p.x > WINDOW_WIDTH || p.x < 0) { p.life = 0; }
        }
        else if (p.type == 3) { // Spark
            p.x += p.vx; p.y += p.vy;
            p.life -= 0.05f;
        }
        else {
            p.x += p.vx; p.y += p.vy;
            p.life -= p.decay;
        }
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0; }), particles.end());

    // Update Shockwaves
    for (auto& s : shockwaves) {
        s.radius += s.maxRadius; // actually using maxRadius as speed storage here
        s.width *= 0.8f;
        s.alpha -= 0.05f;
    }
    shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(), [](const Shockwave& s) { return s.alpha <= 0; }), shockwaves.end());

    // Update Texts
    for (auto& t : floatingTexts) {
        t.y += t.vy;
        t.vy *= 0.9f;
        t.life -= 0.02f;
    }
    floatingTexts.erase(std::remove_if(floatingTexts.begin(), floatingTexts.end(), [](const FloatingText& t) { return t.life <= 0; }), floatingTexts.end());

    Vec2 shoulderL = { player.x - 20, player.y - 40 };
    Vec2 shoulderR = { player.x + 20, player.y - 40 };

    if (punchState == IDLE) {
        float floatY = std::sin(frames * 0.1f) * 5.0f;
        leftArm = { shoulderL.x - 40, shoulderL.y + 20 + floatY };
        rightArm = { shoulderR.x + 40, shoulderR.y + 20 + floatY };
    }
    else if (punchState == WINDUP) {
        punchTimer++;
        float tx = punchTarget.x;
        float ty = punchTarget.y;

        leftArm.x += (tx - 250 - leftArm.x) * 0.25f;
        leftArm.y += (ty - leftArm.y) * 0.25f;
        rightArm.x += (tx + 250 - rightArm.x) * 0.25f;
        rightArm.y += (ty - rightArm.y) * 0.25f;

        if (punchTimer > WINDUP_FRAMES) { punchState = SMASH; punchTimer = 0; }
    }
    else if (punchState == SMASH) {
        punchTimer++;
        float scale = 1.0f + (level - 1) * 0.5f;
        float reach = 36 * scale;
        float speed = SMASH_SPEED;

        float txL = punchTarget.x - reach;
        float txR = punchTarget.x + reach;

        leftArm.x += (txL - leftArm.x) * speed;
        leftArm.y += (punchTarget.y - leftArm.y) * speed;
        rightArm.x += (txR - rightArm.x) * speed;
        rightArm.y += (punchTarget.y - rightArm.y) * speed;

        if (leftArm.x > punchTarget.x - reach) leftArm.x = punchTarget.x - reach;
        if (rightArm.x < punchTarget.x + reach) rightArm.x = punchTarget.x + reach;

        if (std::abs(rightArm.x - leftArm.x) <= reach * 2 + 15) {
            checkCollision(leftArm.x, rightArm.x, punchTarget.y);
        }

        if (punchTimer > 5) { punchState = HOLD; punchTimer = 0; }

    }
    else if (punchState == HOLD) {
        punchTimer++;
        if (punchTimer > 6) { punchState = RECOVER; punchTimer = 0; }
    }
    else if (punchState == RECOVER) {
        punchTimer++;
        float floatY = std::sin(frames * 0.1f) * 5.0f;

        float targetXL = shoulderL.x - 40;
        float targetYL = shoulderL.y + 20 + floatY;
        float targetXR = shoulderR.x + 40;
        float targetYR = shoulderR.y + 20 + floatY;

        leftArm.x += (targetXL - leftArm.x) * 0.2f;
        leftArm.y += (targetYL - leftArm.y) * 0.2f;
        rightArm.x += (targetXR - rightArm.x) * 0.2f;
        rightArm.y += (targetYR - rightArm.y) * 0.2f;

        if (punchTimer > 10) {
            punchState = IDLE;
            leftArm = { targetXL, targetYL };
            rightArm = { targetXR, targetYR };
        }
    }
}

void drawGlove(Renderer& r, float x, float y, bool isLeft) {
    float s = 1.0f + (level - 1) * 0.3f;

    SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_ADD);
    Color auraColor = COL_RED_500;
    if (level == 2) auraColor = COL_ORANGE;
    if (level == 3) auraColor = COL_YELLOW_400;
    if (level == 4) auraColor = COL_PURPLE;

    if (level >= 2) {
        float pulse = std::sin(frames * 0.2f) * 5.0f;
        auraColor.a = 60;
        r.fillCircle(x, y, (40 + pulse) * s, auraColor);
    }
    SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_BLEND);

    Color gc = COL_RED_500;
    if (level == 2) gc = COL_ORANGE;
    if (level == 3) gc = COL_YELLOW_400;
    if (level == 4) gc = COL_PURPLE;

    float cuffOffsetX = isLeft ? -20 * s : 20 * s;

    r.drawThickLine(
        x + cuffOffsetX, y - 15 * s,
        x + cuffOffsetX, y + 15 * s, 
        12 * s,
        COL_WHITE
    );

    float gloveOffsetX = isLeft ? 5 * s : -5 * s;
    r.fillCircle(x + gloveOffsetX, y, 28 * s, gc);

    float thumbX = isLeft ? 15 * s : -15 * s;
    r.fillCircle(x + thumbX, y - 10 * s, 12 * s, gc);

    r.fillCircle(x + gloveOffsetX, y - 10 * s, 8 * s, { 255, 255, 255, 80 });
}

void render(Renderer& r) {
    SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_NONE);
    // Background
    Color bg = COL_BG_DARK;
    if (level == 2) bg = { 46, 16, 5, 255 };
    if (level == 3) bg = { 30, 32, 16, 255 };
    if (level == 4) bg = { 21, 5, 46, 255 };
    r.setColor(bg);
    SDL_RenderClear(r.renderer);

    // Apply Camera
    r.camZoom = camZoom;
    if (shakeIntensity > 0) {
        r.shakeX = (randomFloat(0, 1) - 0.5f) * shakeIntensity;
        r.shakeY = (randomFloat(0, 1) - 0.5f) * shakeIntensity;
    }
    else {
        r.shakeX = 0; r.shakeY = 0;
    }

    float screenFloorY = r.transform(0, player.y).y;

    SDL_Rect floorRect;
    floorRect.x = 0;
    floorRect.y = (int)screenFloorY;
    floorRect.w = WINDOW_WIDTH;
    floorRect.h = WINDOW_HEIGHT;

    SDL_SetRenderDrawColor(r.renderer, 20, 25, 40, 255);
    SDL_RenderFillRect(r.renderer, &floorRect);
    r.drawThickLine(0, player.y, WINDOW_WIDTH, player.y, 4, { 60, 70, 90, 255 });

    //Additive Layer
    SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_ADD);

    for (auto& s : shockwaves) {
        Color c = s.color;
        c.a = (Uint8)(s.alpha * 255);
        r.fillCircle(s.x, s.y, s.radius, c);
    }

    for (auto& p : particles) {
        Color c = p.color;
        if (p.type == 3) { // Spark (Line)
            c.a = (Uint8)(p.life * 255);
            r.drawThickLine(p.x, p.y, p.x - p.vx * 2, p.y - p.vy * 2, p.w, c);
        }
        else if (p.type != 2) { // Not Debris
            c.a = (Uint8)(p.life * 255);
            r.fillCircle(p.x, p.y, p.size, c);
        }
    }

    SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_BLEND);

    for (auto& p : particles) {
        if (p.type == 2) {
            std::vector<Vec2> shape = {
                {-p.w / 2, -p.h / 2}, {p.w / 2, -p.h / 4}, {0, p.h / 2}, {-p.w / 2, p.h / 4}
            };
            r.drawPolygon(p.x, p.y, shape, p.rotation, 1.0f, p.color);
        }
    }

    if (gameState == MENU) {
        return;
    }

    for (auto& e : enemies) {
        auto getRotatedPos = [&](float dx, float dy) -> Vec2 {
            float rx = dx * std::cos(e.rotation) - dy * std::sin(e.rotation);
            float ry = dx * std::sin(e.rotation) + dy * std::cos(e.rotation);
            return { e.x + rx, e.y + ry };
        };

        if (e.type == CRATE) {
            float hs = e.size / 2.0f;
            std::vector<Vec2> boxShape = { {-hs,-hs}, {hs,-hs}, {hs,hs}, {-hs,hs} };

            Color shadowCol = { 0, 0, 0, 80 };
            r.drawPolygon(e.x + 10, e.y + 10, boxShape, e.rotation, 1.0f, shadowCol);

            r.drawPolygon(e.x, e.y, boxShape, e.rotation, 1.0f, e.color);

            Color strokeColor = { 120, 53, 15, 255 };
            float thick = 3.0f;

            Vec2 p1 = getRotatedPos(-hs, -hs);
            Vec2 p2 = getRotatedPos(hs, -hs);
            Vec2 p3 = getRotatedPos(hs, hs);
            Vec2 p4 = getRotatedPos(-hs, hs);

            r.drawThickLine(p1.x, p1.y, p2.x, p2.y, thick, strokeColor);
            r.drawThickLine(p2.x, p2.y, p3.x, p3.y, thick, strokeColor);
            r.drawThickLine(p3.x, p3.y, p4.x, p4.y, thick, strokeColor);
            r.drawThickLine(p4.x, p4.y, p1.x, p1.y, thick, strokeColor);
            r.drawThickLine(p1.x, p1.y, p3.x, p3.y, thick, strokeColor);
            r.drawThickLine(p2.x, p2.y, p4.x, p4.y, thick, strokeColor);
        }
        else if (e.type == HEX) {
            std::vector<Vec2> hex;
            for (int i = 0; i < 6; i++) {
                float a = i * PI / 3.0f;
                hex.push_back({ std::cos(a) * e.size / 1.5f, std::sin(a) * e.size / 1.5f });
            }

            r.drawPolygon(e.x + 10, e.y + 10, hex, e.rotation, 1.0f, { 0, 0, 0, 80 });

            r.drawPolygon(e.x, e.y, hex, e.rotation, 1.0f, e.color);

            Color strokeColor = { 76, 29, 149, 255 }; // #4c1d95
            for (int i = 0; i < 6; i++) {
                float a1 = i * PI / 3.0f;
                float a2 = (i + 1) * PI / 3.0f;
                Vec2 p1 = getRotatedPos(std::cos(a1) * e.size / 1.5f, std::sin(a1) * e.size / 1.5f);
                Vec2 p2 = getRotatedPos(std::cos(a2) * e.size / 1.5f, std::sin(a2) * e.size / 1.5f);
                r.drawThickLine(p1.x, p1.y, p2.x, p2.y, 3.0f, strokeColor);
            }
        }
        else {
            std::vector<Vec2> spikes;
            int numSpikes = 8;
            float innerR = e.size / 2.0f;
            float outerR = e.size / 1.3f;

            for (int i = 0; i < numSpikes * 2; i++) {
                float angle = i * PI / numSpikes;
                float r_val = (i % 2 == 0) ? outerR : innerR;
                spikes.push_back({
                    std::cos(angle) * r_val,
                    std::sin(angle) * r_val
                    });
            }

            r.drawPolygon(e.x + 10, e.y + 10, spikes, e.rotation, 1.0f, { 0, 0, 0, 80 });

            r.drawPolygon(e.x, e.y, spikes, e.rotation, 1.0f, e.color);

            Color strokeColor = { 127, 29, 29, 255 };

            for (size_t i = 0; i < spikes.size(); i++) {
                size_t next = (i + 1) % spikes.size();
                Vec2 p1 = getRotatedPos(spikes[i].x, spikes[i].y);
                Vec2 p2 = getRotatedPos(spikes[next].x, spikes[next].y);
                r.drawThickLine(p1.x, p1.y, p2.x, p2.y, 2.0f, strokeColor);
            }
        }
    }

    if (lockedEnemy && lockedEnemy->active) {
        float size = lockedEnemy->size + 20;
        float angle = frames * 0.1f;

        r.setColor({ 0, 255, 0, 255 });

        float cx = lockedEnemy->x;
        float cy = lockedEnemy->y;
        float s = size / 2.0f;
        float len = 15.0f;

        std::vector<Vec2> reticle;
        reticle.push_back({ -s, -s }); reticle.push_back({ s, -s });
        reticle.push_back({ s, s });   reticle.push_back({ -s, s });

        auto drawRotatedCorner = [&](float dx, float dy) {
            float rx = dx * std::cos(angle) - dy * std::sin(angle);
            float ry = dx * std::sin(angle) + dy * std::cos(angle);
        };

        Vec2 tl = r.transform(cx - s, cy - s);
        Vec2 tr = r.transform(cx + s, cy - s);
        Vec2 br = r.transform(cx + s, cy + s);
        Vec2 bl = r.transform(cx - s, cy + s);

        r.drawThickLine(cx - s, cy - s, cx + s, cy - s, 4, { 0, 255, 0, 255 });
        r.drawThickLine(cx + s, cy - s, cx + s, cy + s, 4, { 0, 255, 0, 255 });
        r.drawThickLine(cx + s, cy + s, cx - s, cy + s, 4, { 0, 255, 0, 255 });
        r.drawThickLine(cx - s, cy + s, cx - s, cy - s, 4, { 0, 255, 0, 255 });
    }

    // Player
    Vec2 shoulderL = { player.x - 15, player.y - 50 };
    Vec2 shoulderR = { player.x + 15, player.y - 50 };

    // Arms (Bezier)
    Vec2 midL = { (shoulderL.x + leftArm.x) / 2 - 50, (shoulderL.y + leftArm.y) / 2 + 20 };
    r.drawQuadraticBezier(shoulderL, midL, leftArm, 24, COL_SKIN);

    Vec2 midR = { (shoulderR.x + rightArm.x) / 2 + 50, (shoulderR.y + rightArm.y) / 2 + 20 };
    r.drawQuadraticBezier(shoulderR, midR, rightArm, 24, COL_SKIN);

    if (playerTexture) {
        int drawW = 1000;
        int drawH = 1000;
        SDL_Rect destRect;
        Vec2 screenPos = r.transform(player.x, player.y);
        destRect.x = (int)(screenPos.x - drawW / 2);
        int manualOffsetY = 450;
        destRect.y = (int)(screenPos.y - drawH + manualOffsetY);
        destRect.w = drawW;
        destRect.h = drawH;
        SDL_RenderCopy(r.renderer, playerTexture, NULL, &destRect);
    }
    else {
        r.fillCircle(player.x, player.y - 60, 30, COL_BLUE_500);
    }

    drawGlove(r, leftArm.x, leftArm.y, true);
    drawGlove(r, rightArm.x, rightArm.y, false);

    r.camZoom = 1.0f; r.shakeX = 0; r.shakeY = 0;
    r.drawNumber(score, 20, 50, 25, COL_YELLOW_400);
    r.drawNumber((int)std::max(0.0f, health), WINDOW_WIDTH - 150, 50, 25, COL_RED_500);

    // Floating Text
    for (auto& t : floatingTexts) {
        Color c = t.color;
        c.a = (Uint8)(t.life * 255); // Alpha fade not fully supported by drawNumber logic but works for blending
        r.drawNumber(t.value, t.x, t.y, 20, c);
    }

    // Flash
    if (flashIntensity > 0) {
        SDL_SetRenderDrawBlendMode(r.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r.renderer, 255, 255, 255, (Uint8)(flashIntensity * 255));
        SDL_Rect rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        SDL_RenderFillRect(r.renderer, &rect);
    }
}

// --- Main ---

int main(int argc, char* argv[]) {
    std::srand(std::time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Smash Master - C++ SDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) return 1;

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    Renderer r(sdlRenderer, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_Surface* tempSurface = SDL_LoadBMP("player.bmp");
    if (tempSurface) {
        Uint32 colKey = SDL_MapRGB(tempSurface->format, 255, 0, 255);
        SDL_SetColorKey(tempSurface, SDL_TRUE, colKey);

        playerTexture = SDL_CreateTextureFromSurface(sdlRenderer, tempSurface);
        SDL_FreeSurface(tempSurface);
    }
    else {
        std::cout << "failed finding player.bmp" << std::endl;
    }
    bool running = true;
    SDL_Event event;

    initGame();
    gameState = PLAYING;

    while (running) {
        // Input
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                WINDOW_WIDTH = event.window.data1;
                WINDOW_HEIGHT = event.window.data2;
                r.screenW = WINDOW_WIDTH; r.screenH = WINDOW_HEIGHT;
                player.y = WINDOW_HEIGHT - 100;
            }
            if (event.type == SDL_MOUSEMOTION) {
                mouse.x = event.motion.x;
                mouse.y = event.motion.y;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (gameState == MENU || gameState == GAME_OVER) {
                    initGame();
                    gameState = PLAYING;
                }
                else {
                    if (punchState == IDLE) triggerPunch();
                }
            }
        }

        // Loop
        if (gameState == PLAYING) {
            update();
        }

        // Draw
        render(r);

        if (gameState == MENU) {
            r.camZoom = 1.0f; r.shakeX = 0; r.shakeY = 0;
            r.fillCircle(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 80, COL_RED_500);
        }
        if (gameState == GAME_OVER) {
            r.camZoom = 1.0f; r.shakeX = 0; r.shakeY = 0;
            // Darken
            SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 200);
            SDL_Rect rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
            SDL_RenderFillRect(sdlRenderer, &rect);

            r.drawNumber(score, WINDOW_WIDTH / 2 - 50, WINDOW_HEIGHT / 2, 60, COL_YELLOW_400);
        }

        SDL_RenderPresent(sdlRenderer);
    }

    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}