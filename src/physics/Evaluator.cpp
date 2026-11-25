// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) SoundPhysics.java, ReflectedAudio.java, utils/RaycastUtils.java, ported to C++ and modified.
#include "physics/Evaluator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/phys/AABB.h"

#include "config/Config.h"
#include "physics/Materials.h"

namespace sound_physics {

namespace {

constexpr float PHI = 1.618033988F;

D3 operator+(D3 a, D3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
D3 operator-(D3 a, D3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
D3 operator*(D3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }

double dot(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(D3 a) { return std::sqrt(dot(a, a)); }
double distance(D3 a, D3 b) { return length(a - b); }

D3 normalize(D3 a) {
    double const l = length(a);
    return l < 1.0E-5 ? D3{} : a * (1.0 / l);
}

BlockPos floorPos(D3 p) {
    return BlockPos{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z))};
}

bool same(BlockPos const& a, BlockPos const& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

struct RayHit {
    bool     hit = false;
    BlockPos pos{0, 0, 0};
    D3       location;
    D3       normal;
};

bool clipBox(D3 from, D3 delta, D3 lo, D3 hi, double& outT, D3& outNormal) {
    double tMin = 0.0, tMax = 1.0;
    D3     n{};
    bool   entered = false;
    for (int axis = 0; axis < 3; ++axis) {
        double const f = axis == 0 ? from.x : (axis == 1 ? from.y : from.z);
        double const d = axis == 0 ? delta.x : (axis == 1 ? delta.y : delta.z);
        double const a = axis == 0 ? lo.x : (axis == 1 ? lo.y : lo.z);
        double const b = axis == 0 ? hi.x : (axis == 1 ? hi.y : hi.z);
        if (std::abs(d) < 1.0E-12) {
            if (f < a || f > b) return false;
            continue;
        }
        double t0 = (a - f) / d, t1 = (b - f) / d;
        double const sign = d > 0 ? -1.0 : 1.0;
        if (t0 > t1) std::swap(t0, t1);
        if (t0 > tMin) {
            tMin    = t0;
            n       = axis == 0 ? D3{sign, 0, 0} : (axis == 1 ? D3{0, sign, 0} : D3{0, 0, sign});
            entered = true;
        }
        tMax = (std::min)(tMax, t1);
        if (tMin > tMax) return false;
    }
    if (!entered) {
        double const ax = std::abs(delta.x), ay = std::abs(delta.y), az = std::abs(delta.z);
        if (ax >= ay && ax >= az) n = {delta.x > 0 ? -1.0 : 1.0, 0, 0};
        else if (ay >= az) n = {0, delta.y > 0 ? -1.0 : 1.0, 0};
        else n = {0, 0, delta.z > 0 ? -1.0 : 1.0};
    }
    outT      = tMin;
    outNormal = n;
    return true;
}

bool clipCell(BlockSource& region, BlockPos const& pos, D3 from, D3 delta, RayHit& out) {
    Block const& block = region.getBlock(pos);
    if (block.isAir()) return false;
    D3 lo{double(pos.x), double(pos.y), double(pos.z)};
    D3 hi = lo + D3{1, 1, 1};
    if (!acousticsOf(block).liquid) {
        AABB const shape = block.getBlockType().getCollisionShape(block, region, pos, {});
        if (shape.max.x <= shape.min.x || shape.max.y <= shape.min.y || shape.max.z <= shape.min.z) return false;
        lo = {shape.min.x, shape.min.y, shape.min.z};
        hi = {shape.max.x, shape.max.y, shape.max.z};
    }
    double t = 0;
    D3     n{};
    if (!clipBox(from, delta, lo, hi, t, n)) return false;
    out.hit      = true;
    out.pos      = pos;
    out.location = from + delta * t;
    out.normal   = n;
    return true;
}

RayHit rayCast(BlockSource& region, D3 from, D3 to, BlockPos const* ignore) {
    RayHit   result;
    D3 const delta = to - from;
    BlockPos cell  = floorPos(from);
    BlockPos end   = floorPos(to);
    int const stepX = delta.x > 0 ? 1 : (delta.x < 0 ? -1 : 0);
    int const stepY = delta.y > 0 ? 1 : (delta.y < 0 ? -1 : 0);
    int const stepZ = delta.z > 0 ? 1 : (delta.z < 0 ? -1 : 0);
    auto boundary = [](double p, double d, int c) {
        if (d > 0) return (c + 1 - p) / d;
        if (d < 0) return (c - p) / d;
        return (std::numeric_limits<double>::max)();
    };
    double tX = boundary(from.x, delta.x, cell.x), tY = boundary(from.y, delta.y, cell.y), tZ = boundary(from.z, delta.z, cell.z);
    double const dX = stepX ? std::abs(1.0 / delta.x) : (std::numeric_limits<double>::max)();
    double const dY = stepY ? std::abs(1.0 / delta.y) : (std::numeric_limits<double>::max)();
    double const dZ = stepZ ? std::abs(1.0 / delta.z) : (std::numeric_limits<double>::max)();
    int const maxSteps = std::abs(end.x - cell.x) + std::abs(end.y - cell.y) + std::abs(end.z - cell.z) + 1;
    for (int i = 0; i < maxSteps; ++i) {
        if ((!ignore || !same(cell, *ignore)) && clipCell(region, cell, from, delta, result)) return result;
        if (tX <= tY && tX <= tZ) {
            cell.x += stepX;
            tX += dX;
        } else if (tY <= tZ) {
            cell.y += stepY;
            tY += dY;
        } else {
            cell.z += stepZ;
            tZ += dZ;
        }
    }
    result.hit      = false;
    result.location = to;
    return result;
}

std::optional<D3> sharedAirspace(BlockSource& region, D3 start, D3 listener) {
    if (rayCast(region, start, listener, nullptr).hit) return std::nullopt;
    return start - listener;
}

unsigned char nearestFace(D3 v) {
    double const ax = std::abs(v.x), ay = std::abs(v.y), az = std::abs(v.z);
    if (ax >= ay && ax >= az) return v.x > 0 ? 5 : 4;
    if (ay >= az) return v.y > 0 ? 1 : 0;
    return v.z > 0 ? 3 : 2;
}

double runOcclusion(BlockSource& region, D3 soundPos, D3 playerPos) {
    auto const& cfg = config();
    double      acc = 0.0;
    D3          origin = soundPos;
    BlockPos    last   = floorPos(soundPos);
    for (int i = 0; i < cfg.maxOcclusionRays; ++i) {
        RayHit const hit = rayCast(region, origin, playerPos, &last);
        last             = hit.hit ? hit.pos : floorPos(playerPos);
        if (!hit.hit) break;
        origin = hit.location;
        Block const& block = region.getBlock(hit.pos);
        float        o     = acousticsOf(block).occlusion;
        D3 const dir = origin - D3{hit.pos.x + 0.5, hit.pos.y + 0.5, hit.pos.z + 0.5};
        if (!block.canProvideFullSupport(nearestFace(dir))) o *= static_cast<float>(cfg.nonFullBlockOcclusionFactor);
        acc += o;
        if (acc > cfg.maxOcclusion) break;
    }
    return acc;
}

double calculateOcclusion(BlockSource& region, EvalInput const& in) {
    auto const& cfg = config();
    if (cfg.strictOcclusion) return (std::min)(runOcclusion(region, in.soundPos, in.listenerPos), cfg.maxOcclusion);
    bool const isBlock = in.category == "block" || in.name.find("block") != std::string_view::npos;
    double     variation = cfg.occlusionVariation;
    if (isBlock) variation = (std::max)(variation, 0.49);
    double minAcc = runOcclusion(region, in.soundPos, in.listenerPos);
    if (variation > 0.0) {
        for (int x = -1; x <= 1; x += 2) {
            for (int y = -1; y <= 1; y += 2) {
                for (int z = -1; z <= 1; z += 2) {
                    D3 const offset = D3{double(x), double(y), double(z)} * variation;
                    minAcc = (std::min)(minAcc, runOcclusion(region, in.soundPos + offset, in.listenerPos + offset));
                }
            }
        }
    }
    return (std::min)(minAcc, cfg.maxOcclusion);
}

class ReflectedAudio {
public:
    explicit ReflectedAudio(double occlusion, std::string_view name) {
        auto const& cfg = config();
        mEvaluate = cfg.soundDirectionEvaluation && (occlusion > 0.0 || !cfg.redirectNonOccludedSounds)
                 && !name.starts_with("voicechat");
    }
    [[nodiscard]] int  shared() const { return mShared; }
    void addDirect(D3 v) { mDirect = v; }
    void addShared(D3 v, double totalRayDistance) {
        auto const&  cfg = config();
        double const l   = totalRayDistance + length(v);
        if (l <= 0.0 || l > cfg.maxSoundProcessingDistance / cfg.attenuationFactor) return;
        ++mShared;
        if (mEvaluate) mDirections.emplace_back(v, l);
    }
    [[nodiscard]] std::optional<D3> evaluate(D3 soundPos, D3 listenerPos) const {
        if (!mEvaluate || mDirections.empty()) return std::nullopt;
        D3 sum = mDirect ? normalize(*mDirect) : D3{};
        for (auto const& [v, l] : mDirections) {
            if (l <= 0.0) return std::nullopt;
            sum = sum + normalize(v) * (1.0 / (l * l));
        }
        D3 const n = normalize(sum);
        if (length(n) < 0.5) return std::nullopt;
        return n * distance(soundPos, listenerPos) + listenerPos;
    }

private:
    bool                             mEvaluate = false;
    int                              mShared   = 0;
    std::optional<D3>                mDirect;
    std::vector<std::pair<D3, double>> mDirections;
};

float clamp01(float v) { return std::clamp(v, 0.0F, 1.0F); }

}

EvalResult evaluateEnvironment(BlockSource& region, EvalInput const& in) {
    auto const& cfg = config();
    EvalResult  r;
    D3          soundPos  = in.soundPos;
    D3 const    playerPos = in.listenerPos;
    float const absorptionCoeff = static_cast<float>(cfg.blockAbsorption * 3.0);

    BlockPos const soundBlockPos = floorPos(soundPos);
    bool const     sourceUnderwater   = acousticsOf(region.getBlock(soundBlockPos)).water;
    bool const     listenerUnderwater = acousticsOf(region.getBlock(floorPos(playerPos))).water;
    bool const     underwater         = sourceUnderwater || listenerUnderwater;

    double const occlusion = calculateOcclusion(region, in);
    r.occlusion            = occlusion;
    float directCutoff     = static_cast<float>(std::exp(-occlusion * absorptionCoeff));
    if (underwater) directCutoff *= 1.0F - static_cast<float>(cfg.underwaterFilter);

    float const maxDistance = 256.0F;
    int const   numRays     = cfg.environmentEvaluationRayCount;
    int const   rayBounces  = cfg.environmentEvaluationRayBounces;

    ReflectedAudio     audio(occlusion, in.name);
    std::vector<float> ratio(static_cast<size_t>(rayBounces), 0.0F);
    float const        rcpTotalRays = 1.0F / static_cast<float>(numRays * rayBounces);
    float const        gAngle       = PHI * std::numbers::pi_v<float> * 2.0F;
    std::array<float, 4> send{0, 0, 0, 0};

    if (auto direct = sharedAirspace(region, soundPos, playerPos)) audio.addDirect(*direct);

    for (int i = 0; i < numRays; ++i) {
        float const fiN       = static_cast<float>(i) / static_cast<float>(numRays);
        float const longitude = gAngle * static_cast<float>(i) * 1.0F;
        float const latitude  = static_cast<float>(std::asin(fiN * 2.0F - 1.0F));
        D3 const    rayDir{
            std::cos(latitude) * std::cos(longitude),
            std::cos(latitude) * std::sin(longitude),
            std::sin(latitude)
        };
        RayHit const first = rayCast(region, soundPos, soundPos + rayDir * maxDistance, &soundBlockPos);
        if (!first.hit) continue;

        BlockPos lastHitBlock  = first.pos;
        D3       lastHitPos    = first.location;
        D3       lastHitNormal = first.normal;
        D3       lastRayDir    = rayDir;
        float    totalRayDistance = static_cast<float>(distance(soundPos, first.location));

        if (auto v = sharedAirspace(region, first.location + first.normal * 0.001, playerPos)) {
            audio.addShared(*v, totalRayDistance);
        }

        for (int j = 0; j < rayBounces; ++j) {
            D3 const     newRayDir = lastRayDir - lastHitNormal * (dot(lastRayDir, lastHitNormal) * 2.0);
            D3 const     newStart  = lastHitPos;
            RayHit const hit       = rayCast(region, newStart, newStart + newRayDir * maxDistance, &lastHitBlock);
            float const  reflectivity = acousticsOf(region.getBlock(lastHitBlock)).reflectivity;
            float const  energy       = 0.25F * (reflectivity * 0.75F + 0.25F);

            if (!hit.hit) {
                totalRayDistance += static_cast<float>(distance(lastHitPos, playerPos));
            } else {
                ratio[static_cast<size_t>(j)] += reflectivity;
                totalRayDistance += static_cast<float>(distance(lastHitPos, hit.location));
                lastHitPos    = hit.location;
                lastHitNormal = hit.normal;
                lastRayDir    = newRayDir;
                lastHitBlock  = hit.pos;
                if (auto v = sharedAirspace(region, hit.location + hit.normal * 0.001, playerPos)) {
                    audio.addShared(*v, totalRayDistance);
                }
            }
            if (totalRayDistance < cfg.reverbAttenuationDistance) continue;

            float const delay  = (std::max)(totalRayDistance, 0.0F) * 0.12F * reflectivity;
            float const cross0 = 1.0F - clamp01(std::abs(delay - 0.0F));
            float const cross1 = 1.0F - clamp01(std::abs(delay - 1.0F));
            float const cross2 = 1.0F - clamp01(std::abs(delay - 2.0F));
            float const cross3 = clamp01(delay - 2.0F);
            send[0] += cross0 * energy * 6.4F * rcpTotalRays;
            send[1] += cross1 * energy * 12.8F * rcpTotalRays;
            send[2] += cross2 * energy * 12.8F * rcpTotalRays;
            send[3] += cross3 * energy * 12.8F * rcpTotalRays;

            if (!hit.hit) break;
        }
    }

    for (auto& v : ratio) v /= static_cast<float>(numRays);

    if (auto newPos = audio.evaluate(soundPos, playerPos)) {
        r.hasNewPos = true;
        r.newPos    = *newPos;
        soundPos    = *newPos;
    }

    float const shared = static_cast<float>(audio.shared()) * 64.0F * rcpTotalRays;
    r.sharedAirspace   = audio.shared();
    std::array<float, 4> const weight{clamp01(shared / 20.0F), clamp01(shared / 15.0F), clamp01(shared / 10.0F), clamp01(shared / 10.0F)};

    float const occludedCutoff = static_cast<float>(std::exp(-occlusion * absorptionCoeff * 1.0));
    std::array<float, 4> cutoff{};
    for (size_t k = 0; k < 4; ++k) cutoff[k] = occludedCutoff * (1.0F - weight[k]) + weight[k];

    float const average = (weight[0] + weight[1] + weight[2] + weight[3]) * 0.25F;
    directCutoff        = (std::max)(static_cast<float>(std::pow(average, 0.5)) * 0.2F, directCutoff);
    r.directCutoff      = directCutoff;
    r.directGain        = static_cast<float>(std::pow(directCutoff, 0.1));

    send[1] *= ratio[1];
    if (ratio.size() > 2) send[2] *= static_cast<float>(std::pow(ratio[2], 3.0));
    if (ratio.size() > 3) send[3] *= static_cast<float>(std::pow(ratio[3], 4.0));

    send[0] = clamp01(send[0]);
    send[1] = clamp01(send[1]);
    send[2] = clamp01(send[2] * 1.05F - 0.05F);
    send[3] = clamp01(send[3] * 1.05F - 0.05F);
    for (size_t k = 0; k < 4; ++k) send[k] *= static_cast<float>(std::pow(cutoff[k], 0.1));

    float const soundDistance = static_cast<float>(distance(playerPos, soundPos));
    float const multiplier =
        1.0F - (std::min)(soundDistance / (in.maxDistance * static_cast<float>(cfg.reverbDistance)), 1.0F);
    for (auto& v : send) v *= multiplier;

    if (underwater) {
        for (auto& v : cutoff) v *= 0.4F;
    }
    r.sendGain   = send;
    r.sendCutoff = cutoff;
    return r;
}

}
