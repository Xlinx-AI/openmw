#include "noise.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace CSMProcs
{
    // ========== RandomGenerator ==========
    
    RandomGenerator::RandomGenerator(uint64_t seed)
        : mSeed(seed)
        , mState(seed ^ 0x5DEECE66DULL)
    {
    }
    
    void RandomGenerator::setSeed(uint64_t seed)
    {
        mSeed = seed;
        mState = seed ^ 0x5DEECE66DULL;
    }
    
    uint64_t RandomGenerator::next()
    {
        // xorshift64* algorithm
        mState ^= mState >> 12;
        mState ^= mState << 25;
        mState ^= mState >> 27;
        return mState * 0x2545F4914F6CDD1DULL;
    }
    
    uint32_t RandomGenerator::nextInt(uint32_t max)
    {
        if (max == 0) return 0;
        return static_cast<uint32_t>(next() % max);
    }
    
    int RandomGenerator::nextIntRange(int min, int max)
    {
        if (min >= max) return min;
        return min + static_cast<int>(next() % static_cast<uint64_t>(max - min + 1));
    }
    
    float RandomGenerator::nextFloat()
    {
        return static_cast<float>(next() & 0xFFFFFFFFULL) / static_cast<float>(0x100000000ULL);
    }
    
    float RandomGenerator::nextFloatRange(float min, float max)
    {
        return min + nextFloat() * (max - min);
    }
    
    bool RandomGenerator::nextBool(float probability)
    {
        return nextFloat() < probability;
    }
    
    void RandomGenerator::nextPointInCircle(float& x, float& y)
    {
        // Rejection sampling for uniform distribution
        do {
            x = nextFloatRange(-1.0f, 1.0f);
            y = nextFloatRange(-1.0f, 1.0f);
        } while (x * x + y * y > 1.0f);
    }
    
    // ========== PerlinNoise ==========
    
    PerlinNoise::PerlinNoise(uint64_t seed)
    {
        setSeed(seed);
    }
    
    void PerlinNoise::setSeed(uint64_t seed)
    {
        RandomGenerator rng(seed);
        
        // Initialize with values 0-255
        std::array<int, 256> p;
        for (int i = 0; i < 256; ++i)
            p[i] = i;
        
        // Shuffle using Fisher-Yates
        for (int i = 255; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            std::swap(p[i], p[j]);
        }
        
        // Duplicate for overflow
        for (int i = 0; i < 256; ++i)
        {
            mPermutation[i] = p[i];
            mPermutation[i + 256] = p[i];
        }
    }
    
    float PerlinNoise::fade(float t)
    {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }
    
    float PerlinNoise::lerp(float t, float a, float b)
    {
        return a + t * (b - a);
    }
    
    float PerlinNoise::grad(int hash, float x, float y)
    {
        int h = hash & 7;
        float u = h < 4 ? x : y;
        float v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
    }
    
    float PerlinNoise::noise2D(float x, float y) const
    {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        
        x -= std::floor(x);
        y -= std::floor(y);
        
        float u = fade(x);
        float v = fade(y);
        
        int A = mPermutation[X] + Y;
        int AA = mPermutation[A];
        int AB = mPermutation[A + 1];
        int B = mPermutation[X + 1] + Y;
        int BA = mPermutation[B];
        int BB = mPermutation[B + 1];
        
        return lerp(v,
            lerp(u, grad(mPermutation[AA], x, y), grad(mPermutation[BA], x - 1, y)),
            lerp(u, grad(mPermutation[AB], x, y - 1), grad(mPermutation[BB], x - 1, y - 1)));
    }
    
    float PerlinNoise::fractalNoise2D(float x, float y, int octaves, float persistence, float lacunarity) const
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxValue = 0.0f;
        
        for (int i = 0; i < octaves; ++i)
        {
            total += noise2D(x * frequency, y * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }
        
        return total / maxValue;
    }
    
    float PerlinNoise::turbulence2D(float x, float y, int octaves, float persistence, float lacunarity) const
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxValue = 0.0f;
        
        for (int i = 0; i < octaves; ++i)
        {
            total += std::abs(noise2D(x * frequency, y * frequency)) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }
        
        return total / maxValue;
    }
    
    float PerlinNoise::ridgedNoise2D(float x, float y, int octaves, float persistence, float lacunarity, float offset) const
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float weight = 1.0f;
        
        for (int i = 0; i < octaves; ++i)
        {
            float signal = offset - std::abs(noise2D(x * frequency, y * frequency));
            signal *= signal;
            signal *= weight;
            weight = std::clamp(signal * 2.0f, 0.0f, 1.0f);
            total += signal * amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }
        
        return total;
    }
    
    // ========== VoronoiNoise ==========
    
    VoronoiNoise::VoronoiNoise(uint64_t seed)
        : mRng(seed)
    {
    }
    
    void VoronoiNoise::setSeed(uint64_t seed)
    {
        mRng.setSeed(seed);
    }
    
    void VoronoiNoise::getPointInCell(int cellX, int cellY, float& px, float& py) const
    {
        // Use deterministic hash to get point position
        uint64_t hash = static_cast<uint64_t>(cellX) * 73856093ULL ^ static_cast<uint64_t>(cellY) * 19349663ULL;
        RandomGenerator localRng(hash ^ mRng.getSeed());
        px = static_cast<float>(cellX) + localRng.nextFloat();
        py = static_cast<float>(cellY) + localRng.nextFloat();
    }
    
    float VoronoiNoise::distanceToNearest(float x, float y) const
    {
        int cellX = static_cast<int>(std::floor(x));
        int cellY = static_cast<int>(std::floor(y));
        
        float minDist = std::numeric_limits<float>::max();
        
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                float px, py;
                getPointInCell(cellX + dx, cellY + dy, px, py);
                float dist = (px - x) * (px - x) + (py - y) * (py - y);
                minDist = std::min(minDist, dist);
            }
        }
        
        return std::sqrt(minDist);
    }
    
    float VoronoiNoise::distanceToSecondNearest(float x, float y) const
    {
        int cellX = static_cast<int>(std::floor(x));
        int cellY = static_cast<int>(std::floor(y));
        
        float minDist1 = std::numeric_limits<float>::max();
        float minDist2 = std::numeric_limits<float>::max();
        
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                float px, py;
                getPointInCell(cellX + dx, cellY + dy, px, py);
                float dist = (px - x) * (px - x) + (py - y) * (py - y);
                
                if (dist < minDist1)
                {
                    minDist2 = minDist1;
                    minDist1 = dist;
                }
                else if (dist < minDist2)
                {
                    minDist2 = dist;
                }
            }
        }
        
        return std::sqrt(minDist2);
    }
    
    float VoronoiNoise::cellularNoise(float x, float y) const
    {
        return distanceToSecondNearest(x, y) - distanceToNearest(x, y);
    }
    
    // ========== PoissonDiskSampler ==========
    
    PoissonDiskSampler::PoissonDiskSampler(uint64_t seed, float minDistance, float width, float height)
        : mRng(seed)
        , mMinDistance(minDistance)
        , mWidth(width)
        , mHeight(height)
    {
        mCellSize = minDistance / std::sqrt(2.0f);
        mGridWidth = static_cast<int>(std::ceil(width / mCellSize));
        mGridHeight = static_cast<int>(std::ceil(height / mCellSize));
    }
    
    bool PoissonDiskSampler::isValidPoint(float x, float y,
                                          const std::vector<std::pair<float, float>>& points,
                                          const std::vector<std::vector<int>>& grid) const
    {
        if (x < 0 || x >= mWidth || y < 0 || y >= mHeight)
            return false;
            
        int cellX = static_cast<int>(x / mCellSize);
        int cellY = static_cast<int>(y / mCellSize);
        
        int searchStartX = std::max(0, cellX - 2);
        int searchEndX = std::min(mGridWidth - 1, cellX + 2);
        int searchStartY = std::max(0, cellY - 2);
        int searchEndY = std::min(mGridHeight - 1, cellY + 2);
        
        for (int dy = searchStartY; dy <= searchEndY; ++dy)
        {
            for (int dx = searchStartX; dx <= searchEndX; ++dx)
            {
                int idx = grid[dy][dx];
                if (idx >= 0)
                {
                    float ox = points[idx].first;
                    float oy = points[idx].second;
                    float dist = std::sqrt((x - ox) * (x - ox) + (y - oy) * (y - oy));
                    if (dist < mMinDistance)
                        return false;
                }
            }
        }
        
        return true;
    }
    
    std::vector<std::pair<float, float>> PoissonDiskSampler::generatePoints(int maxAttempts)
    {
        std::vector<std::pair<float, float>> points;
        std::vector<std::vector<int>> grid(mGridHeight, std::vector<int>(mGridWidth, -1));
        std::queue<int> activeList;
        
        // Add initial point
        float startX = mRng.nextFloat() * mWidth;
        float startY = mRng.nextFloat() * mHeight;
        points.emplace_back(startX, startY);
        
        int cellX = static_cast<int>(startX / mCellSize);
        int cellY = static_cast<int>(startY / mCellSize);
        grid[cellY][cellX] = 0;
        activeList.push(0);
        
        while (!activeList.empty())
        {
            int idx = activeList.front();
            activeList.pop();
            
            float px = points[idx].first;
            float py = points[idx].second;
            
            for (int attempt = 0; attempt < maxAttempts; ++attempt)
            {
                float angle = mRng.nextFloat() * 2.0f * 3.14159265f;
                float dist = mMinDistance + mRng.nextFloat() * mMinDistance;
                
                float newX = px + std::cos(angle) * dist;
                float newY = py + std::sin(angle) * dist;
                
                if (isValidPoint(newX, newY, points, grid))
                {
                    int newIdx = static_cast<int>(points.size());
                    points.emplace_back(newX, newY);
                    
                    int newCellX = static_cast<int>(newX / mCellSize);
                    int newCellY = static_cast<int>(newY / mCellSize);
                    grid[newCellY][newCellX] = newIdx;
                    activeList.push(newIdx);
                }
            }
        }
        
        return points;
    }
    
    std::vector<std::pair<float, float>> PoissonDiskSampler::generatePointsWithMask(
        const std::vector<float>& densityMask,
        int maskWidth, int maskHeight,
        int maxAttempts)
    {
        std::vector<std::pair<float, float>> points;
        std::vector<std::vector<int>> grid(mGridHeight, std::vector<int>(mGridWidth, -1));
        std::queue<int> activeList;
        
        // Find starting point with non-zero density
        float startX, startY;
        float density = 0.0f;
        int attempts = 0;
        do {
            startX = mRng.nextFloat() * mWidth;
            startY = mRng.nextFloat() * mHeight;
            
            int maskX = static_cast<int>(startX / mWidth * maskWidth);
            int maskY = static_cast<int>(startY / mHeight * maskHeight);
            maskX = std::clamp(maskX, 0, maskWidth - 1);
            maskY = std::clamp(maskY, 0, maskHeight - 1);
            density = densityMask[maskY * maskWidth + maskX];
            ++attempts;
        } while (density < 0.1f && attempts < 1000);
        
        if (density < 0.1f)
            return points;
            
        points.emplace_back(startX, startY);
        int cellX = static_cast<int>(startX / mCellSize);
        int cellY = static_cast<int>(startY / mCellSize);
        grid[cellY][cellX] = 0;
        activeList.push(0);
        
        while (!activeList.empty())
        {
            int idx = activeList.front();
            activeList.pop();
            
            float px = points[idx].first;
            float py = points[idx].second;
            
            for (int attempt = 0; attempt < maxAttempts; ++attempt)
            {
                float angle = mRng.nextFloat() * 2.0f * 3.14159265f;
                
                // Get local density
                int maskX = static_cast<int>(px / mWidth * maskWidth);
                int maskY = static_cast<int>(py / mHeight * maskHeight);
                maskX = std::clamp(maskX, 0, maskWidth - 1);
                maskY = std::clamp(maskY, 0, maskHeight - 1);
                float localDensity = densityMask[maskY * maskWidth + maskX];
                
                if (localDensity < 0.01f)
                    continue;
                    
                float adjustedMinDist = mMinDistance / std::sqrt(localDensity);
                float dist = adjustedMinDist + mRng.nextFloat() * adjustedMinDist;
                
                float newX = px + std::cos(angle) * dist;
                float newY = py + std::sin(angle) * dist;
                
                // Check density at new position
                int newMaskX = static_cast<int>(newX / mWidth * maskWidth);
                int newMaskY = static_cast<int>(newY / mHeight * maskHeight);
                if (newMaskX >= 0 && newMaskX < maskWidth && newMaskY >= 0 && newMaskY < maskHeight)
                {
                    float newDensity = densityMask[newMaskY * maskWidth + newMaskX];
                    if (newDensity > mRng.nextFloat() && isValidPoint(newX, newY, points, grid))
                    {
                        int newIdx = static_cast<int>(points.size());
                        points.emplace_back(newX, newY);
                        
                        int newCellX = static_cast<int>(newX / mCellSize);
                        int newCellY = static_cast<int>(newY / mCellSize);
                        grid[newCellY][newCellX] = newIdx;
                        activeList.push(newIdx);
                    }
                }
            }
        }
        
        return points;
    }
}
