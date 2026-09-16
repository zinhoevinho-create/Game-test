/*
 * JUMP QUEST - Homebrew platformer para PSP
 * Personagem original (robozinho), inspirado na mecanica classica
 * de plataforma (pular em blocos, coletar moedas, pisar em inimigos,
 * chegar numa bandeira no fim da fase).
 *
 * Todo o conteudo (codigo, sprites, niveis) e original, criado do zero.
 * Nenhum asset de jogo comercial e usado.
 *
 * Controles:
 *   Setas / Analog Esq/Dir -> mover
 *   X (Cross)              -> pular
 *   START                  -> pausar
 *   SELECT                 -> reiniciar fase atual
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("JumpQuest", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCR_W 480
#define SCR_H 272
#define BUF_WIDTH 512

/* ---------------------------------------------------------------- */
/* Callback padrao de saida (HOME button)                            */
/* ---------------------------------------------------------------- */
int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread,
                                      0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
    return thid;
}

/* ---------------------------------------------------------------- */
/* GU (grafico) - vertice simples com cor + posicao                  */
/* ---------------------------------------------------------------- */
typedef struct { unsigned int color; float x, y, z; } Vertex;

static unsigned int __attribute__((aligned(16))) list[262144];

void draw_rect(float x, float y, float w, float h, unsigned int color) {
    Vertex* vertices = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].color = color;
    vertices[0].x = x;     vertices[0].y = y;     vertices[0].z = 0;
    vertices[1].color = color;
    vertices[1].x = x + w; vertices[1].y = y + h; vertices[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);
    sceGumDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, vertices);
}

/* ---------------------------------------------------------------- */
/* Entidades do jogo                                                  */
/* ---------------------------------------------------------------- */
#define MAX_PLATFORMS 12
#define MAX_COINS     10
#define MAX_ENEMIES   4
#define MAX_LEVELS    5

typedef struct { float x, y, w, h; } Rect;

typedef struct {
    Rect r;
    int alive;
} Coin;

typedef struct {
    Rect r;
    float speed;
    float minX, maxX;
    int alive;
} Enemy;

typedef struct {
    Rect platforms[MAX_PLATFORMS];
    int  numPlatforms;
    Coin coins[MAX_COINS];
    int  numCoins;
    Enemy enemies[MAX_ENEMIES];
    int  numEnemies;
    Rect goal;
    float playerStartX, playerStartY;
} Level;

Level levels[MAX_LEVELS];

/* Robozinho (jogador) */
typedef struct {
    float x, y;
    float vx, vy;
    int onGround;
    int facingRight;
} Player;

Player player;

int currentLevel = 0;
int score = 0;
int lives = 3;
int gameWon = 0;
int gameOver = 0;
int paused = 0;

#define GRAVITY      0.6f
#define JUMP_FORCE  -9.5f
#define MOVE_SPEED   2.6f
#define PLAYER_W     16.0f
#define PLAYER_H     20.0f

/* Cores (ARGB / GU_COLOR_8888 em ordem 0xAABBGGRR) */
#define COLOR_SKY     0xFFEEC29A
#define COLOR_GROUND  0xFF2E86D8
#define COLOR_PLAT    0xFF3D9BE0
#define COLOR_ROBOT   0xFF4CC2E8
#define COLOR_ROBOT2  0xFF1F6FA8
#define COLOR_COIN    0xFF2AD1E8
#define COLOR_ENEMY   0xFF2A2AD0
#define COLOR_GOAL    0xFF3ADC6B

/* ---------------------------------------------------------------- */
/* Construcao das 5 fases                                             */
/* ---------------------------------------------------------------- */
void build_levels(void) {
    memset(levels, 0, sizeof(levels));

    /* ---- Fase 1: introdutoria, poucos obstaculos ---- */
    Level* l = &levels[0];
    l->playerStartX = 20; l->playerStartY = 200;
    l->platforms[l->numPlatforms++] = (Rect){0, 240, 480, 32};      /* chao */
    l->platforms[l->numPlatforms++] = (Rect){150, 190, 60, 12};
    l->platforms[l->numPlatforms++] = (Rect){280, 150, 60, 12};
    l->coins[l->numCoins++] = (Coin){{170, 160, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{300, 120, 12, 12}, 1};
    l->enemies[l->numEnemies++] = (Enemy){{380, 220, 18, 18}, 1.0f, 360, 450, 1};
    l->goal = (Rect){440, 190, 16, 50};

    /* ---- Fase 2: mais plataformas em altura ---- */
    l = &levels[1];
    l->playerStartX = 20; l->playerStartY = 200;
    l->platforms[l->numPlatforms++] = (Rect){0, 240, 200, 32};
    l->platforms[l->numPlatforms++] = (Rect){260, 240, 220, 32};
    l->platforms[l->numPlatforms++] = (Rect){120, 190, 50, 12};
    l->platforms[l->numPlatforms++] = (Rect){200, 150, 50, 12};
    l->platforms[l->numPlatforms++] = (Rect){300, 190, 50, 12};
    l->coins[l->numCoins++] = (Coin){{135, 160, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{215, 120, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{315, 160, 12, 12}, 1};
    l->enemies[l->numEnemies++] = (Enemy){{270, 220, 18, 18}, 1.3f, 260, 400, 1};
    l->goal = (Rect){440, 190, 16, 50};

    /* ---- Fase 3: gap no chao, precisa pular com precisao ---- */
    l = &levels[2];
    l->playerStartX = 20; l->playerStartY = 200;
    l->platforms[l->numPlatforms++] = (Rect){0, 240, 140, 32};
    l->platforms[l->numPlatforms++] = (Rect){180, 240, 100, 32};
    l->platforms[l->numPlatforms++] = (Rect){340, 240, 140, 32};
    l->platforms[l->numPlatforms++] = (Rect){90, 180, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){220, 160, 40, 12};
    l->coins[l->numCoins++] = (Coin){{100, 150, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{230, 130, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{370, 210, 12, 12}, 1};
    l->enemies[l->numEnemies++] = (Enemy){{190, 220, 18, 18}, 1.1f, 180, 270, 1};
    l->enemies[l->numEnemies++] = (Enemy){{360, 220, 18, 18}, 1.4f, 345, 460, 1};
    l->goal = (Rect){440, 190, 16, 50};

    /* ---- Fase 4: torres, mais verticalidade ---- */
    l = &levels[3];
    l->playerStartX = 20; l->playerStartY = 200;
    l->platforms[l->numPlatforms++] = (Rect){0, 240, 480, 32};
    l->platforms[l->numPlatforms++] = (Rect){60, 190, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){140, 150, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){220, 110, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){300, 150, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){380, 190, 40, 12};
    l->coins[l->numCoins++] = (Coin){{155, 120, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{235, 80, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{315, 120, 12, 12}, 1};
    l->enemies[l->numEnemies++] = (Enemy){{150, 220, 18, 18}, 1.2f, 60, 200, 1};
    l->enemies[l->numEnemies++] = (Enemy){{330, 220, 18, 18}, 1.5f, 260, 420, 1};
    l->goal = (Rect){440, 190, 16, 50};

    /* ---- Fase 5: final, mistura tudo ---- */
    l = &levels[4];
    l->playerStartX = 20; l->playerStartY = 200;
    l->platforms[l->numPlatforms++] = (Rect){0, 240, 120, 32};
    l->platforms[l->numPlatforms++] = (Rect){160, 240, 80, 32};
    l->platforms[l->numPlatforms++] = (Rect){300, 240, 180, 32};
    l->platforms[l->numPlatforms++] = (Rect){70, 180, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){180, 150, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){260, 190, 40, 12};
    l->platforms[l->numPlatforms++] = (Rect){340, 130, 40, 12};
    l->coins[l->numCoins++] = (Coin){{85, 150, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{195, 120, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{275, 160, 12, 12}, 1};
    l->coins[l->numCoins++] = (Coin){{355, 100, 12, 12}, 1};
    l->enemies[l->numEnemies++] = (Enemy){{170, 220, 18, 18}, 1.3f, 160, 235, 1};
    l->enemies[l->numEnemies++] = (Enemy){{320, 220, 18, 18}, 1.6f, 300, 470, 1};
    l->enemies[l->numEnemies++] = (Enemy){{400, 220, 18, 18}, 1.0f, 380, 470, 1};
    l->goal = (Rect){440, 190, 16, 50};
}

/* ---------------------------------------------------------------- */
/* Colisao AABB simples                                               */
/* ---------------------------------------------------------------- */
int check_collision(float ax, float ay, float aw, float ah,
                     float bx, float by, float bw, float bh) {
    return (ax < bx + bw && ax + aw > bx &&
            ay < by + bh && ay + ah > by);
}

void load_level(int idx) {
    Level* l = &levels[idx];
    player.x = l->playerStartX;
    player.y = l->playerStartY;
    player.vx = 0; player.vy = 0;
    player.onGround = 0;
    player.facingRight = 1;
    for (int i = 0; i < l->numCoins; i++) l->coins[i].alive = 1;
    for (int i = 0; i < l->numEnemies; i++) l->enemies[i].alive = 1;
    gameWon = 0;
}

/* ---------------------------------------------------------------- */
/* Update de fisica e logica                                         */
/* ---------------------------------------------------------------- */
void update_game(SceCtrlData pad) {
    if (gameOver || gameWon) return;

    Level* l = &levels[currentLevel];

    /* Movimento horizontal */
    player.vx = 0;
    if (pad.Buttons & PSP_CTRL_LEFT)  { player.vx = -MOVE_SPEED; player.facingRight = 0; }
    if (pad.Buttons & PSP_CTRL_RIGHT) { player.vx =  MOVE_SPEED; player.facingRight = 1; }
    if (pad.Lx < 100) { player.vx = -MOVE_SPEED; player.facingRight = 0; }
    if (pad.Lx > 160) { player.vx =  MOVE_SPEED; player.facingRight = 1; }

    /* Pulo */
    if ((pad.Buttons & PSP_CTRL_CROSS) && player.onGround) {
        player.vy = JUMP_FORCE;
        player.onGround = 0;
    }

    /* Gravidade */
    player.vy += GRAVITY;
    if (player.vy > 12.0f) player.vy = 12.0f;

    /* Move eixo X e checa colisao com plataformas */
    player.x += player.vx;
    if (player.x < 0) player.x = 0;
    if (player.x > SCR_W - PLAYER_W) player.x = SCR_W - PLAYER_W;

    /* Move eixo Y e checa colisao vertical */
    player.y += player.vy;
    player.onGround = 0;
    for (int i = 0; i < l->numPlatforms; i++) {
        Rect* p = &l->platforms[i];
        if (check_collision(player.x, player.y, PLAYER_W, PLAYER_H,
                             p->x, p->y, p->w, p->h)) {
            if (player.vy > 0) {
                player.y = p->y - PLAYER_H;
                player.vy = 0;
                player.onGround = 1;
            } else if (player.vy < 0) {
                player.y = p->y + p->h;
                player.vy = 0;
            }
        }
    }

    /* Caiu no vazio -> perde vida */
    if (player.y > SCR_H) {
        lives--;
        if (lives <= 0) {
            gameOver = 1;
        } else {
            load_level(currentLevel);
        }
        return;
    }

    /* Coletar moedas */
    for (int i = 0; i < l->numCoins; i++) {
        if (l->coins[i].alive &&
            check_collision(player.x, player.y, PLAYER_W, PLAYER_H,
                l->coins[i].r.x, l->coins[i].r.y, l->coins[i].r.w, l->coins[i].r.h)) {
            l->coins[i].alive = 0;
            score += 100;
        }
    }

    /* Inimigos: patrulha e colisao (pisar mata, tocar de lado machuca) */
    for (int i = 0; i < l->numEnemies; i++) {
        Enemy* e = &l->enemies[i];
        if (!e->alive) continue;
        e->r.x += e->speed;
        if (e->r.x < e->minX || e->r.x > e->maxX) e->speed = -e->speed;

        if (check_collision(player.x, player.y, PLAYER_W, PLAYER_H,
                             e->r.x, e->r.y, e->r.w, e->r.h)) {
            /* pisou por cima -> mata o inimigo */
            if (player.vy > 0 && (player.y + PLAYER_H) - e->r.y < 10) {
                e->alive = 0;
                player.vy = JUMP_FORCE * 0.6f;
                score += 200;
            } else {
                /* tocou de lado -> perde vida e reinicia fase */
                lives--;
                if (lives <= 0) gameOver = 1;
                else load_level(currentLevel);
                return;
            }
        }
    }

    /* Chegou na bandeira/goal */
    if (check_collision(player.x, player.y, PLAYER_W, PLAYER_H,
                         l->goal.x, l->goal.y, l->goal.w, l->goal.h)) {
        gameWon = 1;
    }
}

/* ---------------------------------------------------------------- */
/* Render                                                             */
/* ---------------------------------------------------------------- */
void draw_player(void) {
    /* corpo do robozinho: bloco maior + "cabeca" menor + antena */
    draw_rect(player.x, player.y + 4, PLAYER_W, PLAYER_H - 4, COLOR_ROBOT);
    draw_rect(player.x + 2, player.y, PLAYER_W - 4, 8, COLOR_ROBOT2);
    /* antena */
    draw_rect(player.x + PLAYER_W/2 - 1, player.y - 6, 2, 6, COLOR_ROBOT2);
    /* "olho" indicando direcao */
    if (player.facingRight)
        draw_rect(player.x + PLAYER_W - 5, player.y + 2, 3, 3, 0xFFFFFFFF);
    else
        draw_rect(player.x + 2, player.y + 2, 3, 3, 0xFFFFFFFF);
}

void render_level(void) {
    Level* l = &levels[currentLevel];

    draw_rect(0, 0, SCR_W, SCR_H, COLOR_SKY);

    for (int i = 0; i < l->numPlatforms; i++) {
        Rect* p = &l->platforms[i];
        unsigned int c = (p->y > 235) ? COLOR_GROUND : COLOR_PLAT;
        draw_rect(p->x, p->y, p->w, p->h, c);
    }

    for (int i = 0; i < l->numCoins; i++) {
        if (l->coins[i].alive) {
            Coin* c = &l->coins[i];
            draw_rect(c->r.x, c->r.y, c->r.w, c->r.h, COLOR_COIN);
        }
    }

    for (int i = 0; i < l->numEnemies; i++) {
        if (l->enemies[i].alive) {
            Enemy* e = &l->enemies[i];
            draw_rect(e->r.x, e->r.y, e->r.w, e->r.h, COLOR_ENEMY);
        }
    }

    /* bandeira/goal */
    draw_rect(l->goal.x, l->goal.y, 4, l->goal.h, 0xFF555555);
    draw_rect(l->goal.x + 4, l->goal.y, 12, 16, COLOR_GOAL);

    draw_player();
}

/* ---------------------------------------------------------------- */
/* Main                                                               */
/* ---------------------------------------------------------------- */
int main(int argc, char* argv[]) {
    SetupCallbacks();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_W, SCR_H, (void*)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_W/2), 2048 - (SCR_H/2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(0,0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    build_levels();
    currentLevel = 0;
    score = 0;
    lives = 3;
    gameOver = 0;
    load_level(currentLevel);

    SceCtrlData pad;
    int prevStart = 0, prevSelect = 0;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        /* SELECT reinicia a fase atual */
        if ((pad.Buttons & PSP_CTRL_SELECT) && !prevSelect) {
            load_level(currentLevel);
        }
        prevSelect = (pad.Buttons & PSP_CTRL_SELECT) ? 1 : 0;

        /* START pausa */
        if ((pad.Buttons & PSP_CTRL_START) && !prevStart) {
            paused = !paused;
        }
        prevStart = (pad.Buttons & PSP_CTRL_START) ? 1 : 0;

        if (!paused) {
            update_game(pad);

            /* Venceu a fase -> avanca ou termina o jogo */
            if (gameWon) {
                if (currentLevel < MAX_LEVELS - 1) {
                    currentLevel++;
                    load_level(currentLevel);
                } else {
                    gameOver = 1; /* jogo completo */
                }
            }
        }

        sceGuStart(GU_DIRECT, list);
        render_level();
        sceGuFinish();
        sceGuSync(0, 0);

        pspDebugScreenSetOffset((int)sceGuSwapBuffers());
        pspDebugScreenSetXY(1, 1);
        pspDebugScreenPrintf("JUMP QUEST  Fase %d/%d  Pontos: %d  Vidas: %d",
            currentLevel + 1, MAX_LEVELS, score, lives);

        if (paused) {
            pspDebugScreenSetXY(1, 3);
            pspDebugScreenPrintf("PAUSADO - START para continuar");
        }
        if (gameOver && lives <= 0) {
            pspDebugScreenSetXY(1, 3);
            pspDebugScreenPrintf("GAME OVER - SELECT para reiniciar a fase");
        }
        if (gameOver && lives > 0) {
            pspDebugScreenSetXY(1, 3);
            pspDebugScreenPrintf("PARABENS! Voce completou o Jump Quest!");
        }

        sceDisplayWaitVblankStart();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}

