//
// Created by bbgsm on 2024/8/24.
//

#include "Game.h"
#include <map>
#include "offsets.h"

// 骨骼地址缓存
std::map<Addr, Addr> playerBones[10];
// 玩家名称缓存
std::map<Addr, char[32]> playerNames;

Vector3D readBonePosition(MemoryToolsBase *mem, Addr player, Vector3D &origin, int id) {
    static const int MAX_BONE_INDEX = 255;
    float matrix[3][4];
    Addr &boneAddr = playerBones[id][player];
    if (boneAddr == 0) {
        Addr model = mem->readA(player, OFF_STUDIO_HDR);
        Addr studioHdr = mem->readA(model + 0x8);
        Addr hitboxCache = mem->readA(studioHdr + 0x34);
        Addr hitboxArray = studioHdr + ((ushort)(hitboxCache & 0xFFFE) << (4 * (hitboxCache & 1)));
        ushort indexCache = mem->readUS(hitboxArray + 0x4);
        int hitboxIndex = ((ushort)(indexCache & 0xFFFE) << (4 * (indexCache & 1)));
        ushort bone = mem->readUS(hitboxIndex + hitboxArray + (id * 0x20));
        if (bone < 0 || bone > MAX_BONE_INDEX) {
            return {};
        }
        Addr bones = mem->readA(player, OFF_BONES);
        boneAddr = bones + bone * sizeof(matrix);
    }
    mem->readV(matrix, sizeof(matrix), boneAddr);
    return {matrix[0][3] + origin.x, matrix[1][3] + origin.y, matrix[2][3] + origin.z};
}

void getName(MemoryToolsBase *mem, uint64_t baseAddr, ulong index, char *name) {
    index *= 0x18;
    Addr addr = baseAddr + OFF_NAME_LIST + index;
    if (playerNames.find(addr) != playerNames.end()) {
        memcpy(name, playerNames[addr], 32);
        return;
    }
    //    logDebug("nameAddr: %llX\n", addr);
    Addr nameAddr = mem->readA(addr);
    mem->readV(name, 32, nameAddr);
    name[31] = '\0';
    memcpy(playerNames[addr], name, 32);
}

bool worldToScreen(const Vector3D &from, const float *matrix, float screenWidth,
                   float screenHeight, VectorRect &screen) {
    // 计算齐次坐标中的 w 分量，使用变换矩阵的最后一行进行点乘
    float w = matrix[12] * from.x + matrix[13] * from.y + matrix[14] * from.z + matrix[15];
    // 如果 w 小于 0.01，则点不可见，返回 false
    if (w < 0.01f) {
        return false;
    }
    // 计算未除以 w 的屏幕 x 和 y 坐标，使用变换矩阵的第一行和第二行进行点乘
    float invW = 1.0f / w;
    float screenX = (matrix[0] * from.x + matrix[1] * from.y + matrix[2] * from.z + matrix[3]) * invW;
    float screenY = (matrix[4] * from.x + matrix[5] * from.y + matrix[6] * from.z + matrix[7]) * invW;
    // 计算屏幕的中心坐标
    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;
    // 将 NDC 坐标转换为屏幕坐标并应用偏移和缩放
    screen.x = centerX + 0.5f * screenX * screenWidth + 0.5f;
    screen.y = centerY - 0.5f * screenY * screenHeight + 0.5f;
    return true;
}

float computeDistance(Vector3D a, Vector3D b) {
    return sqrtf(powf(b.x - a.x, 2.0F) + powf(b.y - a.y, 2.0F) + powf(b.z - a.z, 2.0F));
}

void rotate(float x1, float y1, Vector2D &mapPosition, float angle) {
    mapPosition.x = x1 * cos(angle) - y1 * sin(angle);
    mapPosition.y = x1 * sin(angle) + y1 * cos(angle);
}

void removeInvalidUTF8(char *str) {
    if (str == nullptr) return;
    char *src = str;
    char *dst = str;
    while (*src) {
        // 检查单字节字符
        if ((src[0] & 0x80) == 0) {
            *dst++ = *src++;
            continue;
        }
        // 检查多字节字符的首字节
        if ((src[0] & 0xE0) == 0xC0) {
            if ((src[1] & 0xC0) == 0x80) {
                *dst++ = *src++;
                *dst++ = *src++;
                continue;
            }
        }
        // 检查多字节字符的首字节，可能的3字节或4字节字符
        else if ((src[0] & 0xF0) == 0xE0) {
            if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                continue;
            }
        } else if ((src[0] & 0xF8) == 0xF0) {
            if ((src[1] & 0xC0) == 0x80 && (src[2] & 0xC0) == 0x80 && (src[3] & 0xC0) == 0x80) {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                continue;
            }
        }
        // 如果不是有效的UTF-8字符，跳过
        src++;
    }
    *dst = '\0'; // 确保字符串以空字符结尾
}

void gameClear() {
    playerNames.clear();
    playerBones[0].clear();
}