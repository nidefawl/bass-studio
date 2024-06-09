
#include <vector>
#include <array>

namespace DAW {
template<typename T, size_t SEGMENT_SIZE = 1024>
class SegmentedVector {
public:
    using Segment = std::array<T, SEGMENT_SIZE>;
    using SegmentIdx = size_t;
    constexpr SegmentIdx segmentIdx(size_t index) const {
        return index / SEGMENT_SIZE;
    }
    SegmentedVector()  = default;
    SegmentedVector(const SegmentedVector& graph) = delete;
    SegmentedVector& operator=(const SegmentedVector& graph) = delete;
    SegmentedVector(SegmentedVector&& graph) = delete;
    SegmentedVector& operator=(SegmentedVector&& graph) = delete;
    ~SegmentedVector() {
        for (auto* segment : m_segments) {
            delete segment;
        }
    }

    std::vector<Segment*> m_segments;
    size_t m_size = 0;

    size_t size() const {
        return m_size;
    }
    bool isValidIdx(size_t index) const {
        return index < m_size;
    }
    Segment& getSegment(SegmentIdx idx) {
        
        return *m_segments[idx];
    }
    const T& operator[](size_t index) const {
        return getSegment(segmentIdx(index))[index % SEGMENT_SIZE];
    }

    T& operator[](size_t index) {
        return getSegment(segmentIdx(index))[index % SEGMENT_SIZE];
    }

    T& push_back(const T& value) {
        const auto moduloSize = m_size % SEGMENT_SIZE;
        if (moduloSize == 0) {
            m_segments.push_back(new Segment());
        }
        auto& entry = (*m_segments.back())[moduloSize];
        entry = value;
        ++m_size;
        return entry;
    }
};
} // namespace DAW