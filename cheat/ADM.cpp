#include <iostream>
#include <map>

#include "DirectMemoryTools.h"
#include "DmaMemoryTools.h"
#include "DumpMemoryTools.h"
#include "EventLoop.h"
#include "Game.h"
#include "MemoryToolsBase.h"
#include "Render.h"
#include "Vector3D.hpp"
#include "VectorRect.hpp"
#include "WebSocketServer.h"
#include "hmutex.h"
#include "hooks.h"
#include "offsets.h"

MemoryToolsBase *mem;
Render render;

// 对象结构体
struct OObject {
    // 屏幕坐标
    VectorRect screenPosition;
    // 距离(米)
    int distance = 0;
    // 血量
    int health = 0;
    // 护盾值/最大护盾值
    int shieldHealth[2] = {0};
    // 名称下标
    int nameIndex = 0;
    // 团队id
    int teamId = -1;
    // 存活状态
    int lifeState = 0;
    // 名称
    char name[32] = {0};
    // 视角旋转
    Vector2D viewAngles;
    // 是否是玩家
    bool isPlayer = false;
    // 对象地址
    Addr addr = 0;
    // 物品id
    int itemId = 0;
    // 最后可见时间
    float lastVisTime = 0;
    // 对象3d坐标
    Vector3D playerPosition;
};

// 全局运行标记
bool isRun = true;
// 距离缩放比例
float distanceScal = 0.025;
// 最大玩家数量
const int maxPlayer = 100;

/*** 玩家、物资A/B交换缓存 ***/
OObject mapPlayersCacheA[1100];
int mapPlayerCacheCountA = 0;

OObject mapPlayersCacheB[1100];
int mapPlayerCacheCountB = 0;

bool isMapPlayersCacheA = true;
/*** 玩家、物资A/B交换缓存 ***/


float vx = 0;
float vy = 0;

// localPlayer 3d 坐标
Vector3D localPlayerPosition;
// 最大物品数
int maxObject = 10000;
// 物品列表读取起始下标
int beginObjectIndex = 2000;

/*** 物资A/B交换缓存 ***/
OObject objectsA[10000];
int objectCountA = 0;

OObject objectsB[10000];
int objectCountB = 0;

bool isObjectA = true;
/*** 物资A/B交换缓存 ***/

/* 物资地址缓存 */
Addr objectAddrs[10000];
OObject tempObjects[10000];
std::map<Addr, Vector3D> objectsMap;


std::map<int, std::string> mapNames = {{236, "3倍镜"},
                                       {201, "紫头"},
                                       {202, "金头"},
                                       {195, "医疗箱"},
                                       {280, "涡轮"},
                                       {246, "10倍镜"},
                                       {200, "凤凰"},
                                       {230, "蓝包"},
                                       {203, "大电"},
                                       {225, "紫包"},
                                       {226, "金包"},
                                       {235, "手雷"},
                                       {234, "铝热剂"},
                                       {236, "电弧星"},
                                       {335, "升级包"},

};


void drawObject(ImColor color, OObject player) {
    float x = player.screenPosition.x;
    float y = player.screenPosition.y;
    float w = player.screenPosition.w;
    float h = player.screenPosition.h;
    if (x <= 0 || x > render.screenWidth || y <= 0 || y > render.screenHeight) {
        return;
    }
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    std::string nameText = player.name;
    nameText += "(" + std::to_string(player.distance) + ")";
    ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());
    draw->AddText(ImVec2(x + w / 2 - (textSize.x / 2), y + h + 2), color, nameText.c_str());
}

void drawPlayer(ImColor color, OObject player, float line_w) {
    float x = player.screenPosition.x;
    float y = player.screenPosition.y;
    float w = player.screenPosition.w;
    float h = player.screenPosition.h;
    if (x <= 0 || x > render.screenWidth || y <= 0 || y > render.screenHeight) {
        return;
    }
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    float ww = w > 100 ? w : 100;
    // 定义血条的尺寸和位置
    ImVec2 progressBarSize(ww, 10);
    ImVec2 progressBarPosition{x - progressBarSize.x / 2, y - 12};
    x = x - (w / 2.0f);
    // 绘制血条的背景
    draw->AddRectFilled(progressBarPosition,
                        ImVec2(progressBarPosition.x + progressBarSize.x,
                               progressBarPosition.y + progressBarSize.y), IM_COL32(200, 200, 200, 255)); // 灰色背景
    // 计算血条的宽度
    float progress = player.health / 100.0F;
    float progressWidth = progressBarSize.x * progress;
    // 血条填充部分
    ImVec2 progressStart = progressBarPosition;
    ImVec2 progressEnd = ImVec2(progressStart.x + progressWidth, progressBarPosition.y + progressBarSize.y);
    draw->AddRectFilled(progressStart, progressEnd, IM_COL32(0, 255, 0, 255)); // 绿色填充
    // 血条边框
    draw->AddRect(progressBarPosition,
                  ImVec2(progressBarPosition.x + progressBarSize.x, progressBarPosition.y + progressBarSize.y),
                  IM_COL32(0, 0, 0, 255), 0, 15); // 黑色边框
    draw->AddLine(ImVec2(x, y), ImVec2(x + w, y), color, line_w);
    draw->AddLine(ImVec2(x, y), ImVec2(x, y + h), color, line_w);
    draw->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + h), color, line_w);
    draw->AddLine(ImVec2(x, y + h), ImVec2(x + w, y + h), color, line_w);
    std::string text = std::to_string(player.distance) + "M team:(" + std::to_string(player.teamId) + ")";
    std::string nameText = player.name;
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    ImVec2 nameTextSize = ImGui::CalcTextSize(nameText.c_str());
    draw->AddText(ImVec2(x + w / 2 - (textSize.x / 2), y + h + 2), IM_COL32(255, 255, 255, 255), text.c_str());
    draw->AddText(ImVec2(x + w / 2 - (nameTextSize.x / 2), y - 25), IM_COL32(255, 255, 255, 255), nameText.c_str());
}

/**
* 物品读取线程
*/
void objTest() {
    const Addr baseAddr = mem->getBaseAddr();
    Addr entityList = baseAddr + OFF_ENTITY_LIST;
    while (isRun) {
        Handle handle = mem->createScatter();
        for (int i = beginObjectIndex; i < maxObject; i++) {
            mem->addScatterReadV(handle, &objectAddrs[i], sizeof(Addr), entityList + (static_cast<Addr>(i) << 5));
        }
        mem->executeReadScatter(handle);
        mem->closeScatterHandle(handle);
        int objectIndex = 0;
        for (int i = beginObjectIndex; i < maxObject; i++) {
            Addr object = objectAddrs[i];
            int itemId = mem->readI(object, OFF_ITEM_ID);
            if (itemId < 200 || itemId > 300 || !mapNames.contains(itemId)) {
                continue;
            }
            // 物品坐标
            mem->readV(&tempObjects[objectIndex].playerPosition, sizeof(Vector3D), object,OFF_ORIGIN);
            // 超出范围的物品跳过
            float dis = computeDistance(localPlayerPosition, tempObjects[objectIndex].playerPosition) * distanceScal;
            if (dis > 200) {
                continue;
            }
            std::string str = std::to_string(itemId);
            tempObjects[objectIndex].isPlayer = false;
            tempObjects[objectIndex].itemId = itemId;
            tempObjects[objectIndex].distance = 10;
            strcpy(tempObjects[objectIndex].name, mapNames[itemId].c_str());
            // strcpy(tempObjects[objectIndex].name, std::to_string(itemId).c_str());
            if (objectIndex >= 10000) {
                break;
            }
            objectIndex++;
        }
        if (objectIndex > 0) {
            // objMtx.lock();
            if (isObjectA) {
                memcpy(objectsB, tempObjects, sizeof(OObject) * objectIndex);
                objectCountB = objectIndex;
            } else {
                memcpy(objectsA, tempObjects, sizeof(OObject) * objectIndex);
                objectCountA = objectIndex;
            }
            isObjectA = !isObjectA;
            // objMtx.unlock();
        } else {
            objectCountA = 0;
            objectCountB = 0;
        }
        // 延时10秒
        _sleep(10000);
    }

}

void surface(Addr baseAddr) {
    logInfo("screen width:%d height %d\n", render.screenWidth, render.screenHeight);

    float fx = 0;
    float fy = 0;
    float line = 1;
    float range = 300;
    float matrix[16];
    Addr playersAddrs[maxPlayer] = {0};
    float lastVisTimes[maxPlayer] = {0};
    OObject players[maxPlayer];

    int playerCount = 0;
    int playerIndex = 0;

    while (true) {
        render.drawBegin();
        bool gameState = mem->readB(baseAddr, OFF_GAME_STATE);

        ImDrawList *fpsDraw = ImGui::GetForegroundDrawList();

        std::string PerformanceString = "Render FPS: " + std::to_string(static_cast<int>(ImGui::GetIO().Framerate));
        fpsDraw->AddText(ImVec2(10, 50), IM_COL32(255, 255, 255, 255), PerformanceString.c_str());

        ImGui::SetNextWindowSize(ImVec2(400, 430), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(render.screenWidth - 410, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("ADM");

        ImGui::Checkbox("#gameState", &gameState);
        ImGui::SliderFloat("#offsetX", &fx, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat("#offsetY", &fy, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat("#line", &line, 0.0f, 10.0f, "%.1f");
        ImGui::SliderFloat("#range", &range, 0.0f, 10000.0f, "%.1f");
        ImGui::SliderFloat("#anglesX", &vx, -180, 180, "%.6f");
        ImGui::SliderFloat("#anglesY", &vy, -180, 180, "%.6f");
        ImGui::SliderInt("#maxObject", &maxObject, 0, 10000);
        ImGui::SliderInt("#beginObjectIndex", &beginObjectIndex, 0, 10000);

        if (ImGui::Button("exit")) {
            break;
        }

        if (true) {
            Addr localPlayerAddr = mem->readA(baseAddr, OFF_LOCAL_PLAYER);
            logDebug("localPlayerAddr: %llX\n", localPlayerAddr);
            OObject localPlayer;
            localPlayer.isPlayer = true;
            Handle handle = mem->createScatter();
            mem->addScatterReadV(handle, &localPlayer.playerPosition, sizeof(Vector3D), localPlayerAddr,
                                 OFF_ORIGIN);
            mem->addScatterReadV(handle, &localPlayer.teamId, sizeof(int), localPlayerAddr, OFF_TEAM);
            mem->addScatterReadV(handle, &localPlayer.nameIndex, sizeof(int), localPlayerAddr,
                                 OFF_INDEX_IN_NAMELIST);
            mem->addScatterReadV(handle, &localPlayer.itemId, sizeof(int), localPlayerAddr, OFF_ITEM_ID);
            mem->addScatterReadV(handle, &localPlayer.viewAngles, sizeof(Vector2D), localPlayerAddr,
                                 OFF_VIEW_ANGLES5);
            Addr entityList = baseAddr + OFF_ENTITY_LIST;
            for (int i = 0; i < maxPlayer; i++) {
                //玩家地址读取
                mem->addScatterReadV(handle, &playersAddrs[i], sizeof(Addr),
                                     entityList + (static_cast<Addr>(i) << 5));
            }
            mem->executeReadScatter(handle);
            mem->closeScatterHandle(handle);
            localPlayerPosition = localPlayer.playerPosition;
            vx = localPlayer.viewAngles.x;
            vy = localPlayer.viewAngles.y;

            players[playerIndex++] = localPlayer;
            handle = mem->createScatter();
            for (int i = 0; i < maxPlayer; i++) {
                Addr player = playersAddrs[i];
                if (!MemoryToolsBase::isAddrValid(player)) {
                    continue;
                }
                if (player == localPlayerAddr) {
                    continue;
                }
                // 玩家
                mem->addScatterReadV(handle, &players[playerIndex].teamId, sizeof(int), player, OFF_TEAM);
                mem->addScatterReadV(handle, &players[playerIndex].itemId, sizeof(int), player, OFF_ITEM_ID);
                mem->addScatterReadV(handle, &players[playerIndex].playerPosition, sizeof(Vector3D), player,
                                     OFF_ORIGIN);
                mem->addScatterReadV(handle, &players[playerIndex].viewAngles, sizeof(Vector2D), player,
                                     OFF_VIEW_ANGLES1);
                mem->addScatterReadV(handle, &players[playerIndex].lastVisTime, sizeof(float), player,
                                     OFF_VISIBLE_TIME);
                mem->addScatterReadV(handle, &players[playerIndex].lifeState, sizeof(int), player,
                                     OFF_LIFE_STATE);
                mem->addScatterReadV(handle, &players[playerIndex].nameIndex, sizeof(int), player,
                                     OFF_INDEX_IN_NAMELIST);
                mem->addScatterReadV(handle, &players[playerIndex].health, sizeof(int), player,
                                     OFF_HEALTH);
                mem->addScatterReadV(handle, players[playerIndex].shieldHealth,
                                     sizeof(players[playerIndex].shieldHealth), player, OFF_SHIELD);
                players[playerIndex].isPlayer = true;

                players[playerIndex].addr = player;
                playerIndex++;
            }
            mem->executeReadScatter(handle);
            mem->closeScatterHandle(handle);
            for (int i = 0; i < playerIndex; i++) {
                OObject &player = players[i];
                ImU32 color = IM_COL32(255, 0, 0, 255);
                do {
                    ulong entityIndex = player.nameIndex - 1;
                    getName(mem, baseAddr, entityIndex, player.name);
                    // 团队id校验
                    if (player.teamId < 0 || player.teamId > 50) {
                        break;
                    }
                    // 跳过队友
                    if (player.teamId == localPlayer.teamId) {
                        break;
                    }
                    // 超过范围跳过
                    float dis = computeDistance(localPlayer.playerPosition, player.playerPosition) * distanceScal;
                    if (dis > range) {
                        break;
                    }
                    // 读取矩阵
                    mem->readV(matrix, sizeof(matrix), baseAddr, OFF_MATRIX1);
                    player.distance = dis;
                    // 懒得画盾，暂时把盾和血量的总和加起来计算百分比
                    player.health = static_cast<int>((static_cast<float>(player.health + player.shieldHealth[0]) / (
                        100.0F + player.shieldHealth[1])) * 100.0F);
                    if (!worldToScreen(player.playerPosition, matrix, render.screenWidth, render.screenHeight,
                                       player.screenPosition)) {
                        break;
                    }
                    // 读取头部骨骼坐标
                    Vector3D HeadPosition = readBonePosition(mem, player.addr, player.playerPosition, 0);
                    VectorRect hs;
                    worldToScreen(HeadPosition, matrix, render.screenWidth, render.screenHeight, hs);
                    player.screenPosition.h = abs(abs(hs.y) - abs(player.screenPosition.y));
                    player.screenPosition.w = player.screenPosition.h / 2.0f;
                    player.screenPosition.x += fx;
                    player.screenPosition.y += fy - player.screenPosition.h;
                    if (player.lifeState != 0) {
                        color = IM_COL32(51, 255, 255, 255);
                    } else if (player.lastVisTime > lastVisTimes[i]) {
                        color = IM_COL32(255, 255, 0, 255);
                    } else {
                        color = IM_COL32(255, 0, 0, 255);
                    }
                    logDebug("playerAddr: %llX index: %d name:%s\n", player.addr, player.nameIndex, player.name);
                    drawPlayer(color, player, line);
                    lastVisTimes[i] = player.lastVisTime;
                    playerCount++;
                } while (false);
            }
            OObject *objects;
            int objectCount;
            if (isObjectA) {
                objects = objectsA;
                objectCount = objectCountA;
            } else {
                objects = objectsB;
                objectCount = objectCountB;
            }
            // 读取矩阵
            mem->readV(matrix, sizeof(matrix), baseAddr, OFF_MATRIX1);
            for (int j = 0; j < objectCount; j++) {
                if (!worldToScreen(objects[j].playerPosition, matrix, render.screenWidth, render.screenHeight,
                                   objects[j].screenPosition)) {
                    continue;
                }
                float dis = computeDistance(localPlayerPosition, objects[j].playerPosition) * distanceScal;
                objects[j].distance = dis;
                drawObject(IM_COL32(255, 255, 255, 255), objects[j]);
            }
            // 玩家和物品数据都复制到缓存数组，给web使用
            if (isMapPlayersCacheA) {
                memcpy(mapPlayersCacheB, players, sizeof(OObject) * playerIndex);
                memcpy(mapPlayersCacheB + sizeof(OObject) * playerIndex, objects, sizeof(OObject) * objectCount);
                mapPlayerCacheCountB = playerIndex;
            } else {
                memcpy(mapPlayersCacheA, players, sizeof(OObject) * playerIndex);
                memcpy(mapPlayersCacheA + sizeof(OObject) * playerIndex, objects, sizeof(OObject) * objectCount);
                mapPlayerCacheCountA = playerIndex;
            }
            isMapPlayersCacheA = !isMapPlayersCacheA;
            if (playerCount == 0) {
                gameClear();
            }
            playerIndex = 0;
            playerCount = 0;
        }
        render.drawEnd();
    }
}


using namespace hv;
WebSocketServer server;
WebSocketService wws;
HttpService http;

/**
* 网页地图分享功能
*/
class MyContext {
public:
    MyContext() {
        logDebug("MyContext::MyContext()\n");
        timerID = INVALID_TIMER_ID;
    }

    ~MyContext() {
        logDebug("MyContext::~MyContext()\n");
    }

    static int handleMessage(const std::string &msg, enum ws_opcode opcode) {
        logDebug("onmessage(type=%s len=%d): %.*s\n", opcode == WS_OPCODE_TEXT ? "text" : "binary", (int)msg.size(), (
                int)msg
            .size(), msg.data());
        return msg.size();
    }

    static void run(const WebSocketChannelPtr &channel) {
        OObject *objects;
        int objectCount;
        if (isMapPlayersCacheA) {
            objects = mapPlayersCacheA;
            objectCount = mapPlayerCacheCountA;
        } else {
            objects = mapPlayersCacheB;
            objectCount = mapPlayerCacheCountB;
        }
        Json data;
        for (int i = 0; i < objectCount; i++) {
            OObject &p = objects[i];
            Json objectJson;
            if (p.isPlayer) {
                objectJson["x"] = p.playerPosition.x;
                objectJson["y"] = p.playerPosition.y;
                objectJson["z"] = p.playerPosition.z;
                objectJson["teamId"] = p.teamId;
                objectJson["isPlayer"] = p.isPlayer;
                objectJson["health"] = p.health;
                objectJson["direction"] = p.viewAngles.y;
                p.name[31] = '\0';
                if (strlen(p.name) <= 0) {
                    continue;
                }
                // 玩家的名字是八仙过海，各显神通，只能在这移除不是utf8的字符，不然Json工具会抛异常
                removeInvalidUTF8(p.name);
                objectJson["name"] = p.name;
            } else {
                objectJson["isPlayer"] = p.isPlayer;
                objectJson["x"] = p.playerPosition.x;
                objectJson["y"] = p.playerPosition.y;
                objectJson["z"] = p.playerPosition.z;
                objectJson["name"] = p.name;
            }
            //            objectJson["addr"] = p.addr;
            data += {objectJson};
        }
        try {
            if (!data.empty()) {
                std::ostringstream stream;
                stream << data;
                std::string js = stream.str();
                channel->send(js);
            } else {
                channel->send("[]");
            }
        } catch (std::exception &e) {
            logDebug("exception: %s\n", e.what());
        }
    }

    TimerID timerID;
};

void websocketServer() {
    int port = 6888;
    http.GET("/ping", [](const HttpContextPtr &ctx) {
        return ctx->send("pong", TEXT_HTML);
    });
    http.Static("/","webMap");
    wws.onopen = [](const WebSocketChannelPtr &channel, const HttpRequestPtr &req) {
        logInfo("onopen: GET %s\n", req->Path().c_str());
        auto ctx = channel->newContextPtr<MyContext>();
        // send(time) every 100ms
        ctx->timerID = setInterval(100, [channel](TimerID id) {
            if (channel->isConnected() && channel->isWriteComplete()) {
                MyContext::run(channel);
            }
        });
    };
    wws.onmessage = [](const WebSocketChannelPtr &channel, const std::string &msg) {
        auto ctx = channel->getContextPtr<MyContext>();
        ctx->handleMessage(msg, channel->opcode);
    };
    wws.onclose = [](const WebSocketChannelPtr &channel) {
        logInfo("onclose\n");
        auto ctx = channel->getContextPtr<MyContext>();
        if (ctx->timerID != INVALID_TIMER_ID) {
            killTimer(ctx->timerID);
            ctx->timerID = INVALID_TIMER_ID;
        }
        // channel->deleteContextPtr();
    };
    server.port = port;
    server.registerHttpService(&http);
    server.registerWebSocketService(&wws);
    server.start();
    logInfo("---- WebSocket Server initialized ----\n");
}

void plugin() {
    const Addr baseAddr = mem->getBaseAddr();
    logInfo("Pid: %d\n", mem->getProcessPid());
    logInfo("BaseAddr: %llX\n", baseAddr);
    logInfo("---- END ----\n");
    websocketServer();
    // 物品读取线程
    std::thread th(objTest);
    th.detach();
    render.initImGui(L"ADM");
    surface(baseAddr);
    render.destroyImGui();
    isRun = false;
}


int main() {
    // 可以直接在游戏电脑上读取内存（不怕封号就逝逝）
    // mem = new DirectMemoryTools();
    // Dma读取内存
    mem = new DmaMemoryTools();
    if (!mem->init("r5apex.exe")) {
        logInfo("Failed to initialized DMA\n");
    }
    // 读取Dump的内存
    // mem = new DumpMemoryTools();
    // if (!mem->init("C:\\Users\\user\\Desktop\\test\\apex7\\dict.txt")) {
    //     logInfo("Failed to initialized Dump\n");
    // } else {
    //     logInfo("Dump initialized\n");
    // }
    plugin();
    delete mem;
    return 0;
}