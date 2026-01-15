#ifndef CSM_PROCS_NOISE_H
#define CSM_PROCS_NOISE_H

#include <array>
#include <cstdint>
#include <vector>

namespace CSMProcs
{
    /// Pseudo-random number generator with deterministic seeding
    class RandomGenerator
    {
    public:
        explicit RandomGenerator(uint64_t seed = 0);
        
        void setSeed(uint64_t seed);
        uint64_t getSeed() const { return mSeed; }
        
        /// Generate random integer in [0, max)
        uint32_t nextInt(uint32_t max);
        
        /// Generate random integer in [min, max]
        int nextIntRange(int min, int max);
        
        /// Generate random float in [0, 1)
        float nextFloat();
        
        /// Generate random float in [min, max]
        float nextFloatRange(float min, float max);
        
        /// Generate random boolean with given probability of true
        bool nextBool(float probability = 0.5f);
        
        /// Generate random point within unit circle
        void nextPointInCircle(float& x, float& y);
        
    private:
        uint64_t mSeed;
        uint64_t mState;
        
        uint64_t next();
    };

    /// Perlin noise implementation for terrain generation
    class PerlinNoise
    {
    public:
        explicit PerlinNoise(uint64_t seed = 0);
        
        void setSeed(uint64_t seed);
        
        /// Get 2D noise value at position in range [-1, 1]
        float noise2D(float x, float y) const;
        
        /// Get fractal/octave noise
        float fractalNoise2D(float x, float y, int octaves, float persistence, float lacunarity) const;
        
        /// Get turbulence noise (absolute value)
        float turbulence2D(float x, float y, int octaves, float persistence, float lacunarity) const;
        
        /// Get ridged multifractal noise (good for mountains)
        float ridgedNoise2D(float x, float y, int octaves, float persistence, float lacunarity, float offset) const;
        
    private:
        std::array<int, 512> mPermutation;
        
        static float fade(float t);
        static float lerp(float t, float a, float b);
        static float grad(int hash, float x, float y);
    };

    /// Voronoi/Worley noise for cellular patterns
    class VoronoiNoise
    {
    public:
        explicit VoronoiNoise(uint64_t seed = 0);
        
        void setSeed(uint64_t seed);
        
        /// Get distance to nearest point
        float distanceToNearest(float x, float y) const;
        
        /// Get distance to second nearest point
        float distanceToSecondNearest(float x, float y) const;
        
        /// Get cellular noise (F2 - F1)
        float cellularNoise(float x, float y) const;
        
    private:
        RandomGenerator mRng;
        
        void getPointInCell(int cellX, int cellY, float& px, float& py) const;
    };

    /// Poisson disk sampling for object placement
    class PoissonDiskSampler
    {
    public:
        PoissonDiskSampler(uint64_t seed, float minDistance, float width, float height);
        
        /// Generate points with minimum spacing
        std::vector<std::pair<float, float>> generatePoints(int maxAttempts = 30);
        
        /// Generate points with variable density based on mask
        std::vector<std::pair<float, float>> generatePointsWithMask(
            const std::vector<float>& densityMask,
            int maskWidth, int maskHeight,
            int maxAttempts = 30);
        
    private:
        RandomGenerator mRng;
        float mMinDistance;
        float mWidth;
        float mHeight;
        float mCellSize;
        int mGridWidth;
        int mGridHeight;
        
        bool isValidPoint(float x, float y, 
                         const std::vector<std::pair<float, float>>& points,
                         const std::vector<std::vector<int>>& grid) const;
    };
}

#endif
